"""Build a private, target-bound installer bundle; never contact hardware.

The approved donor is stock V5.16 / BL0.14. A read-only DeviceInfo observation
from BL0.15 is useful evidence but is NOT permission to transplant BL0.14's
assumptions. Without a compatible observation, inspect the artifacts only: no
installable ZIP is emitted. Firmware and donor bytes are never embedded in APK.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
import subprocess
import tempfile
import zipfile
import zlib
from pathlib import Path

import validate_image
from bootstrap_pack import APP_SHA, FULL_SHA
from resource_install import check_slot
from stock_installer import APP_BASE, APP_BYTES, Bundle, validate_app

PROJECT = Path(__file__).resolve().parents[1]
BL_SHA = 'f8b379c3fac078a8e01d8b6c36fccc5bb5ea5852a0db008822ef6871e0f38df5'
VERSIONS = {'bootstrap': (6, 4), 'cfw': (6, 4), 'stock': (5, 16)}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def device_info(payload):
    """Decode only compatibility fields; MAC, serial and PIN stay in evidence."""
    require(len(payload) == 82, 'Expected exact 82-byte stock DeviceInfo reply')
    model = payload[40:50].split(b'\0')[0].decode('ascii').strip()
    pcba = payload[75:81].split(b'\0')[0].decode('ascii').strip()
    require(model and all(32 <= ord(c) <= 126 and c != '\\' for c in model), 'Invalid device model')
    require(pcba and all(32 <= ord(c) <= 126 and c != '\\' for c in pcba), 'Invalid PCBA')
    return dict(status=struct.unpack_from('<H', payload)[0],
                major=struct.unpack_from('<H', payload, 2)[0],
                minor=struct.unpack_from('<H', payload, 4)[0], hardware=payload[7],
                boot_major=struct.unpack_from('<H', payload, 14)[0],
                boot_minor=struct.unpack_from('<H', payload, 16)[0], model=model, pcba=pcba)


def read_identity(path):
    """Reparse saved reply bytes; hand-entered JSON fields cannot set a target."""
    raw = path.read_bytes()
    require(len(raw) <= 16384, 'Identity JSON too large')
    doc = json.loads(raw)
    require(doc.get('format') == 'NOODOE_STOCK_IDENTITY_1', 'Identity evidence format')
    name = doc.get('payload_file', '')
    require(re.fullmatch(r'[A-Za-z0-9_-]+\.bin', name) is not None, 'Identity payload must be adjacent .bin')
    payload = (path.parent / name).read_bytes()
    require(sha(payload) == doc.get('payload_sha256'), 'Identity payload hash changed')
    info = device_info(payload)
    require(info['status'] == 0, 'Stock rejected DeviceInfo request')
    return info, dict(identity_json_sha256=sha(raw), payload_sha256=sha(payload), path=str(path.resolve()))


def compatible(info):
    require((info['major'], info['minor']) == (5, 16), 'Only observed stock V5.16 is approved')
    require((info['boot_major'], info['boot_minor'], info['pcba']) in ((0,14,'sr0601'),(0,15,'SR0701')),
            'Unsupported observed boot/PCBA profile')
    require(info['model'] == 'SAA1AA(KR)' and info['hardware'] == 0,
            'Unsupported model/HW profile')


def capture_identity(log, output):
    """Extract a complete checksum-valid RX frame from an existing local log.

    This is not a Bluetooth operation. Fragmented-only captures are refused,
    not guessed from nearby text or another vehicle's model name.
    """
    source = log.read_bytes()
    matches = []
    for number, line in enumerate(source.decode('utf-8').splitlines(), 1):
        if 'RX SPP:' not in line:
            continue
        for text in re.findall(r'(?:[0-9a-fA-F]{2} ){15,}[0-9a-fA-F]{2}', line):
            frame = bytes.fromhex(text)
            if len(frame) < 20 or frame[:2] != b'\x5a\xff':
                continue
            if struct.unpack_from('<H', frame, 2)[0] != len(frame):
                continue
            body = frame[9:-1]
            if (sum(body) + frame[-1]) & 255:
                continue
            if body[:3] != b'\xa5\x5a\x05' or not body[3] & 8:
                continue
            if len(body) != 92 or struct.unpack_from('<I', body, 6)[0] != 82:
                continue
            payload = body[10:]
            info = device_info(payload)
            require(info['status'] == 0, 'Captured DeviceInfo rejected')
            matches.append((number, payload, info))
    require(matches, 'No complete checksum-valid DeviceInfo RX frame in log')
    # Bind the exact complete reply, not just the visible version fields.
    require(len({p for _, p, _ in matches}) == 1, 'Multiple different device replies; use one capture')
    number, payload, info = matches[0]
    output.mkdir(parents=True, exist_ok=False)
    (output / 'reply.bin').write_bytes(payload)
    evidence = dict(format='NOODOE_STOCK_IDENTITY_1', payload_file='reply.bin',
                    payload_sha256=sha(payload), source='existing-stock-protocol-log',
                    source_path=str(log.resolve()), source_sha256=sha(source), source_line=number,
                    compatibility_fields=info)
    (output / 'identity.json').write_text(json.dumps(evidence, indent=2) + '\n', encoding='utf-8')
    return evidence


def stock_source(path):
    full = path.read_bytes()
    require(len(full) == 0x80000 and sha(full) == FULL_SHA, 'Unapproved full donor dump')
    require(sha(full[:0x8000]) == BL_SHA, 'Pinned immutable BL code mismatch')
    require(struct.unpack_from('<I', full, 0x8000)[0] == 0x000e0000, 'Donor BL version mismatch')
    app = full[0x10000:]
    require(sha(app) == APP_SHA, 'Pinned canonical stock APP mismatch')
    validate_app(app)
    return app, dict(full_sha256=sha(full), app_sha256=sha(app), immutable_bl_sha256=BL_SHA)


def resource_id(header):
    text = header.read_text(encoding='utf-8')
    match = re.search(r'^#define RESOURCES_EXPECTED_SHA\s+\{([^}]+)\}', text, re.M)
    require(match is not None, 'Missing Resources_Expected SHA')
    values = re.findall(r'0x([0-9a-fA-F]{2})\b', match[1])
    require(len(values) == 32, 'Expected exactly 32 resource digest bytes')
    return bytes.fromhex(''.join(values))


def bench_identity(path, stock_full):
    """A donor factory reconstruction is explicitly NOT a radio observation.

    It enables a local artifact for offline tests only. Both client frontends
    reject all wire installation from this scope, even on a matching identity.
    """
    doc = json.loads(path.read_text(encoding='utf-8'))
    full = stock_full.read_bytes()
    require(sha(full) == FULL_SHA, 'Factory reconstruction requires pinned original full dump')
    model = full[0xc034:0xc03e].split(b'\0')[0].decode('ascii').strip()
    pcba = full[0xc022:0xc028].split(b'\0')[0].decode('ascii').strip()
    boot = list(struct.unpack_from('<HH', full, 0x8000))
    require(doc.get('model_ascii') == model and doc.get('PCBA_ascii') == pcba
            and doc.get('boot_u16') == boot and doc.get('hardware_info_field') == 0,
            'Factory identity evidence differs from pinned donor')
    return dict(status=0, major=5, minor=16, hardware=0, boot_major=boot[0],
                boot_minor=boot[1], model=model, pcba=pcba), dict(
                    path=str(path.resolve()), evidence_sha256=sha(path.read_bytes()),
                    source='factory-and-stock-code-reconstruction-not-wire-observed',
                    model_address='0x0800c034', pcba_address='0x0800c022', stock_full_sha256=FULL_SHA)


def resources(path, expected):
    data = path.read_bytes()
    require(len(data) == 0x100000, 'Resources must be the exact 1MiB initial container')
    check_slot(data[:0x80000], expected)
    require(data[0x80000:] == b'\xff' * 0x80000, 'Initial resource slot B must be erased')
    return data


def build_artifact(elf_path, bin_path, role, expected, toolchain=None):
    """Prove ELF layout, independently reconstructed bytes and actual objcopy.

    A .bin path or stale manifest is never accepted as a sufficient build proof.
    The original ELF/BIN are read again after checking for concurrent rebuilding.
    """
    source, supplied = elf_path.read_bytes(), bin_path.read_bytes()
    elf = validate_image.Elf32(source)
    image, report = validate_image.validate(elf)
    require(supplied == image, role + ' BIN differs from validated ELF; rebuild/export first')
    objcopy = validate_image.find_objcopy(toolchain)
    with tempfile.TemporaryDirectory(prefix='noodoe-bundle-check-') as scratch:
        out = Path(scratch) / 'app.bin'
        command = [str(objcopy), '-O', 'binary', '--gap-fill=0xFF', str(elf_path.resolve()), str(out)]
        result = subprocess.run(command, capture_output=True, text=True, timeout=30,
                                creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
        require(result.returncode == 0, 'objcopy failed: ' + result.stderr.strip())
        require(out.read_bytes() == image, 'Objcopy disagrees with independent ELF reconstruction')
    require(elf_path.read_bytes() == source and bin_path.read_bytes() == supplied, 'Build changed during inspection')
    validate_app(image)
    requirement = struct.pack('<III', 0x51534352, 1, int(role == 'cfw')) + expected
    require(image[0x200:0x22c] == requirement, role + ' ResourceExpected requirement differs at APP+0x200')
    if role == 'bootstrap':
        require(0x50000 < len(image) <= APP_BYTES, 'Bootstrap does not meet stock OTA size gate')
        elf.symbol('g_bootstrap')
        start = elf.symbol('g_bootstrap_stock_deflate')['value']
        count = elf.symbol('g_bootstrap_stock_deflate_bytes')['value']
        n = struct.unpack('<I', elf.code_bytes(count, 4))[0]
        require(0 < n <= APP_BYTES, 'Invalid embedded stock asset size')
        compressed = elf.code_bytes(start, n)
        decoder = zlib.decompressobj(-15)
        original = decoder.decompress(compressed, APP_BYTES + 1)
        require(len(original) == APP_BYTES and decoder.eof and not decoder.unused_data
                and not decoder.unconsumed_tail and sha(original) == APP_SHA,
                'Bootstrap lacks the exact approved, fully decodable recovery APP')
    else:
        elf.symbol('g_product_ui')
        require('g_bootstrap' not in elf.symbols, 'Product ELF is a Bootstrap profile')
    return image, dict(elf=str(elf_path.resolve()), elf_sha256=sha(source),
                       bin=str(bin_path.resolve()), bin_sha256=sha(image), bytes=len(image),
                       padded_sha256=sha(image.ljust(APP_BYTES, b'\xff')),
                       reset_vector=report['reset_vector'], resource_id=expected.hex())


def inspect_artifacts(args):
    expected = resource_id(args.expected_header)
    stock, stock_report = stock_source(args.stock_full)
    resource = resources(args.resources, expected)
    images, reports = {}, {}
    for role in ('bootstrap', 'cfw'):
        images[role], reports[role] = build_artifact(getattr(args, role + '_elf'),
            getattr(args, role + '_bin'), role, expected, args.toolchain)
    images.update(stock=stock, resources=resource)
    report = dict(format='NOODOE_BUNDLE_ARTIFACT_AUDIT_1', hardware_access=False,
                  installable_bundle=False, stock=stock_report, builds=reports,
                  resources_sha256=sha(resource), resource_id=expected.hex(),
                  versions={k: list(v) for k, v in VERSIONS.items()})
    if args.identity:
        info, evidence = read_identity(args.identity)
        report.update(target=info, identity_evidence=evidence, target_scope='observed')
        try:
            compatible(info)
            report['target_compatible'] = True
        except ValueError as error:
            report.update(target_compatible=False, target_blocker=str(error))
    elif args.bench_factory_identity:
        info, evidence = bench_identity(args.bench_factory_identity, args.stock_full)
        compatible(info)
        report.update(target=info, identity_evidence=evidence, target_scope='bench-only',
                      target_compatible=True, wire_installation_allowed=False)
    else:
        report.update(target_compatible=False, target_blocker='Exact stock DeviceInfo evidence not supplied')
    return images, report


def make_bundle(images, report, destination):
    """Only a compatible observed target can become an installable ZIP."""
    require(report.get('target_compatible') is True, report.get('target_blocker', 'Unproven target'))
    info = report['target']; compatible(info)
    require(info['boot_minor'] == 14,
            'Legacy recovery ZIP cannot capture a BL identity; use NoodoeInstaller/tools/build_bundle.py for BL0.15')
    manifest = dict(format='NOODOE_RECOVERY_1', **{
        'app.base': hex(APP_BASE), 'app.bytes': hex(APP_BYTES),
        'target.hardware': str(info['hardware']), 'target.boot.major': str(info['boot_major']),
        'target.boot.minor': str(info['boot_minor']), 'target.boot.sha256': BL_SHA,
        'target.stock.major': '5', 'target.stock.minor': '16', 'target.model': info['model'],
        'target.pcba': info['pcba'], 'target.scope': report.get('target_scope', 'observed')})
    for role in ('bootstrap', 'cfw', 'stock', 'resources'):
        manifest.update({role + '.file': role + '.bin', role + '.sha256': sha(images[role])})
        if role in VERSIONS:
            major, minor = VERSIONS[role]
            manifest.update({role + '.major': str(major), role + '.minor': str(minor)})
    manifest_data = ''.join(k + '=' + v + '\n' for k, v in manifest.items()).encode('utf-8')
    require(not destination.exists(), 'Bundle destination already exists')
    destination.parent.mkdir(parents=True, exist_ok=True)
    # Serialize privately and validate through the real importer before exposing
    # the final file. Fixed ZIP metadata makes identical inputs reproducible.
    with tempfile.TemporaryDirectory(prefix='noodoe-bundle-', dir=destination.parent) as scratch:
        temporary = Path(scratch) / 'bundle.zip'
        with zipfile.ZipFile(temporary, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as out:
            for name, data in [('manifest.properties', manifest_data)] + [(r + '.bin', images[r]) for r in ('bootstrap', 'cfw', 'stock', 'resources')]:
                entry = zipfile.ZipInfo(name, (2026, 1, 1, 0, 0, 0)); entry.compress_type = zipfile.ZIP_DEFLATED
                entry.external_attr = 0o600 << 16; out.writestr(entry, data, compresslevel=9)
        validated = Bundle(temporary)
        require(validated.manifest == manifest, 'Final importer manifest disagrees')
        require(sha(validated.image('stock', True)) == APP_SHA, 'Final pinned stock hash changed')
        payload = temporary.read_bytes()
        with destination.open('xb') as out:
            out.write(payload)
    return dict(report, installable_bundle=manifest['target.scope'] == 'observed', bundle_sha256=sha(payload),
                bundle_bytes=len(payload), bundle=str(destination.resolve()), manifest=manifest)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='action', required=True)
    capture = sub.add_parser('capture-evidence', help='Parse an existing complete DeviceInfo RX log, offline')
    capture.add_argument('--protocol-log', type=Path, required=True)
    capture.add_argument('--output', type=Path, required=True)
    for action in ('inspect', 'build'):
        p = sub.add_parser(action)
        p.add_argument('--stock-full', type=Path, required=True)
        for role in ('bootstrap', 'cfw'):
            p.add_argument('--' + role + '-elf', type=Path, required=True)
            p.add_argument('--' + role + '-bin', type=Path, required=True)
        p.add_argument('--resources', type=Path, default=PROJECT / 'Resources/NOODOE.RSC')
        p.add_argument('--expected-header', type=Path, default=PROJECT / 'Middlewares/Noodoe/Resources/inc/Resources_Expected.h')
        target = p.add_mutually_exclusive_group(required=action == 'build')
        target.add_argument('--identity', type=Path)
        target.add_argument('--bench-factory-identity', type=Path, help='Reconstruct an OFFLINE-ONLY bundle; both clients refuse installation')
        p.add_argument('--toolchain', type=Path)
        p.add_argument('--output', type=Path, required=True, help='New audit directory; build adds recovery.zip')
    args = parser.parse_args()
    if args.action == 'capture-evidence':
        result = capture_identity(args.protocol_log, args.output)
        print(json.dumps(result['compatibility_fields'], indent=2)); return
    images, result = inspect_artifacts(args)
    args.output.mkdir(parents=True, exist_ok=False)
    if args.action == 'build':
        try:
            result = make_bundle(images, result, args.output / 'recovery.zip')
        except ValueError as error:
            result.update(build_error=str(error), installable_bundle=False)
            (args.output / 'audit.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
            raise
    (args.output / 'audit.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
