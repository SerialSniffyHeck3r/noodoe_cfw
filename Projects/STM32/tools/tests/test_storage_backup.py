"""Protocol failures must preserve partial evidence without accepting a backup."""
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest
import zlib
import json
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("storage_backup", Path(__file__).resolve().parents[1] / "storage_backup.py")
sb = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sb)


class FakeSerial:
    def __init__(self, mutate=None):
        self.incoming = bytearray()
        self.mutate = mutate
        self.requests = []

    def reset_input_buffer(self):
        self.incoming.clear()

    def response(self, opcode, sequence, address, payload=b"", status=0):
        frame = sb.RESPONSE.pack(b"NDRS", 1, opcode, status, sequence, address,
                                 len(payload), zlib.crc32(payload)) + payload
        self.incoming.extend(self.mutate(frame) if self.mutate else frame)

    def write(self, data):
        magic, version, op, flags, seq, address, length = sb.REQUEST.unpack(data)
        assert (magic, version, flags) == (b"NDRQ", 1, 0)
        self.requests.append(op)
        if op == 3:
            self.response(3, seq, 0)
        elif op == 1:
            self.response(1, seq, 0, sb.HELLO.pack(0xC2201B, sb.CAPACITY, 42000000, 0, 4096, 1,
                                                  0x07F70000, 0x07F70000, 65536, 0x07F80000, 0x07F90000, 0x70000))
        elif op == 4:
            self.response(4, seq, 0, struct.pack("<8I", 0x12345678, 0x90ABCDEF, 0x1234, 0xE0000, 0, 0, 0, 0))
        elif op in (0x10, 0x11, 0x12):
            assert (address, length) == {0x10:(0x07F70000, 0x42414B32),0x11:(0,0),0x12:(0x07F70000,0x464D5431)}[op]
            self.response(op, seq, address, b"" if op == 0x11 else struct.pack("<I", 0))
        elif op == 2:
            end = address + length
            while address < end:
                size = min(4096, end - address)
                self.response(2, seq, address, bytes(((address+i)*7)&255 for i in range(size)))
                address += size
            self.response(0x82, seq, end)
        else:
            raise AssertionError("Unexpected or destructive host command")
        return len(data)

    def read(self, amount):
        # Exercise both USB packet and header splits, including odd sizes.
        count = min(amount, 7, len(self.incoming))
        result = bytes(self.incoming[:count])
        del self.incoming[:count]
        return result


class BackupTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def test_two_fresh_reads_and_hash(self):
        port = FakeSerial()
        client = sb.Client(port, 0.01)
        client.synchronize()
        self.assertEqual(client.hello()["jedec_id"], 0xC2201B)
        a = client.capture(self.root/"A.bin", 0x00FFFFE0, 9001, progress=False)
        b = client.capture(self.root/"B.bin", 0x00FFFFE0, 9001, progress=False)
        self.assertNotEqual(a["sequence"], b["sequence"])
        self.assertEqual(a["data_frames"], 3)
        self.assertTrue(a["terminal_done_validated"])
        self.assertEqual(a["sha256"], b["sha256"])
        result = sb.compare_files(self.root/"A.bin", self.root/"B.bin", 9001)
        self.assertTrue(result["byte_identical"])
        self.assertEqual(result["sha256_a"], a["sha256"])
        self.assertEqual(port.requests, [3, 1, 2, 2])

    def fail_capture(self, mutation, text):
        client = sb.Client(FakeSerial(mutation), 0.001)
        with self.assertRaisesRegex(sb.ProtocolError, text):
            client.capture(self.root/"bad.bin", 0, 5000, progress=False)
        self.assertFalse((self.root/"bad.bin").exists())
        self.assertTrue((self.root/"bad.bin.partial").exists())

    def test_bad_crc(self):
        self.fail_capture(lambda f: f[:-1]+bytes([f[-1]^1]) if f[5]==2 else f, "CRC")

    def test_wrong_sequence(self):
        self.fail_capture(lambda f: f[:8]+struct.pack("<I", 0)+f[12:], "mismatch")

    def test_wrong_offset(self):
        self.fail_capture(lambda f: f[:12]+struct.pack("<I", 42)+f[16:], "mismatch")

    def test_missing_done(self):
        self.fail_capture(lambda f: b"" if f[5]==0x82 else f, "Timed out")

    def test_premature_done(self):
        self.fail_capture(lambda f: sb.RESPONSE.pack(b"NDRS", 1, 0x82, 0,
                          sb.RESPONSE.unpack(f[:24])[4], 0, 0, 0) if f[5]==2 else f, "Premature")

    def test_existing_files_never_overwritten(self):
        output = self.root/"keep.bin"
        output.write_bytes(b"original")
        with self.assertRaises(FileExistsError):
            sb.Client(FakeSerial()).capture(output, 0, 1, progress=False)
        self.assertEqual(output.read_bytes(), b"original")

    def test_offline_mismatch_location_and_alias(self):
        a, b = self.root/"a", self.root/"b"
        a.write_bytes(b"abcd")
        b.write_bytes(b"abCd")
        result = sb.compare_files(a, b, 4)
        self.assertEqual((result["differing_bytes"], result["first_difference"]), (1, 2))
        with self.assertRaises(ValueError):
            sb.compare_files(a, a, 4)
        with self.assertRaises(ValueError):
            sb.compare_files(a, b, 128)

    def test_identity_and_explicit_control_allowlist(self):
        client = sb.Client(FakeSerial())
        identity = client.identity()
        self.assertEqual(identity["usb_serial"], "1234567890ABCDEF00001234")
        self.assertEqual(identity["boot_metadata_words"], [0xE0000, 0, 0, 0, 0])
        for op in (0x10, 0x11, 0x12):
            client.storage_control(op)
        with self.assertRaises(ValueError):
            client.storage_control(0x20)

    def test_unlock_proof_uid_pending_and_changed_file(self):
        client = sb.Client(FakeSerial())
        identity = client.identity()
        captures = [client.capture(self.root/name, 0, 4, progress=False) for name in ("A.bin", "B.bin")]
        manifest = {"verified": True, "state": "verified", "identity": identity, "captures": captures}
        path = self.root/"manifest.json"
        path.write_text(json.dumps(manifest))
        original_compare = sb.compare_files
        # Keep this fixture small while still exercising fresh disk reads/hashes.
        with patch.object(sb, "CAPACITY", 4), patch.object(sb, "compare_files", lambda a,b: original_compare(a,b,4)):
            self.assertTrue(sb.verify_unlock_manifest(path, identity)["byte_identical"])
            wrong_uid = dict(identity, usb_serial="OTHER")
            with self.assertRaisesRegex(ValueError, "UID"):
                sb.verify_unlock_manifest(path, wrong_uid)
            pending = dict(identity, boot_metadata_words=[0xE0000,0,0x7F90,0x60000,0x1234])
            with self.assertRaisesRegex(ValueError, "pending"):
                sb.verify_unlock_manifest(path, pending)
            # Same lengths and matching files are insufficient if their hashes
            # differ from the original streaming proof in the manifest.
            (self.root/"A.bin").write_bytes(b"xxxx")
            (self.root/"B.bin").write_bytes(b"xxxx")
            with self.assertRaisesRegex(ValueError, "validation"):
                sb.verify_unlock_manifest(path, identity)


if __name__ == "__main__":
    unittest.main()
