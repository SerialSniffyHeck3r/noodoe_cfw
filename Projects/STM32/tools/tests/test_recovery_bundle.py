"""Offline packaging gates: identities, pinned donor, resource integrity and ELF/BIN agreement."""
import importlib.util
import json
import struct
import sys
import tempfile
import unittest
import zlib
from pathlib import Path
from types import SimpleNamespace
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import recovery_bundle as bundle


def reply(boot=14):
    data = bytearray(82)
    struct.pack_into('<HH', data, 2, 5, 16)
    struct.pack_into('<HH', data, 14, 0, boot)
    data[40:50] = b'SAA1AA(KR)'
    data[75:81] = b'sr0601'
    return bytes(data)


def app(required=1, digest=bytes(32), size=1024):
    data = bytearray(b'\xff' * size)
    struct.pack_into('<II', data, 0, 0x20030000, 0x08010301)
    data[0x200:0x22c] = struct.pack('<III', 0x51534352, 1, required) + digest
    return bytes(data)


def rsc():
    data = bytearray(b'\xff' * 0x100000)
    payload = bytes(range(48))
    table = b''.join(struct.pack('<IIII', i + 1, i * 4, 4, zlib.crc32(payload[i*4:i*4+4])) for i in range(12))
    digest = bytes.fromhex(bundle.sha(table + payload))
    struct.pack_into('<4I', data, 0, 0x31534352, 1, len(payload), 12)
    data[16:48] = digest; data[48:48+len(table)] = table
    data[4096:4096+len(payload)] = payload
    struct.pack_into('<II', data, 4088, zlib.crc32(data[:4088]), 0x434d5431)
    return bytes(data), digest


class Tests(unittest.TestCase):
    def test_exact_identity_payload_and_boot015_refusal(self):
        with tempfile.TemporaryDirectory() as name:
            path = Path(name); body = reply(15); (path/'reply.bin').write_bytes(body)
            doc = dict(format='NOODOE_STOCK_IDENTITY_1', payload_file='reply.bin', payload_sha256=bundle.sha(body),
                       hardware=99, boot_minor=14)
            (path/'identity.json').write_text(json.dumps(doc))
            info, _ = bundle.read_identity(path/'identity.json')
            self.assertEqual(info['hardware'], 0); self.assertEqual(info['boot_minor'], 15)
            with self.assertRaisesRegex(ValueError, 'Unsupported'): bundle.compatible(info)
            info['pcba']='SR0701';bundle.compatible(info)
            (path/'reply.bin').write_bytes(reply(14))
            with self.assertRaisesRegex(ValueError, 'hash changed'): bundle.read_identity(path/'identity.json')

    def test_capture_requires_checksum_and_unambiguous_reply(self):
        from stock_installer import command, sequence
        with tempfile.TemporaryDirectory() as name:
            root = Path(name); log = root/'protocol.log'
            frame = sequence(command(5, 9, reply(15)), 128, 0)
            log.write_text('2026 RX SPP: ' + frame.hex(' ').upper())
            proof = bundle.capture_identity(log, root/'evidence')
            self.assertEqual(proof['source_line'], 1)
            self.assertEqual(bundle.read_identity(root/'evidence/identity.json')[0]['boot_minor'], 15)
            log.write_text('RX SPP: ' + (frame[:-1] + bytes([frame[-1] ^ 1])).hex(' '))
            with self.assertRaisesRegex(ValueError, 'No complete'): bundle.capture_identity(log, root/'bad')
            other = sequence(command(5, 9, reply(14)), 129, 0)
            log.write_text('RX SPP: ' + frame.hex(' ') + '\nRX SPP: ' + other.hex(' '))
            with self.assertRaisesRegex(ValueError, 'Multiple'): bundle.capture_identity(log, root/'ambiguous')

    def test_stock_source_is_pinned_not_just_valid_vectors(self):
        with tempfile.TemporaryDirectory() as name:
            source = Path(name)/'stock.bin'; source.write_bytes(b'\xff'*0x10000 + app(size=0x70000))
            with self.assertRaisesRegex(ValueError, 'Unapproved'): bundle.stock_source(source)

    def test_resource_id_crc_and_inactive_slot(self):
        with tempfile.TemporaryDirectory() as name:
            path = Path(name)/'NOODOE.RSC'; data, expected = rsc(); path.write_bytes(data)
            self.assertEqual(bundle.resources(path, expected), data)
            with self.assertRaises(ValueError): bundle.resources(path, bytes(32))
            damaged = bytearray(data); damaged[4096] ^= 1; path.write_bytes(damaged)
            with self.assertRaises(ValueError): bundle.resources(path, expected)
            damaged = bytearray(data); damaged[-1] = 0; path.write_bytes(damaged)
            with self.assertRaisesRegex(ValueError, 'slot B'): bundle.resources(path, expected)

    def test_elf_binary_independent_objcopy_and_role_requirement(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name); elf_file = root/'app.elf'; bin_file = root/'app.bin'
            elf_file.write_bytes(b'validated ELF fixture'); good = app(); bin_file.write_bytes(good)
            fake_elf = SimpleNamespace(symbols={'g_product_ui': {}}, symbol=lambda name: {})
            def export(command, **kwargs):
                Path(command[-1]).write_bytes(good)
                return SimpleNamespace(returncode=0, stderr='')
            with mock.patch.object(bundle.validate_image, 'Elf32', return_value=fake_elf), \
                 mock.patch.object(bundle.validate_image, 'validate', return_value=(good, {'reset_vector':'0x08010301'})), \
                 mock.patch.object(bundle.validate_image, 'find_objcopy', return_value=Path('objcopy')), \
                 mock.patch.object(bundle.subprocess, 'run', side_effect=export):
                self.assertEqual(bundle.build_artifact(elf_file, bin_file, 'cfw', bytes(32))[0], good)
                with self.assertRaisesRegex(ValueError, 'ResourceExpected'):
                    bundle.build_artifact(elf_file, bin_file, 'cfw', b'\x01'*32)
                bin_file.write_bytes(good[:-1]+b'\x00')
                with self.assertRaisesRegex(ValueError, 'BIN differs'): bundle.build_artifact(elf_file, bin_file, 'cfw', bytes(32))

    def test_deterministic_bundle_and_noninstallable_refusal(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name); stock = app(size=0x70000)
            images = dict(bootstrap=app(required=0), cfw=app(), stock=stock, resources=rsc()[0])
            report = dict(target_compatible=True, target=bundle.device_info(reply()))
            with mock.patch.object(bundle, 'APP_SHA', bundle.sha(stock)):
                a = bundle.make_bundle(images, report, root/'a.zip')
                b = bundle.make_bundle(images, report, root/'b.zip')
            self.assertEqual(a['bundle_sha256'], b['bundle_sha256'])
            actual = bundle.Bundle(root/'a.zip')
            self.assertEqual(actual.manifest['bootstrap.major'], '6')
            self.assertEqual(actual.manifest['cfw.minor'], '1')
            self.assertEqual(actual.manifest['target.boot.sha256'], bundle.BL_SHA)
            with mock.patch.object(bundle, 'APP_SHA', bundle.sha(stock)):
                bench = bundle.make_bundle(images, dict(report,target_scope='bench-only'), root/'bench.zip')
            self.assertFalse(bench['installable_bundle'])
            with self.assertRaisesRegex(ValueError, 'Bench-only'): bundle.Bundle(root/'bench.zip').require_installable()
            with self.assertRaisesRegex(ValueError, 'already exists'): bundle.make_bundle(images, report, root/'a.zip')
            with self.assertRaisesRegex(ValueError, 'Unsupported'):
                bundle.make_bundle(images, dict(target_compatible=True,target=bundle.device_info(reply(15))), root/'bad.zip')
            self.assertFalse((root/'bad.zip').exists())


if __name__ == '__main__':
    unittest.main()
