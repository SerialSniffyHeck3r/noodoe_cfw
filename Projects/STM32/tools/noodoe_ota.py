"""NDCP/Classic Bluetooth APP updater; preparation/status never enables writes.

Stage, commit and reset are separate explicit commands. Device authorization
after verified full NOR backup is an additional gate owned by Runtime.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
import secrets
import struct
import time
import zlib
from pathlib import Path

APP_BASE = 0x08010000
APP_BYTES = 0x70000
NOR_BYTES = 0x08000000
STAG, COMT, RESET = 0x53544147, 0x434F4D54, 0x52535421
BEGIN, DATA, FINISH, COMMIT, ABORT, STATUS, READ_STAGE, RESET_OP = range(0x40, 0x48)
HEADER = struct.Struct('<4sBBHIHH')


def encode(opcode: int, sequence: int, payload: bytes = b'', flags: int = 0) -> bytes:
    """Encode one bounded request; CRC is transport ISO-HDLC, not STM32 wordCRC."""
    if len(payload) > 1024 or flags & ~3:
        raise ValueError('NDCP payload/flags out of range')
    data = HEADER.pack(b'NDCP', 1, opcode, flags, sequence, len(payload), 0) + payload
    return data + struct.pack('<I', zlib.crc32(data))


class Decoder:
    """Bounded stream decoder; corrupt headers/CRCs resynchronize one byte later."""
    def __init__(self):
        self.data = bytearray()
        self.errors = 0

    def feed(self, data: bytes):
        frames = []
        for byte in data:
            self.data.append(byte)
            while self.data:
                if self.data[:1] != b'N':
                    del self.data[0]
                    continue
                if len(self.data) < 4:
                    break
                if self.data[:4] != b'NDCP':
                    del self.data[0]
                    continue
                if len(self.data) < 16:
                    break
                magic, version, opcode, flags, sequence, length, reserved = HEADER.unpack_from(self.data)
                if version != 1 or flags & ~3 or length > 1024 or reserved:
                    self.errors += 1
                    del self.data[0]
                    continue
                total = length + 20
                if len(self.data) < total:
                    break
                frame = bytes(self.data[:total])
                if zlib.crc32(frame[:-4]) != struct.unpack_from('<I', frame, total - 4)[0]:
                    self.errors += 1
                    del self.data[0]
                    continue
                del self.data[:total]
                frames.append(dict(opcode=opcode, flags=flags, sequence=sequence, payload=frame[16:-4]))
        return frames


class Connection:
    """One outstanding NDCP request, preserving partial RFCOMM receives."""
    def __init__(self, transport, timeout: float = 30):
        self.transport = transport
        self.timeout = timeout
        self.decoder = Decoder()
        self.sequence = secrets.randbelow(0x70000000) + 1

    def request(self, opcode: int, payload: bytes = b'') -> dict:
        self.sequence += 1
        sequence = self.sequence
        self.transport.sendall(encode(opcode, sequence, payload))
        deadline = time.monotonic() + self.timeout
        while time.monotonic() < deadline:
            data = self.transport.recv(4096)
            if not data:
                raise ConnectionError('SPP closed before matching reply; operation result is unknown')
            for frame in self.decoder.feed(data):
                if frame['sequence'] != sequence or frame['opcode'] != opcode:
                    continue
                if not frame['flags'] & 1 or len(frame['payload']) < 20:
                    raise ValueError('Malformed matching NDCP response')
                status, state, transaction, received, verified = struct.unpack_from('<5I', frame['payload'])
                result = dict(status=status, state=state, transaction=transaction,
                              received=received, verified=verified, sequence=sequence,
                              extra=frame['payload'][20:])
                if bool(frame['flags'] & 2) != bool(status):
                    raise ValueError('Response status/error flag disagree')
                if status:
                    raise RuntimeError(f'Device rejected opcode 0x{opcode:02X}: {result}')
                return result
        raise TimeoutError('No matching NDCP reply; operation result is unknown')


def file_hash(path: Path) -> str:
    """Hash backup files incrementally; do not load128MiB copies into RAM."""
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def load_image(path: Path) -> tuple[bytes, dict]:
    """Accept APP-only binary and make the entire448KiB tail deterministically FF."""
    data = path.read_bytes()
    if len(data) < 0x1AC or len(data) > APP_BYTES:
        raise ValueError('APP binary must be 428..458752 bytes; full512KiB dump is not an APP')
    msp, reset = struct.unpack_from('<II', data)
    if not (0x20000000 < msp <= 0x20030000 and msp % 8 == 0):
        raise ValueError('Initial MSP is outside aligned main SRAM stack bounds')
    if not (reset & 1 and APP_BASE <= reset - 1 < APP_BASE + len(data)):
        raise ValueError('Thumb Reset vector must point inside the supplied APP image')
    padded = data.ljust(APP_BYTES, b'\xFF')
    crc = zlib.crc32(padded)
    if crc in (0, 0xFFFFFFFF):
        raise ValueError('CRC collides with stock request sentinel; use a distinct reviewed image')
    return padded, dict(input=str(path.resolve()), input_bytes=len(data), input_sha256=hashlib.sha256(data).hexdigest(),
                        app_base=APP_BASE, bytes=APP_BYTES, sha256=hashlib.sha256(padded).hexdigest(),
                        crc32_iso=crc, msp=msp, reset=reset)


def save_journal(path: Path, journal: dict) -> None:
    """Flush a temporary file before atomic replacement; not a power-loss guarantee."""
    path.parent.mkdir(parents=True, exist_ok=True)
    journal['updated_at_unix'] = time.time()
    temporary = path.with_name(path.name + '.tmp')
    with temporary.open('w', encoding='utf-8') as output:
        json.dump(journal, output, ensure_ascii=False, indent=2)
        output.flush()
        os.fsync(output.fileno())
    temporary.replace(path)


def backup_evidence(a: Path, b: Path) -> dict:
    """Explicit full NOR A/B files must both be128MiB and hash-identical."""
    if a.resolve() == b.resolve() or a.stat().st_size != NOR_BYTES or b.stat().st_size != NOR_BYTES:
        raise ValueError('Two distinct complete128MiB NOR backups are required')
    first, second = file_hash(a), file_hash(b)
    if first != second:
        raise ValueError('NOR backup A/B hashes differ; stage is refused')
    return dict(a=str(a.resolve()), b=str(b.resolve()), bytes=NOR_BYTES, sha256=first)


def status(connection: Connection) -> dict:
    """Decode service metadata; status observation alone does not clear a request."""
    result = connection.request(STATUS)
    extra = result.pop('extra')
    if len(extra) != 64:
        raise ValueError('Unsupported STATUS layout')
    version, crc, authorized, *metadata = struct.unpack_from('<8I', extra)
    result.update(image_version=version, crc32_iso=crc, authorized=authorized,
                  metadata=metadata, sha256=extra[32:].hex())
    return result


def stage(connection: Connection, args) -> dict:
    """Only this explicit command erases/programs external APP staging; never commits."""
    if not args.allow_stage:
        raise ValueError('stage requires --allow-stage')
    evidence = backup_evidence(args.backup_a, args.backup_b)
    image, manifest = load_image(args.image)
    if args.journal.exists():
        raise FileExistsError('Use a new journal path; existing transaction history is preserved')
    current = status(connection)
    if not current['authorized'] or current['metadata'][0] != 0x000E0000 or current['metadata'][4] != 0:
        raise RuntimeError('Device staging authorization/no-pending gate is not satisfied')
    tx = secrets.randbelow(0xFFFFFFFF) + 1
    journal = dict(protocol='NDCP1', address=args.address, channel=args.channel,
                   transaction=tx, image_version=args.version, image=manifest,
                   nor_backup=evidence, state='begin_intent', acknowledged=0)
    save_journal(args.journal, journal)
    payload = struct.pack('<4I', tx, args.version, APP_BYTES, manifest['crc32_iso'])
    payload += bytes.fromhex(manifest['sha256']) + struct.pack('<2I', STAG, 0)
    connection.request(BEGIN, payload)
    for offset in range(0, APP_BYTES, 512):
        answer = connection.request(DATA, struct.pack('<II', tx, offset) + image[offset:offset + 512])
        if answer['received'] != offset + 512:
            raise RuntimeError('ACK offset mismatch; no retry/write guessing is performed')
        journal.update(state='staging', acknowledged=answer['received'])
        save_journal(args.journal, journal)
    journal['state'] = 'verification_requested'
    save_journal(args.journal, journal)
    answer = connection.request(FINISH, struct.pack('<I', tx))
    if answer['state'] != 3 or answer['verified'] != APP_BYTES or answer['extra'].hex() != manifest['sha256']:
        raise RuntimeError('Device full readback/hash verification did not match')
    journal['state'] = 'verified_no_install_request'
    save_journal(args.journal, journal)
    return journal


def install_step(connection: Connection, args) -> dict:
    """Commit and Reset each require a separate invocation and an intent record."""
    journal = json.loads(args.journal.read_text(encoding='utf-8'))
    if journal['address'].replace(':', '').lower() != args.address.replace(':', '').lower():
        raise ValueError('Journal Bluetooth target differs')
    current = status(connection)
    tx = journal['transaction']
    if current['transaction'] != tx or current['sha256'] != journal['image']['sha256']:
        raise RuntimeError('Current device session/hash differs from the verified journal')
    if args.action == 'commit':
        if not args.allow_commit or current['state'] != 3 or journal['state'] != 'verified_no_install_request':
            raise ValueError('commit requires --allow-commit and the still-verified transaction')
        payload = struct.pack('<II', tx, COMT) + bytes.fromhex(journal['image']['sha256'])
        opcode, state = COMMIT, 'committed_pending_next_boot'
    else:
        if not args.allow_reset or current['state'] != 4 or journal['state'] != 'committed_pending_next_boot':
            raise ValueError('reset requires --allow-reset and known committed metadata')
        payload = struct.pack('<II', tx, RESET)
        opcode, state = RESET_OP, 'reset_acknowledged_installation_not_verified'
    journal['state'] = args.action + '_intent_result_unknown'
    save_journal(args.journal, journal)
    answer = connection.request(opcode, payload)
    journal.update(state=state, last_reply={k: v for k, v in answer.items() if k != 'extra'})
    save_journal(args.journal, journal)
    if args.action == 'reset':
        # Device cancels RESET_WAIT on an intentional early disconnect. Keep RFCOMM
        # open beyond its1500ms ACK grace; this is not a proof of new APP boot.
        time.sleep(2.0)
        journal['state'] = 'reset_ack_grace_elapsed_installation_not_verified'
        save_journal(args.journal, journal)
    return journal


def reconcile(connection: Connection, args) -> dict:
    """Read-only device query reconciles a lost FINISH/COMMIT ACK with its journal."""
    journal = json.loads(args.journal.read_text(encoding='utf-8'))
    if journal['address'].replace(':', '').lower() != args.address.replace(':', '').lower():
        raise ValueError('Journal Bluetooth target differs')
    current = status(connection)
    image = journal['image']
    expected = [0x000E0000, journal['image_version'], 0x7F90, APP_BYTES, image['crc32_iso']]
    same_session = (current['transaction'] == journal['transaction'] and
                    current['sha256'] == image['sha256'] and current['crc32_iso'] == image['crc32_iso'] and
                    current['received'] == APP_BYTES and current['verified'] == APP_BYTES)
    if same_session and current['state'] == 3 and current['metadata'][0] == 0x000E0000 and current['metadata'][4] == 0:
        recovered = 'verified_no_install_request'
    elif same_session and current['state'] == 4 and current['metadata'] == expected:
        recovered = 'committed_pending_next_boot'
    elif current['transaction'] == 0 and current['metadata'] == expected[:4] + [0]:
        recovered = 'boot_request_cleared_app_not_verified'
    else:
        raise RuntimeError(f'Status cannot safely reconcile this journal: {current}')
    journal.setdefault('reconciliation_history', []).append(dict(previous=journal['state'], status=current))
    journal['state'] = recovered
    save_journal(args.journal, journal)
    return journal


def main() -> None:
    """CLI dispatch imports the Windows Bluetooth transport only for online commands."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--address', help='paired Noodoe Bluetooth address')
    parser.add_argument('--channel', type=int, default=0)
    parser.add_argument('--timeout', type=float, default=60)
    sub = parser.add_subparsers(dest='action', required=True)
    prepare = sub.add_parser('prepare');prepare.add_argument('image', type=Path);prepare.add_argument('--out', type=Path, required=True)
    sub.add_parser('status')
    recover = sub.add_parser('reconcile');recover.add_argument('--journal', type=Path, required=True)
    upload = sub.add_parser('stage');upload.add_argument('image', type=Path);upload.add_argument('--version', type=lambda v: int(v, 0), required=True)
    upload.add_argument('--journal', type=Path, required=True);upload.add_argument('--backup-a', type=Path, required=True);upload.add_argument('--backup-b', type=Path, required=True)
    upload.add_argument('--allow-stage', action='store_true')
    for action in ('commit', 'reset'):
        item = sub.add_parser(action);item.add_argument('--journal', type=Path, required=True);item.add_argument('--allow-' + action, action='store_true')
    args = parser.parse_args()
    if args.action == 'prepare':
        image, manifest = load_image(args.image)
        if args.out.exists():
            raise FileExistsError('Refusing to overwrite an existing prepared image')
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_bytes(image)
        save_journal(args.out.with_suffix(args.out.suffix + '.json'), manifest)
        print(json.dumps(manifest, indent=2));return
    if not args.address:
        parser.error('--address is required for online commands')
    from bluetooth_pc import BluetoothSocket
    with BluetoothSocket(args.address, channel=args.channel, timeout=args.timeout) as transport:
        connection = Connection(transport, args.timeout)
        if args.action == 'status':
            result = status(connection)
        elif args.action == 'reconcile':
            result = reconcile(connection, args)
        elif args.action == 'stage':
            result = stage(connection, args)
        else:
            result = install_step(connection, args)
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
