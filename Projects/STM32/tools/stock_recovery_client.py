"""Local approved-stock recovery reference client. Default is offline validation.

Uses one authenticated SPP connection for identity, backup authorization, local
REQUEST/CONFIRM and reset grace. Does not upload an image or replay unknown writes.
"""
import argparse
import hashlib
import json
import struct
import time
from pathlib import Path
from stock_installer import Bundle, Journal, sha


class SocketPort:
    """Exact-read serial-style adapter for the existing generic NDCP client."""
    def __init__(self, socket): self.socket, self.pending = socket, bytearray()
    def write(self, data): self.socket.sendall(data); return len(data)
    def read(self, n):
        if not self.pending:
            data = self.socket.recv(4096)
            if not data: raise ConnectionError('SPP disconnected; new boot unverified')
            self.pending += data
        data = bytes(self.pending[:n]); del self.pending[:n]; return data


def identity(client, bundle):
    end = time.monotonic() + 15
    while True:
        try: response = client.request(0x58); break
        except Exception as error:
            # Read-only NOT_READY may be retried while firmware hashes its APP.
            if 'result=2,' not in str(error) or time.monotonic() >= end: raise
            time.sleep(.1)
    if len(response) != 84 or struct.unpack_from('<II', response) != (1, 2):
        raise ValueError('CFW identity role/layout mismatch')
    if (response[20:52].hex() != sha(bundle.image('cfw', True))
            or response[52:84].hex() != bundle.manifest['target.boot.sha256']):
        raise ValueError('Actual CFW APP/BL code differs from recovery bundle')
    return list(struct.unpack_from('<3I', response, 8))


def backup_proof(path, a, b, uid):
    proof = json.loads(path.read_text(encoding='utf-8'))
    if proof.get('state') != 'verified' or proof['identity']['uid_words'] != uid:
        raise ValueError('Verified provisioning proof for actual UID required')
    if a.resolve() == b.resolve() or a.stat().st_size != 0x8000000 or b.stat().st_size != 0x8000000:
        raise ValueError('Independent complete128MiB backup files required')
    digest = hashlib.sha256()
    with a.open('rb') as first, b.open('rb') as second:
        for data in iter(lambda: first.read(32768), b''):
            if data != second.read(len(data)): raise ValueError('Backup A/B differ')
            digest.update(data)
    if digest.hexdigest() != proof['after_sha256']:
        raise ValueError('Backup hash differs from retained provisioning evidence')
    return digest.hexdigest()


def restore(client, bundle, journal, proof, a, b):
    bundle.require_installable()
    if journal.data.get('state', 'IMPORTED') != 'IMPORTED':
        raise ValueError('Existing recovery transaction; query status instead of replay')
    uid = identity(client, bundle)
    digest = backup_proof(proof, a, b, uid)
    journal.save('RECOVERY_AUTH_INTENT', uid_words=uid, backup_sha256=digest)
    client.request(0x1f, struct.pack('<4I', *uid, 0x42414b32))
    journal.save('RECOVERY_REQUEST_RESULT_UNKNOWN')
    requested = client.request(0x48, struct.pack('<II', 1, 0x53544f43))
    if len(requested) != 28 or struct.unpack_from('<I', requested)[0] != 2:
        raise ValueError('Device did not enter recovery confirmation state')
    journal.save('RECOVERY_COMMIT_RESULT_UNKNOWN')
    committed = client.request(0x48, struct.pack('<II', 2, 0x53544f43))
    if len(committed) != 28 or struct.unpack_from('<I', committed)[0] != 6:
        raise ValueError('Device did not accept recovery reset')
    journal.save('RECOVERY_RUNNING')
    time.sleep(2)
    deadline = time.monotonic() + 120
    while time.monotonic() < deadline:
        try: result = client.request(0x48, bytes(8))
        except Exception as error:
            journal.save('RECOVERY_RESULT_UNKNOWN_BOOT_UNVERIFIED', detail=str(error))
            return journal.data
        if len(result) != 28: raise ValueError('Unknown recovery status layout')
        state, source_ready, verified, total, error, flags, reserved = struct.unpack('<7I', result)
        if state == 5 or error: raise ValueError('Device reported recovery failure')
        time.sleep(.25)
    raise TimeoutError('Recovery did not reset; no automatic CONFIRM repeat')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('action', choices=('status', 'restore'))
    p.add_argument('--bundle', type=Path, required=True); p.add_argument('--address')
    p.add_argument('--execute', action='store_true'); p.add_argument('--journal', type=Path)
    p.add_argument('--proof', type=Path); p.add_argument('--backup-a', type=Path); p.add_argument('--backup-b', type=Path)
    args = p.parse_args(); bundle = Bundle(args.bundle)
    if not args.execute:
        print(json.dumps(dict(action=args.action, bundle_sha256=bundle.sha256, executes=False))); return
    if not args.address: p.error('--address required')
    if args.action == 'restore' and not all((args.journal, args.proof, args.backup_a, args.backup_b)):
        p.error('restore requires --journal --proof --backup-a --backup-b')
    from bluetooth_pc import BluetoothSocket
    from noodoe_control import Client
    with BluetoothSocket(args.address, timeout=60) as socket:
        client = Client(SocketPort(socket), timeout=60)
        if args.action == 'status':
            uid = identity(client, bundle)
            result = dict(uid_words=uid, recovery_words=struct.unpack('<7I', client.request(0x48, bytes(8))))
        else:
            journal = Journal(args.journal, args.address, bundle.sha256)
            result = restore(client, bundle, journal, args.proof, args.backup_a, args.backup_b)
    print(json.dumps(result, indent=2))


if __name__ == '__main__': main()
