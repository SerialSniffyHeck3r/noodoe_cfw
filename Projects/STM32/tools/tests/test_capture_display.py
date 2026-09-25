"""Mock the inherited command runner, never attach to ST-LINK hardware."""
from pathlib import Path
import contextlib
import io
import json
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import bringup
import capture_display as capture


class CaptureTransportTest(unittest.TestCase):
    def test_layout2_is_accepted_only_at_product_base(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);path=root/'manifest.json'
            data=dict(status='valid',layout_version=2,flash_address='0x08020000',symbols={
                'g_bsp_capture':dict(size=capture.MAILBOX_SIZE,address='0x20001000')})
            args=['--manifest',str(path),'--output',str(root/'capture')]
            path.write_text(json.dumps(data))
            with contextlib.redirect_stdout(io.StringIO()):self.assertEqual(capture.main(args),0)
            data['flash_address']='0x08010000';path.write_text(json.dumps(data))
            with self.assertRaisesRegex(RuntimeError,'validated'):capture.main(args)

    def test_page_selector_adds_only_one_exact_read_address(self):
        with tempfile.TemporaryDirectory() as directory:
            board = capture.LiveBoard("probe", Path(directory), 400, 0x20001000, 0x20002080)
            commands = []
            def runner(command, log, timeout):
                commands.append(command)
                index = command.index("-u")
                Path(command[index + 3]).write_bytes(struct.pack("<I", 2))
                return "ok"
            with patch.object(bringup, "run", runner):
                self.assertEqual(board.dump("page", 0x20002080, 4), struct.pack("<I", 2))
                for address, size in ((0x2000207C, 4), (0x20002084, 4), (0x20002080, 8)):
                    with self.subTest(address=address, size=size), self.assertRaisesRegex(RuntimeError, "outside"):
                        board.dump("forbidden", address, size)
            self.assertEqual(len(commands), 1)

    def test_liveboard_initializes_inherited_read_clock_and_never_halts(self):
        with tempfile.TemporaryDirectory() as directory:
            board = capture.LiveBoard("probe", Path(directory), 400, 0x20001000)
            commands = []
            def runner(command, log, timeout):
                commands.append(command)
                if "-u" in command:
                    index = command.index("-u")
                    count = int(command[index + 2], 0)
                    Path(command[index + 3]).write_bytes(b"\0" * count)
                return "ok"
            with patch.object(bringup, "run", runner):
                self.assertEqual(board.dump("mailbox", board.mailbox, 80), b"\0" * 80)
                self.assertEqual(board.assert_running("dhcsr"), 0)
                mailbox = capture.Mailbox(board, board.mailbox)
                sequence = mailbox.request(1)
            self.assertTrue(all("freq=400" in command for command in commands))
            self.assertTrue(all("mode=HOTPLUG" in command for command in commands))
            self.assertFalse(any(op in command for command in commands for op in ("-halt", "-run", "-rst", "-w")))
            self.assertEqual(commands[-1][-3:], ["-w32", hex(board.mailbox + 8), hex(sequence)])

    def test_halted_cpu_is_reported_without_resume_and_other_registers_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            board = capture.LiveBoard("probe", Path(directory), 100, 0x20001000)
            def runner(command, log, timeout):
                index = command.index("-u")
                Path(command[index + 3]).write_bytes(struct.pack("<I", 1 << 17))
                return "ok"
            with patch.object(bringup, "run", runner) as mocked:
                with self.assertRaisesRegex(RuntimeError, "CPU is halted"):
                    board.assert_running("halted")
                with self.assertRaisesRegex(RuntimeError, "outside"):
                    board.dump("other-register", 0xE000EDF4, 4)

    def test_page_argument_and_manifest_address_bounds_before_hardware(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            manifest = root / "manifest.json"
            data = {"status": "valid", "flash_address": hex(capture.APP), "symbols": {
                "g_bsp_capture": {"size": capture.MAILBOX_SIZE, "address": "0x20001000"},
                "g_graphics_bringup_page": {"size": 4, "address": "0x40000000"}}}
            manifest.write_text(json.dumps(data))
            arguments = ["--manifest", str(manifest), "--output", str(root / "capture")]
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                capture.main(arguments + ["--page", "4"])
            with self.assertRaisesRegex(RuntimeError, "selector address"):
                capture.main(arguments + ["--page", "1"])
            data["symbols"]["g_graphics_bringup_page"]["address"] = "0x20002001"
            manifest.write_text(json.dumps(data))
            with self.assertRaisesRegex(RuntimeError, "selector address"):
                capture.main(arguments + ["--page", "1"])

    def test_selected_page_restores_before_export_and_on_snapshot_error(self):
        # Complete control flow is exercised with no transport/device. A small
        # fake image avoids113 repetitive chunks; raster correctness is tested
        # separately in display_capture_host and actual hardware screenshots.
        for fail_snapshot in (False, True):
            with self.subTest(fail_snapshot=fail_snapshot), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                manifest = root / "manifest.json"
                manifest.write_text(json.dumps({"status": "valid", "flash_address": hex(capture.APP),
                    "binary_size": 4, "binary_sha256": capture.sha(b"APP!"), "symbols": {
                    "g_bsp_capture": {"size": capture.MAILBOX_SIZE, "address": "0x20001000"},
                    "g_graphics_bringup_page": {"size": 4, "address": "0x20003000"}}}))
                (root / "app.bin").write_bytes(b"APP!")
                events = []
                header = dict.fromkeys(capture.FIELDS, 0)
                header.update(magic=0x43415031, version=1, width=480, height=480, format=7, total_bytes=8)
                class FakeBoard:
                    def __init__(self, *args): pass
                    def prepare(self): pass
                    def assert_running(self, name): return 0
                    def command(self, name, *args): events.append(name)
                    def dump(self, name, address, size, resume=True):
                        if name == "app-identity": return b"APP!"
                        if name == "page-before": return struct.pack("<I", 1)
                        return capture.HEADER.pack(*(header[key] for key in capture.FIELDS))
                class FakeMailbox:
                    def __init__(self, *args): pass
                    def request(self, kind, generation=0, offset=0, length=0):
                        events.append(f"request-{kind}")
                        self.kind, self.generation, self.offset, self.length = kind, generation, offset, length
                        return kind
                    def response(self, sequence, with_payload=False):
                        if sequence == 1 and fail_snapshot: raise RuntimeError("snapshot failure")
                        return {"generation": 1, "expected_generation": self.generation,
                                "offset": self.offset, "payload_crc32": 0}, b"\0" * self.length
                output = root / "capture"
                args = ["--manifest", str(manifest), "--output", str(output), "--page", "2", "--execute"]
                with patch.object(capture, "LiveBoard", FakeBoard), patch.object(capture, "Mailbox", FakeMailbox), patch.object(capture, "TOTAL", 8), patch.object(capture.time, "sleep"), patch.object(capture, "decode_rgb565", lambda raw, path: path.write_bytes(b"fixture")), contextlib.redirect_stdout(io.StringIO()):
                    if fail_snapshot:
                        with self.assertRaisesRegex(RuntimeError, "snapshot failure"):
                            capture.main(args)
                        self.assertIn("restore-page-after-error", events)
                        self.assertNotIn("request-2", events)
                    else:
                        self.assertEqual(capture.main(args), 0)
                        self.assertLess(events.index("restore-page"), events.index("request-2"))
                self.assertTrue((output / "capture.json").exists())


if __name__ == "__main__":
    unittest.main()
