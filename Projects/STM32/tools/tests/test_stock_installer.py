"""Offline stock installer: realistic framed peer, no Windows Bluetooth import."""
import importlib.util
import io
import struct
import tempfile
import unittest
import zipfile
from pathlib import Path

spec = importlib.util.spec_from_file_location('stock_installer', Path(__file__).resolve().parents[1] / 'stock_installer.py')
client = importlib.util.module_from_spec(spec); spec.loader.exec_module(client)


def image(size=512):
    return struct.pack('<II', 0x20020000, 0x08010101) + bytes(size - 8)


def make_bundle(path, extra='', first=None):
    manifest = ('format=NOODOE_RECOVERY_1\napp.base=0x08010000\napp.bytes=0x70000\n'
                'target.hardware=1\ntarget.boot.major=1\ntarget.boot.minor=0\n'
                'target.stock.major=5\ntarget.stock.minor=16\ntarget.model=AK550\ntarget.pcba=sr0601\ntarget.scope=observed\n' + 'target.boot.sha256=' + '00' * 32 + '\n')
    with zipfile.ZipFile(path, 'w') as out:
        for role in ('bootstrap', 'cfw', 'stock'):
            data = first if role == 'bootstrap' and first is not None else image()
            out.writestr(role + '.bin', data)
            manifest += f'{role}.file={role}.bin\n{role}.sha256={client.sha(data)}\n{role}.major=5\n{role}.minor={16 if role == "stock" else 17}\n'
        out.writestr('manifest.properties', manifest + extra)


class Peer:
    def __init__(self, fail=None, fragmented=False, implicit=False):
        self.pending = bytearray(); self.received = 128; self.commands = []
        self.phase = 0; self.total = 0; self.fail = fail; self.fragmented = fragmented; self.implicit = implicit

    def sendall(self, wire):
        if wire == b'\x05\0\0\0\0':
            self.pending += b'\x85' + struct.pack('<I', 21) + bytes(21); return
        if len(wire) == 9:
            return
        payload = wire[9:-1]; command = payload[2]; attr = payload[3]; p = payload[10:]
        self.commands.append((command, attr))
        if command == 5:
            r = bytearray(82); struct.pack_into('<HH', r, 2, 5, 16); r[7] = 1; r[14] = 1; r[40:45] = b'AK550'; r[75:81] = b'sr0601'
        elif command == 12:
            r = bytearray(11); r[2] = 1
        else:
            r = bytearray(16 if command == 13 else 6); r[2:4] = p[:2]; r[4] = 1
            if command == 10:
                self.phase = p[10]
            if command == 13:
                self.total += len(p) - 6; struct.pack_into('<I', r, 12, self.total)
            if self.fail == 'terminate' and command == 11 and p[2] == 2:
                return
            if self.fail == 'done' and command == 10 and p[10] == 3:
                return
        response = bytearray(client.sequence(client.command(command, 8, r), self.received, wire[5]))
        if self.implicit: response[4] = 0
        self.pending += response; self.received += 1

    def recv(self, length):
        if not self.pending:
            raise TimeoutError('Injected lost ACK')
        length = 3 if self.fragmented else length
        result = bytes(self.pending[:length]); del self.pending[:length]; return result


class Tests(unittest.TestCase):
    def test_bundle_validation(self):
        with tempfile.TemporaryDirectory() as temp:
            p = Path(temp) / 'bundle.zip'; make_bundle(p)
            b = client.Bundle(p); self.assertEqual(len(b.image('bootstrap', True)), 0x70000)
            self.assertEqual(b.image('bootstrap', True)[512:], b'\xff' * (0x70000 - 512))
            for bad in (image(0x80000), bytes(512), image(128)):
                make_bundle(p, first=bad)
                with self.assertRaises(ValueError): client.Bundle(p)
            for extra in ('format=NOODOE_RECOVERY_1\n', 'wrong=escape\\x\n'):
                make_bundle(p, extra)
                with self.assertRaises(ValueError): client.Bundle(p)

    def test_complete_stock_transfer_and_ign_boundary(self):
        with tempfile.TemporaryDirectory() as temp:
            p = Path(temp) / 'bundle.zip'; make_bundle(p); b = client.Bundle(p)
            peer = Peer(fragmented=True); j = client.Journal(Path(temp) / 'journal.json', 'aa', b.sha256)
            client.install_bootstrap(client.StockSession(peer), b, j)
            self.assertEqual(j.data['state'], 'STOCK_ACCEPTED_WAIT_IGN_OFF')
            self.assertEqual(peer.total, 0x70000); self.assertEqual(peer.phase, 3)
            self.assertEqual(set(c for c, a in peer.commands), {5, 12, 10, 11, 13})
            self.assertNotIn((2, 2), peer.commands)

    def test_ambiguous_terminate_or_done_never_replayed(self):
        with tempfile.TemporaryDirectory() as temp:
            p = Path(temp) / 'bundle.zip'; make_bundle(p); b = client.Bundle(p)
            for failure, state in (('terminate', 'STOCK_TERMINATE_RESULT_UNKNOWN'), ('done', 'STOCK_DONE_RESULT_UNKNOWN')):
                peer = Peer(fail=failure); j = client.Journal(Path(temp) / (failure + '.json'), 'aa', b.sha256)
                with self.assertRaises(TimeoutError): client.install_bootstrap(client.StockSession(peer), b, j)
                self.assertEqual(j.data['state'], state)
                before = len(peer.commands)
                with self.assertRaises(ValueError): client.install_bootstrap(client.StockSession(peer), b, j)
                self.assertEqual(len(peer.commands), before)
                if failure == 'terminate': self.assertEqual(peer.phase, 1)

    def test_journal_target_binding(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'journal.json'
            client.Journal(path, 'AA', 'hash').save('UNKNOWN')
            self.assertEqual(client.Journal(path, 'aa', 'hash').data['state'], 'UNKNOWN')
            with self.assertRaises(ValueError): client.Journal(path, 'BB', 'hash')
            with self.assertRaises(ValueError): client.Journal(path, 'AA', 'other')

    def test_implicit_ack_in_data_reply(self):
        self.assertEqual(client.StockSession(Peer(implicit=True)).identify()['major'], 5)

    def test_policy_reply_ids_and_frame_checksum(self):
        wire = client.sequence(client.command(5, 1), 0)
        self.assertEqual(wire.hex(), '5aff14000000000000a55a0501000000000000fb')
        request = struct.pack('<HHH', 5, 1, 7)
        self.assertTrue(client.StockSession.matches(11, request, struct.pack('<HHH', 0, 5, 7)))
        self.assertFalse(client.StockSession.matches(11, request, struct.pack('<HHH', 0, 4, 7)))


if __name__ == '__main__': unittest.main()
