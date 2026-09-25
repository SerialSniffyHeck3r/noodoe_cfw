"""Offline SWD backup protocol/whitelist tests; no programmer is invoked."""
import hashlib
import importlib.util
import json
import struct
import tempfile
import unittest
from unittest.mock import patch
from types import SimpleNamespace
import zlib
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('storage_swd_backup', TOOLS / 'storage_swd_backup.py')
backup = importlib.util.module_from_spec(spec);spec.loader.exec_module(backup)
EVIDENCE = TOOLS.parents[2] / 'analysis/2026-09-12-integrated-bringup'
ACTUAL_ELF = EVIDENCE / 'integrated-final-image/FuckNudo_Noodoe_CFW_Project.elf'
ACTUAL_BIN = EVIDENCE / 'integrated-final-image/app.bin'
ACTUAL_READ = EVIDENCE / 'nor-full-backup/00006-running-app-identity.bin'
ACTUAL_SHA256 = '89b28979eef992a7d1dea8361755625e0ac41edc56e33f34673682c1981898f6'


def box_bytes(**updates):
    """Construct a known128-byte ABI fixture, not a fake parser implementation."""
    values = dict.fromkeys(backup.FIELDS, 0)
    values.update(magic=backup.MAGIC, abi=1, bytes=128, buffer_address=0xC0010000,
                  buffer_capacity=backup.BUFFER_BYTES, nor_capacity=backup.CAPACITY,
                  jedec_id=0xC2201B, uid0=1, uid1=2, uid2=3)
    values.update(updates)
    return struct.pack('<32I', *(values[key] for key in backup.FIELDS))


class ProtocolTests(unittest.TestCase):
    def test_progress_publish_retries_only_transient_windows_lock(self):
        with tempfile.TemporaryDirectory() as directory:
            target=Path(directory)/'manifest.json';original=Path.replace;attempts=[]
            def replace(source,destination):
                attempts.append(1)
                if len(attempts)<3:raise PermissionError('temporary sharing violation')
                return original(source,destination)
            with patch.object(Path,'replace',replace),patch.object(backup.time,'sleep'):
                backup.save(target,dict(state='in_progress',verified=False))
            self.assertEqual(len(attempts),3)
            self.assertFalse(json.loads(target.read_text())['verified'])
            with patch.object(Path,'replace',side_effect=PermissionError('permanent')),patch.object(backup.time,'sleep'),self.assertRaises(PermissionError):
                backup.save(target,dict(state='not_published'))
            self.assertEqual(json.loads(target.read_text())['state'],'in_progress')

    def test_product_layout_identity_uses_08020000(self):
        elf=TOOLS.parent/'Release/FuckNudo_Noodoe_CFW_Project.elf'
        _,payload,manifest=backup.elf_app_image(elf)
        self.assertEqual(manifest['layout_version'],2)
        self.assertEqual(int(manifest['flash_address'],0),0x08020000)
        test=self
        class Memory(backup.LiveMemory):
            def running(self):pass
            def upload(self,name,address,length):
                test.assertEqual(address,0x08020000);test.assertEqual(length,len(payload))
                p=self.directory/'read.bin';p.write_bytes(payload);return p
        with tempfile.TemporaryDirectory() as directory:
            memory=Memory('test',Path(directory),int(manifest['symbols']['g_storage_swd']['address'],0),950,10)
            result=memory.verify_app(elf)
            self.assertTrue(memory.app_verified)
            self.assertEqual(result['live_span_address'],0x08020000)

    def setUp(self):
        # A failed whitelist regression must never accidentally invoke a real debugger.
        self.no_device = patch.object(backup.subprocess, 'run', side_effect=AssertionError('Device invocation forbidden'))
        self.no_device.start()

    def tearDown(self):
        self.no_device.stop()

    def test_elf_load_range_and_truncation(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'test.elf'
            original = ACTUAL_ELF.read_bytes()
            damaged = bytearray(original)
            phoff = struct.unpack_from('<I', damaged, 28)[0]
            self.assertEqual(struct.unpack_from('<I', damaged, phoff)[0], 1)
            struct.pack_into('<I', damaged, phoff + 12, 0x08000000)
            for data in (bytes(damaged), original[:84], b'not an ELF'):
                path.write_bytes(data)
                with self.assertRaises(RuntimeError):
                    backup.elf_app_image(path)

    def test_actual_elf_gap_fill_matches_installed_image_and_read(self):
        source, image, manifest = backup.elf_app_image(ACTUAL_ELF)
        self.assertEqual(len(image), 383400)
        self.assertEqual(hashlib.sha256(image).hexdigest(), ACTUAL_SHA256)
        self.assertEqual(image, ACTUAL_BIN.read_bytes())
        self.assertEqual(image, ACTUAL_READ.read_bytes())
        self.assertEqual(manifest['binary_sha256'], ACTUAL_SHA256)
        # This exact four-byte gap caused the live rejection. It is not section
        # payload: the installed exporter uses FF, while PT_LOAD file padding is00.
        phoff = struct.unpack_from('<I', source, 28)[0]
        _, offset, _, load, filesz, _, _, _ = struct.unpack_from('<8I', source, phoff)
        relative = 0x080101AC - load
        self.assertLess(relative + 4, filesz)
        self.assertEqual(source[offset + relative:offset + relative + 4], bytes(4))
        self.assertEqual(image[0x1AC:0x1B0], b'\xff' * 4)

    def test_running_app_identity_is_required_before_mailbox_writes(self):
        class Memory(backup.LiveMemory):
            def running(self):
                pass

            def upload(self, name, address, length):
                self.test.assertEqual(address, backup.APP_BASE)
                self.test.assertEqual(length, len(self.actual))
                path = self.directory / 'app.bin';path.write_bytes(self.actual)
                return path

        with tempfile.TemporaryDirectory() as directory:
            path = ACTUAL_ELF;payload = ACTUAL_READ.read_bytes()
            _, _, manifest = backup.elf_app_image(path)
            mailbox = int(manifest['symbols']['g_storage_swd']['address'], 0)
            memory = Memory('test', Path(directory), mailbox, 4000, 10)
            memory.test = self;memory.validated = True;memory.actual = payload
            with self.assertRaises(RuntimeError):
                memory.request(0, 4096, 1)
            evidence = memory.verify_app(path)
            self.assertTrue(memory.app_verified)
            self.assertEqual(evidence['compared_bytes'], len(payload))
            self.assertEqual(evidence['canonical_sha256'], ACTUAL_SHA256)
            # Reject changed vector, code, FLASH initializer(.data/RamFunc) and
            # canonical gap bytes; neither loaded bytes nor gaps are ignored.
            for offset in (0, 0x1B0, len(payload)-1, 0x1AC):
                altered = bytearray(payload);altered[offset] ^= 1
                memory.actual = bytes(altered)
                with self.assertRaises(RuntimeError):
                    memory.verify_app(path)
                self.assertFalse(memory.app_verified)
                with self.assertRaises(RuntimeError):
                    memory.request(0, 4096, 1)

    def test_qualified_read_rate_does_not_increase_request_write_rate(self):
        with tempfile.TemporaryDirectory() as directory:
            memory = backup.LiveMemory('test', Path(directory), 0x20001000, 4000, 10)
            with patch.object(backup.subprocess, 'run', return_value=SimpleNamespace(returncode=0)) as run:
                memory._command('read-clock', ['-u', '0x20001000', '0x80', 'unused.bin'], True)
                read_command = run.call_args.args[0]
                memory._command('write-clock', ['-w32', '0x20001040', '0'], False)
                write_command = run.call_args.args[0]
            self.assertIn('freq=4000', read_command)
            self.assertIn('freq=50', write_command)
            self.assertIn('mode=HOTPLUG', read_command)
            self.assertIn('mode=HOTPLUG', write_command)

    def test_layout_and_alias_guards(self):
        descriptor = backup.decode(box_bytes())
        self.assertEqual(descriptor['buffer_address'], 0xC0010000)
        for update in (dict(magic=0), dict(abi=2), dict(bytes=124), dict(jedec_id=0),
                       dict(buffer_address=0x08000000), dict(buffer_address=0xC3FFFFE0),
                       dict(buffer_capacity=4096), dict(init_result=1)):
            with self.assertRaises(RuntimeError):
                backup.decode(box_bytes(**update))

    def test_mutable_poll_counter_is_not_response_identity(self):
        first = backup.decode(box_bytes(state=2, response_seq=5))
        second = first.copy();second['polls'] += 1
        self.assertTrue(backup.stable(first, second))
        second['response_seq'] += 1
        self.assertFalse(backup.stable(first, second))

    def test_flash_upload_and_unvalidated_write_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            memory = backup.LiveMemory('test', Path(directory), 0x20001000, 100, 10)
            with self.assertRaises(RuntimeError):
                memory.upload('forbidden-flash', 0x08000000, 4096)
            with self.assertRaises(RuntimeError):
                memory.request(0, 4096, 1)
            self.assertEqual(memory.counter, 0)

    def test_request_only_touches_five_volatile_words(self):
        class Memory(backup.LiveMemory):
            def descriptor(self):
                return backup.decode(box_bytes())

            def running(self):
                pass

            def _command(self, name, operations, read):
                self.commands.append((operations, read))

        with tempfile.TemporaryDirectory() as directory:
            memory = Memory('test', Path(directory), 0x20001000, 950, 10)
            memory.validated = True;memory.app_verified = True;memory.commands = []
            memory.request(0x800000, 0x800000, 7)
            self.assertEqual(len(memory.commands), 3)
            addresses = [int(command[0][1], 16) for command in memory.commands]
            self.assertEqual(addresses, [0x20001040, 0x20001030, 0x20001040])
            self.assertEqual(memory.commands[-1][0][-1], '0x7')
            for operations, read in memory.commands:
                self.assertFalse(read)
                self.assertEqual(operations[0], '-w32')
                self.assertFalse(any(word in operations for word in ('-halt', '-run', '-rst', '-w', '-e')))

    def test_same_buffer_retry_and_changed_response_rejection(self):
        with tempfile.TemporaryDirectory() as directory:
            class Memory:
                def __init__(self, mutate=False):
                    self.data = bytes(range(256)) * 16
                    self.requests = self.reads = 0
                    self.mutate = mutate
                    self.box = backup.decode(box_bytes(state=2, active_seq=7, response_seq=7,
                                                       response_length=4096, response_address=0xC0010000,
                                                       completed=4096, crc32=zlib.crc32(self.data)))

                def request(self, offset, length, sequence):
                    self.requests += 1

                def descriptor(self):
                    return self.box.copy()

                def running(self):
                    pass

                def upload(self, name, address, length):
                    self.reads += 1
                    path = Path(directory) / f'{self.mutate}-{self.reads}.bin'
                    path.write_bytes((b'X' + self.data[1:]) if self.reads == 1 else self.data)
                    if self.mutate:
                        self.box['response_seq'] += 1
                    return path

            memory = Memory()
            path, evidence = backup.read_segment(memory, 0, 4096, 7, 1, 3)
            self.assertEqual(path.read_bytes(), memory.data)
            self.assertEqual(memory.requests, 1)
            self.assertEqual(memory.reads, 2)
            self.assertEqual(len(evidence['reads']), 2)
            changed = Memory(True)
            with self.assertRaises(RuntimeError):
                backup.read_segment(changed, 0, 4096, 7, 1, 3)
            self.assertEqual(changed.requests, 1)
            self.assertEqual(changed.reads, 1)


if __name__ == '__main__':
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(ProtocolTests))
    output = Path(__file__).resolve().parent / 'storage_swd_pc_output'
    output.mkdir(exist_ok=True)
    report = dict(tests=result.testsRun, status='pass' if result.wasSuccessful() else 'fail',
                  sources={str(path): hashlib.sha256(path.read_bytes()).hexdigest()
                           for path in (Path(__file__).resolve(), TOOLS / 'storage_swd_backup.py',
                                        TOOLS / 'validate_image.py')},
                  fixtures={str(path): hashlib.sha256(path.read_bytes()).hexdigest()
                            for path in (ACTUAL_ELF, ACTUAL_BIN, ACTUAL_READ)},
                  limits='Offline mailbox and command-boundary mocks; no SWD/NOR/SDRAM hardware.')
    (output / 'results.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    raise SystemExit(0 if result.wasSuccessful() else 1)
