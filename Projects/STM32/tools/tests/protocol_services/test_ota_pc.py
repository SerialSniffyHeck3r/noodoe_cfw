"""PC NDCP/APP preparation tests; no Bluetooth API is imported or contacted."""
import importlib.util
import struct
import tempfile
import unittest
import json
import sys
import hashlib
from types import SimpleNamespace
from unittest.mock import patch
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('noodoe_ota', TOOLS / 'noodoe_ota.py')
ota = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ota)


class ProtocolTests(unittest.TestCase):
    def test_fragment_boundaries(self):
        payload = bytes(range(256)) * 4
        frame = ota.encode(0x41, 0x12345678, payload)
        self.assertEqual(len(frame), 1044)
        for split in range(len(frame) + 1):
            decoder = ota.Decoder()
            received = decoder.feed(frame[:split]) + decoder.feed(frame[split:])
            self.assertEqual(len(received), 1)
            self.assertEqual(received[0]['payload'], payload)
            self.assertEqual(received[0]['sequence'], 0x12345678)

    def test_corruption_resync(self):
        frame = ota.encode(0x45, 9)
        corrupt = bytearray(frame)
        corrupt[-1] ^= 1
        decoder = ota.Decoder()
        self.assertEqual(len(decoder.feed(b'noise' + corrupt + frame)), 1)
        self.assertEqual(decoder.errors, 1)
        self.assertLessEqual(len(decoder.data), 1044)

    def test_prepare_and_vector_guards(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'app.bin'
            data = bytearray(512)
            data[:8] = struct.pack('<II', 0x20020000, 0x08010101)
            path.write_bytes(data)
            padded, info = ota.load_image(path)
            self.assertEqual(len(padded), 0x70000)
            self.assertEqual(padded[512:], b'\xFF' * (0x70000 - 512))
            self.assertEqual(info['msp'], 0x20020000)
            for msp, reset in ((0x10000000, 0x08010101), (0x20020001, 0x08010101),
                               (0x20020000, 0x08000101), (0x20020000, 0x08010100)):
                data[:8] = struct.pack('<II', msp, reset)
                path.write_bytes(data)
                with self.assertRaises(ValueError):
                    ota.load_image(path)
            path.write_bytes(b'\xFF' * 0x80000)
            with self.assertRaises(ValueError):
                ota.load_image(path)

    def test_small_backup_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            a, b = Path(directory) / 'a.bin', Path(directory) / 'b.bin'
            a.write_bytes(b'x');b.write_bytes(b'x')
            with self.assertRaises(ValueError):
                ota.backup_evidence(a, b)

    def test_response_fragmentation(self):
        class Transport:
            def sendall(self, data):
                request = ota.Decoder().feed(data)[0]
                payload = struct.pack('<5I', 0, 3, 7, 0x70000, 0x70000)
                self.pending = bytearray(ota.encode(request['opcode'], request['sequence'], payload, 1))

            def recv(self, limit):
                result = bytes(self.pending[:3]);del self.pending[:3]
                return result

        result = ota.Connection(Transport()).request(0x45)
        self.assertEqual(result['state'], 3)
        self.assertEqual(result['transaction'], 7)

    def test_reset_keeps_connection_during_grace(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'transaction.json'
            journal = dict(address='AA:BB:CC:DD:EE:FF', transaction=7, image_version=1,
                           state='committed_pending_next_boot', image=dict(sha256='ab' * 32, crc32_iso=99))
            path.write_text(json.dumps(journal), encoding='utf-8')
            args = SimpleNamespace(journal=path, address=journal['address'], action='reset', allow_reset=True)
            current = dict(transaction=7, sha256='ab' * 32, state=4)
            class Connection:
                def request(self, opcode, payload):
                    self.opcode, self.payload = opcode, payload
                    return dict(state=5, extra=b'', transaction=7)
            connection = Connection()
            with patch.object(ota, 'status', return_value=current), patch.object(ota.time, 'sleep') as sleep:
                result = ota.install_step(connection, args)
                sleep.assert_called_once_with(2.0)
            self.assertEqual(connection.opcode, ota.RESET_OP)
            self.assertEqual(result['state'], 'reset_ack_grace_elapsed_installation_not_verified')

    def test_lost_commit_ack_reconcile_read_only(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'transaction.json'
            journal = dict(address='AA:BB:CC:DD:EE:FF', transaction=7, image_version=1,
                           state='commit_intent_result_unknown', image=dict(sha256='ab' * 32, crc32_iso=99))
            path.write_text(json.dumps(journal), encoding='utf-8')
            args = SimpleNamespace(journal=path, address=journal['address'])
            current = dict(transaction=7, sha256='ab' * 32, state=4, crc32_iso=99,
                           received=ota.APP_BYTES, verified=ota.APP_BYTES,
                           metadata=[0xE0000, 1, 0x7F90, ota.APP_BYTES, 99])
            with patch.object(ota, 'status', return_value=current):
                result = ota.reconcile(object(), args)
            self.assertEqual(result['state'], 'committed_pending_next_boot')
            current['metadata'][-1] = 100
            with patch.object(ota, 'status', return_value=current), self.assertRaises(RuntimeError):
                ota.reconcile(object(), args)

    def test_lost_finish_ack_reconcile(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'transaction.json'
            journal = dict(address='AA:BB:CC:DD:EE:FF', transaction=7, image_version=1,
                           state='verification_requested', image=dict(sha256='ab' * 32, crc32_iso=99))
            path.write_text(json.dumps(journal), encoding='utf-8')
            args = SimpleNamespace(journal=path, address=journal['address'])
            current = dict(transaction=7, sha256='ab' * 32, state=3, crc32_iso=99,
                           received=ota.APP_BYTES, verified=ota.APP_BYTES, metadata=[0xE0000, 0, 0, 0, 0])
            with patch.object(ota, 'status', return_value=current):
                result = ota.reconcile(object(), args)
            self.assertEqual(result['state'], 'verified_no_install_request')


if __name__ == '__main__':
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(ProtocolTests)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    output = Path(__file__).resolve().parent / 'output'
    output.mkdir(exist_ok=True)
    evidence = dict(tests=result.testsRun, failures=len(result.failures), errors=len(result.errors),
                    status='pass' if result.wasSuccessful() else 'fail',
                    sources={str(path): hashlib.sha256(path.read_bytes()).hexdigest()
                             for path in (Path(__file__).resolve(), TOOLS / 'noodoe_ota.py')},
                    limits='No Bluetooth API/hardware. PC stream, guards, reset grace, lost ACK reconciliation fixtures only.')
    (output / 'pc_results.json').write_text(json.dumps(evidence, indent=2), encoding='utf-8')
    sys.exit(0 if result.wasSuccessful() else 1)
