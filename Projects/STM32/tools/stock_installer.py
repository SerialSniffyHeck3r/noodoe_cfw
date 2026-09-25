"""Stock SPP Bootstrap installer and recovery bundle validator (no device access without --execute).

Stock transfer completion is NOT installation completion: keep permanent12V and
switch IGN OFF after accepted DONE, then query Bootstrap over its NDCP protocol.
No generic DONE-on-error, reconnection/resend, full-flash write or firmware guessing.
"""
from __future__ import annotations
import argparse
import hashlib
import io
import json
import os
import secrets
import struct
import time
import zipfile
import zlib
from pathlib import Path

APP_BASE, APP_BYTES = 0x08010000, 0x70000


def sha(data):
    return hashlib.sha256(data).hexdigest()


def properties(data):
    """Canonical bundle manifest is UTF-8, one unescaped key=value per line."""
    result = {}
    for line in data.decode('utf-8').splitlines():
        if not line or line.startswith('#'):
            continue
        if '=' not in line or '\\' in line:
            raise ValueError('Use canonical key=value manifest lines without escapes')
        key, value = line.split('=', 1)
        if not key or key in result or key != key.strip() or value != value.strip():
            raise ValueError('Duplicate or noncanonical manifest key/value')
        result[key] = value
    return result


def validate_app(data):
    if not 0x1AC <= len(data) <= APP_BYTES:
        raise ValueError('APP-only image required; full512KiB dump is rejected')
    sp, pc = struct.unpack_from('<II', data)
    if not (0x20000000 < sp <= 0x20030000 and sp % 8 == 0
            and pc & 1 and APP_BASE <= pc - 1 < APP_BASE + len(data)):
        raise ValueError('Invalid APP MSP/Reset/base')


class Bundle:
    def __init__(self, path):
        archive = Path(path).read_bytes()
        if len(archive) > 4 * 1024 * 1024:
            raise ValueError('Bundle exceeds4MiB')
        self.sha256 = sha(archive)
        self.files = {}
        total = 0
        import re
        with zipfile.ZipFile(io.BytesIO(archive)) as zip_file:
            for entry in zip_file.infolist():
                if (not re.fullmatch(r'[A-Za-z0-9_.-]+', entry.filename) or '..' in entry.filename
                        or entry.is_dir() or entry.filename in self.files or len(self.files) >= 12):
                    raise ValueError('Unsafe/duplicate bundle entry')
                total += entry.file_size
                if total > 4 * 1024 * 1024:
                    raise ValueError('Expanded bundle exceeds4MiB')
                self.files[entry.filename] = zip_file.read(entry)
        manifest = self.files.get('manifest.properties', b'')
        if not manifest or len(manifest) > 16384:
            raise ValueError('Missing bounded manifest')
        self.manifest = properties(manifest)
        m = self.manifest
        if (m['format'] != 'NOODOE_RECOVERY_1' or int(m['app.base'], 0) != APP_BASE
                or int(m['app.bytes'], 0) != APP_BYTES):
            raise ValueError('Unsupported bundle layout')
        for key in ('target.hardware', 'target.boot.major', 'target.boot.minor',
                    'target.stock.major', 'target.stock.minor'):
            self.u16(key)
        if not m['target.model']:
            raise ValueError('Target model required')
        if not m['target.pcba'] or m['target.scope'] not in ('observed', 'bench-only'):
            raise ValueError('Exact PCBA and target evidence scope required')
        if not re.fullmatch('[0-9a-f]{64}', m['target.boot.sha256']):
            raise ValueError('BL code hash required')
        expected = {'manifest.properties'}
        for role in ('bootstrap', 'cfw', 'stock'):
            name = m[role + '.file']
            if name in expected or not name.endswith('.bin'):
                raise ValueError('Distinct APP files required')
            expected.add(name)
            validate_app(self.image(role))
            self.u16(role + '.major'); self.u16(role + '.minor')
        if 'resources.file' in m:
            if m['resources.file'] in expected:
                raise ValueError('Duplicate resource role')
            expected.add(m['resources.file']); self.image('resources')
        if expected != self.files.keys():
            raise ValueError('Unmanifested or missing bundle entry')
        if any(self.u16('stock.' + part) != self.u16('target.stock.' + part) for part in ('major', 'minor')):
            raise ValueError('Recovery version differs from target stock')

    def u16(self, key):
        value = int(self.manifest[key], 0)
        if not 0 <= value <= 65535:
            raise ValueError('uint16 out of range: ' + key)
        return value

    def image(self, role, padded=False):
        data = self.files[self.manifest[role + '.file']]
        if sha(data) != self.manifest[role + '.sha256']:
            raise ValueError('Bundle SHA mismatch: ' + role)
        return data.ljust(APP_BYTES, b'\xff') if padded else data

    def check_stock(self, device):
        self.require_installable()
        for field, key in (('hardware', 'target.hardware'), ('boot_major', 'target.boot.major'),
                           ('boot_minor', 'target.boot.minor'), ('major', 'target.stock.major'),
                           ('minor', 'target.stock.minor')):
            if device[field] != self.u16(key):
                raise ValueError('Device compatibility mismatch: ' + field)
        if (device['model'] != self.manifest['target.model'] or device['pcba'] != self.manifest['target.pcba']
                or device['status'] != 0):
            raise ValueError('Device model/status mismatch')

    def require_installable(self):
        if self.manifest['target.scope'] != 'observed':
            raise ValueError('Bench-only reconstructed bundle: wireless installation is disabled')


class Journal:
    def __init__(self, path, address, bundle):
        self.path = Path(path)
        self.data = json.loads(self.path.read_text(encoding='utf-8')) if self.path.exists() else {}
        if (self.data.get('address', address).lower() != address.lower()
                or self.data.get('bundle', bundle) != bundle):
            raise ValueError('Journal is bound to a different device/bundle')
        self.data.update(address=address, bundle=bundle)

    def save(self, state, **fields):
        self.data.update(state=state, updated_unix=time.time(), **fields)
        self.path.parent.mkdir(parents=True, exist_ok=True)
        temporary = self.path.with_name(self.path.name + '.tmp')
        with temporary.open('w', encoding='utf-8') as out:
            json.dump(self.data, out, indent=2); out.flush(); os.fsync(out.fileno())
        os.replace(temporary, self.path)


def sequence(payload=b'', index=0, ack=None, session=0):
    wire = bytearray(struct.pack('<2sHBBBBB', b'\x5a\xff', 9 + len(payload) + bool(payload),
                                 0x40 if ack is not None else 0, index, ack or 0, session, 0))
    if payload:
        wire += payload + bytes([(-sum(payload)) & 255])
    return bytes(wire)


def command(command_id, attr, payload=b''):
    return struct.pack('<2sBBHI', b'\xa5\x5a', command_id, attr, 0, len(payload)) + payload


class StockSession:
    def __init__(self, transport, timeout=15):
        self.transport, self.timeout = transport, timeout
        self.buffer = bytearray()
        self.index, self.received = 0, None

    def receive(self):
        data = self.transport.recv(4096)
        if not data:
            raise ConnectionError('SPP closed; command result may be unknown')
        return data

    def identify(self):
        self.transport.sendall(b'\x05\0\0\0\0')
        raw = bytearray(); deadline = time.monotonic() + self.timeout
        while time.monotonic() < deadline:
            raw += self.receive()
            if len(raw) > 4101 or raw[0] != 0x85:
                raise ValueError('Invalid stock bootstrap')
            if len(raw) >= 5:
                length = struct.unpack_from('<I', raw, 1)[0]
                if not 13 <= length <= 4096:
                    raise ValueError('Invalid bootstrap payload length')
                if len(raw) >= length + 5:
                    if len(raw) != length + 5:
                        raise ValueError('Unexpected trailing bootstrap data')
                    break
        else:
            raise TimeoutError('Stock bootstrap timeout')
        body = self.request(5, 1)
        if len(body) < 82:
            raise ValueError('Unsupported device-info ABI')
        self.identity_reply = body
        return dict(status=struct.unpack_from('<H', body)[0], major=struct.unpack_from('<H', body, 2)[0],
                    minor=struct.unpack_from('<H', body, 4)[0], hardware=body[7],
                    boot_major=struct.unpack_from('<H', body, 14)[0], boot_minor=struct.unpack_from('<H', body, 16)[0],
                    model=body[40:50].split(b'\0')[0].decode('ascii').strip(), mac=body[8:14].hex(),
                    pcba=body[75:81].split(b'\0')[0].decode('ascii').strip())

    def request(self, command_id, attr, payload=b'', session=0):
        self.transport.sendall(sequence(command(command_id, attr, payload), self.index, self.received, session))
        ack, reply = False, None
        deadline = time.monotonic() + self.timeout
        while time.monotonic() < deadline:
            self.buffer += self.receive()
            if len(self.buffer) > 32768:
                raise ValueError('Stock receive bound exceeded')
            while len(self.buffer) >= 9:
                if self.buffer[:2] != b'\x5a\xff':
                    raise ValueError('Unexpected stock stream header')
                size = struct.unpack_from('<H', self.buffer, 2)[0]
                if not 9 <= size <= 11844:
                    raise ValueError('Invalid stock frame size')
                if len(self.buffer) < size:
                    break
                frame = bytes(self.buffer[:size]); del self.buffer[:size]
                control, received, ack_index, frame_session = frame[4:8]
                if control & ~0x40 or frame_session > 1:
                    raise ValueError('Stock reset/invalid sequence; reconnect read-only')
                if (control & 0x40 or size > 9) and ack_index == self.index:
                    ack = True
                if size == 9:
                    continue
                content = frame[9:-1]
                if (sum(content) + frame[-1]) & 255:
                    raise ValueError('Stock checksum mismatch')
                self.transport.sendall(sequence(index=(self.index + 1) % 128 if ack else self.index, ack=received))
                if received == self.received:
                    continue
                if received < 128 or (self.received is not None and received != (128 if self.received == 255 else self.received + 1)):
                    raise ValueError('Out-of-order stock reply')
                self.received = received
                while content:
                    if len(content) < 10 or content[:2] != b'\xa5\x5a':
                        raise ValueError('Invalid command frame')
                    length = struct.unpack_from('<I', content, 6)[0]
                    if length > len(content) - 10:
                        raise ValueError('Truncated command reply')
                    answer = content[10:10+length]
                    if content[2] == command_id and content[3] & 8 and self.matches(command_id, payload, answer):
                        reply = answer
                    content = content[10+length:]
            if ack and reply is not None:
                self.index = (self.index + 1) % 128
                return reply
        raise TimeoutError('Command reply absent; no application-level resend is performed')

    @staticmethod
    def matches(command_id, request, reply):
        if command_id in (0x0a, 0x0b, 0x0d):
            if len(reply) < 4 or reply[2:4] != request[:2]:
                return False
            if command_id in (0x0b, 0x0d):
                offset = 4 if command_id == 0x0b else 2
                return len(reply) >= 6 and reply[4:6] == request[offset:offset+2]
        return True


def negotiate(task, size, phase, major, minor):
    return struct.pack('<HHHIB', task, 2, 0x0800, size, phase) + struct.pack('<HH', major, minor) + bytes(12)


def control(task, operation, crc, size):
    return struct.pack('<HHH', task, operation, 1) + b'\x01\0' + bytes(14) + struct.pack('<IIH', crc, size, 0)


def success(reply):
    if len(reply) < 2 or struct.unpack_from('<H', reply)[0]:
        raise ValueError('Stock rejected transfer: ' + str(reply[:2].hex()))
    return reply


def install_bootstrap(session, bundle, journal):
    if journal.data.get('state', 'IMPORTED') not in ('IMPORTED', 'STOCK_IDENTIFIED'):
        raise ValueError('Existing transaction is not restartable; inspect/reconcile instead')
    device = session.identify(); bundle.check_stock(device)
    major, minor = bundle.u16('bootstrap.major'), bundle.u16('bootstrap.minor')
    if (major, minor) <= (device['major'], device['minor']):
        raise ValueError('Stock requires strictly newer firmware version')
    riding = session.request(0x0c, 1)
    if len(riding) < 11 or struct.unpack_from('<H', riding)[0] != 0 or riding[2] != 1 or riding[10] != 0:
        raise ValueError('Fresh stationary IGN ON state required')
    image = bundle.image('bootstrap', padded=True); crc = zlib.crc32(image); task = 1
    if crc in (0, 0xffffffff):
        raise ValueError('Reserved image CRC')
    journal.save('STOCK_BEGIN_RESULT_UNKNOWN', device=device, image_sha256=sha(image))
    success(session.request(0x0a, 2, negotiate(task, len(image), 1, major, minor)))
    journal.save('STOCK_START_RESULT_UNKNOWN')
    success(session.request(0x0b, 2, control(task, 1, crc, len(image))))
    for offset in range(0, len(image), 11816):
        data = image[offset:offset+11816]
        journal.save('STOCK_DATA_RESULT_UNKNOWN', offset=offset)
        answer = success(session.request(0x0d, 2, struct.pack('<HHH', task, 1, 1) + data, 1))
        if (len(answer) < 16 or struct.unpack_from('<HH', answer, 2) != (task, 1)
                or struct.unpack_from('<I', answer, 12)[0] != offset + len(data)):
            raise ValueError('Data cumulative offset mismatch')
        print(f'Bootstrap transfer {offset+len(data)}/{len(image)}', flush=True)
    journal.save('STOCK_TERMINATE_RESULT_UNKNOWN')
    success(session.request(0x0b, 2, control(task, 2, crc, len(image))))
    journal.save('STOCK_DONE_RESULT_UNKNOWN')
    success(session.request(0x0a, 2, negotiate(task, len(image), 3, major, minor)))
    journal.save('STOCK_ACCEPTED_WAIT_IGN_OFF')
    return journal.data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('inspect', 'identify', 'install-bootstrap'))
    parser.add_argument('--bundle', type=Path)
    parser.add_argument('--evidence', type=Path, help='New directory for exact read-only DeviceInfo reply')
    parser.add_argument('--address'); parser.add_argument('--channel', type=int, default=0)
    parser.add_argument('--journal', type=Path); parser.add_argument('--execute', action='store_true')
    args = parser.parse_args()
    if args.action != 'identify' and not args.bundle: parser.error('--bundle required for this action')
    bundle = Bundle(args.bundle) if args.bundle else None
    if args.action == 'identify' and not args.bundle and not args.execute:
        print('Read-only DeviceInfo query; supply --execute --address and optionally --evidence. No bundle required.'); return
    summary = dict(bundle_sha256=bundle.sha256, manifest=bundle.manifest,
                   meaning='After accepted stock DONE: keep permanent12V, IGN OFF, query Bootstrap; no automatic key/reset action') if bundle else {}
    if args.action == 'inspect' or not args.execute:
        print(json.dumps(summary, indent=2)); return
    if not args.address: parser.error('--address required')
    from bluetooth_pc import BluetoothSocket
    with BluetoothSocket(args.address, channel=args.channel, timeout=15) as transport:
        session = StockSession(transport)
        if args.action == 'identify':
            result = session.identify()
            if args.evidence:
                args.evidence.mkdir(parents=True, exist_ok=False)
                (args.evidence / 'reply.bin').write_bytes(session.identity_reply)
                (args.evidence / 'identity.json').write_text(json.dumps(dict(format='NOODOE_STOCK_IDENTITY_1',
                    payload_file='reply.bin', payload_sha256=sha(session.identity_reply), source='windows-stock-device-info-read'), indent=2), encoding='utf-8')
            if bundle: bundle.check_stock(result)
        else:
            if not args.journal: parser.error('--journal required')
            journal = Journal(args.journal, args.address, bundle.sha256)
            result = install_bootstrap(session, bundle, journal)
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
