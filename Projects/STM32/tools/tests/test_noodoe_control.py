"""Exercise the public NDCP client with fragmented, corrupt and stale replies.

The fake transport never opens serial hardware. Backup authorization calls the
real verifier using small files, with only its capacity fixture reduced.
"""
import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import noodoe_control as nc
import storage_backup as sb


class Serial:
    def __init__(self, mutate=None):
        self.pending = bytearray()
        self.requests = []
        self.mutate = mutate
        self.uid = (0x12345678, 0x90ABCDEF, 0x1234)

    def write(self, packet):
        _, _, op, flags, seq, length, reserved = nc.HEADER.unpack(packet[:16])
        assert flags == reserved == 0 and len(packet) == length + 20
        assert nc.encode(op, seq, packet[16:-4]) == packet
        self.requests.append((op, packet[16:-4]))
        result = b"\0" * 4
        if op == 0:
            result += packet[16:-4]
        elif op == 1:
            result += struct.pack("<9I", 1, *self.uid, 0xE0000, 0, 0, 0, 0)
            result += b"test-build".ljust(32, b"\0")
        elif op != 0x1F:
            raise AssertionError("Unrequested operation")
        frame = nc.encode(op, seq, result, 1)
        self.pending.extend(self.mutate(frame) if self.mutate else frame)
        return len(packet)

    def read(self, count):
        count = min(count, 3, len(self.pending))
        result = bytes(self.pending[:count])
        del self.pending[:count]
        return result


class ControlClientTest(unittest.TestCase):
    def test_fragmented_echo_wrap_and_preamble(self):
        port = Serial(lambda frame: b"garbageNDC" + frame)
        client = nc.Client(port, 0.1)
        client.sequence = 0xFFFFFFFF
        self.assertEqual(client.request(0, b"abc"), b"abc")
        self.assertEqual(client.sequence, 0)

    def test_stale_sequence_is_not_current_reply(self):
        def stale(frame):
            _, _, op, _, seq, _, _ = nc.HEADER.unpack(frame[:16])
            return nc.encode(op, (seq - 1) & 0xFFFFFFFF, b"\0" * 4 + b"stale", 1) + frame
        self.assertEqual(nc.Client(Serial(stale)).request(0, b"current"), b"current")

    def test_crc_rejected(self):
        port = Serial(lambda frame: frame[:-1] + bytes([frame[-1] ^ 1]))
        with self.assertRaisesRegex(nc.ControlError, "CRC"):
            nc.Client(port).request(0)

    def test_invalid_header_and_missing_response_flag(self):
        for mutation in (lambda f: f[:4] + b"\2" + f[5:],
                         lambda f: nc.encode(f[5], struct.unpack_from("<I", f, 8)[0], f[16:-4], 0)):
            with self.subTest(mutation=mutation), self.assertRaises(nc.ControlError):
                nc.Client(Serial(mutation)).request(0)

    def test_timeout_and_negative_result(self):
        with self.assertRaisesRegex(nc.ControlError, "timed out"):
            nc.Client(Serial(lambda _: b""), 0.001).request(0)
        error = lambda f: nc.encode(f[5], struct.unpack_from("<I", f, 8)[0], struct.pack("<i", -4), 3)
        with self.assertRaisesRegex(nc.ControlError, "result=-4"):
            nc.Client(Serial(error)).request(0)

    def test_info_identity_and_build(self):
        info = nc.Client(Serial()).info()
        self.assertEqual(info["usb_serial"], "1234567890ABCDEF00001234")
        self.assertEqual(info["boot_metadata_words"], [0xE0000, 0, 0, 0, 0])
        self.assertEqual(info["build"], "test-build")

    def test_authorization_requires_fresh_uid_bound_proof(self):
        port = Serial()
        client = nc.Client(port)
        identity = client.info()
        data = b"full fixture"
        sha = hashlib.sha256(data).hexdigest()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            captures = []
            for index, name in enumerate(("A.bin", "B.bin")):
                file = root / name
                file.write_bytes(data)
                captures.append({"path": str(file), "address": 0, "length": len(data),
                                 "sequence": index + 1, "sha256": sha,
                                 "terminal_done_validated": True})
            manifest = root / "manifest.json"
            manifest.write_text(json.dumps({"state": "verified", "verified": True,
                                           "identity": identity, "captures": captures}))
            original = sb.compare_files
            with patch.object(sb, "CAPACITY", len(data)), patch.object(sb, "compare_files", lambda a, b: original(a, b, len(data))):
                self.assertTrue(client.authorize_update(manifest)["authorized"])
                self.assertEqual(port.requests[-1], (0x1F, struct.pack("<4I", *port.uid, 0x42414B32)))
                previous = sum(op == 0x1F for op, _ in port.requests)
                port.uid = (1, 2, 3)
                with self.assertRaisesRegex(ValueError, "UID"):
                    client.authorize_update(manifest)
                port.uid = tuple(identity["uid_words"])
                for name in ("A.bin", "B.bin"):
                    (root / name).write_bytes(b"X" * len(data))
                with self.assertRaisesRegex(ValueError, "validation"):
                    client.authorize_update(manifest)
                self.assertEqual(sum(op == 0x1F for op, _ in port.requests), previous)

    def test_snapshot_signed_fields_and_raw_boundaries(self):
        vehicle = [0] * 19
        vehicle[12], vehicle[18] = 0xFFFFFFFE, 2
        result = nc.decode_snapshot(0x0A, struct.pack("<19I", *vehicle) + b"\x55\xaa")
        self.assertEqual((result["temperature_candidate_c"], result["raw_hex"]), (-2, "55aa"))
        gnss = [0] * 27
        gnss[16], gnss[22], gnss[26] = (-375000000) & 0xFFFFFFFF, 0xFFFFFC18, 1
        result = nc.decode_snapshot(0x0B, struct.pack("<27I", *gnss) + b"$")
        self.assertEqual((result["latitude_e7"], result["altitude_mm"], result["raw"]), (-375000000, -1000, "$"))
        obd = [0] * 40
        obd[15], obd[39] = 0xFFFFFFFF, 3
        result = nc.decode_snapshot(0x0C, struct.pack("<40I", *obd) + b"010D".ljust(16, b"\0") + b"OK>")
        self.assertEqual((result["values"][0], result["command"], result["raw"]), (-1, "010D", "OK>"))
        for op, payload in ((0x0A, struct.pack("<19I", *vehicle)),
                            (0x0B, struct.pack("<27I", *gnss)),
                            (0x0C, struct.pack("<40I", *obd))):
            with self.subTest(op=op), self.assertRaises(nc.ControlError):
                nc.decode_snapshot(op, payload)

    def test_bt_roles_use_fixed_wire_layout(self):
        payload = bytearray(208)
        struct.pack_into("<I", payload, 4, 1)
        payload[68:74] = nc.address("00:11:22:33:44:55")
        for index in range(3):
            offset = 76 + 44 * index
            struct.pack_into("<HH8I", payload, offset + 8, index + 10, 512, *range(index, index + 8))
        result = nc.decode_bt(bytes(payload))
        self.assertEqual(result["local_address"], "00:11:22:33:44:55")
        self.assertEqual(result["links"]["gps"]["cid"], 12)
        self.assertEqual(result["links"]["elm"]["reconnects"], 5)
        struct.pack_into("<I", payload, 4, 2)
        result = nc.decode_bt(bytes(payload))
        self.assertEqual(result["links"]["phone2"]["cid"], 12)
        self.assertNotIn("gps", result["links"])
        struct.pack_into("<I", payload, 4, 3)
        with self.assertRaises(nc.ControlError):
            nc.decode_bt(bytes(payload))
        with self.assertRaises(nc.ControlError):
            nc.decode_bt(bytes(payload[:-1]))


if __name__ == "__main__":
    unittest.main()
