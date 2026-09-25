"""Host-only corruption and interrupted-capture checks; never opens a debugger."""
import copy
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch,MagicMock
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import storage_swd_resume as subject


def segment(offset, data):
    crc = zlib.crc32(data)
    sequence = offset // 4096 + 1
    return dict(offset=offset, length=len(data), sequence=sequence, crc32=crc,
                sha256=hashlib.sha256(data).hexdigest(), reads=[dict(match=True, crc32=crc)],
                committed_response=dict(state=2, result=0, crc32=crc, response_offset=offset,
                                        response_length=len(data), completed=len(data),
                                        active_seq=sequence, response_seq=sequence))


class ResumeTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.raw = bytes(range(256)) * 64
        self.first = self.directory / 'A.bin'
        self.first.write_bytes(self.raw)
        self.partial = self.directory / 'B.partial'
        self.partial.write_bytes(self.raw[:8192])
        self.segments = [segment(i, self.raw[i:i+4096]) for i in range(0, len(self.raw), 4096)]
        self.manifest = dict(state='failed', verified=False,
                             captures=[dict(path=str(self.first), length=len(self.raw),
                                            terminal_done_validated=True, segments=self.segments,
                                            sha256=hashlib.sha256(self.raw).hexdigest())],
                             progress=dict(capture='B', received=8192, segments=self.segments[:2]))
        self.limits = patch.object(subject.backup, 'CAPACITY', len(self.raw))
        self.limits.start()
        self.addCleanup(self.limits.stop)

    def validate(self):
        return subject.validate_resume(self.directory, self.manifest)

    def test_valid_prefix(self):
        path, segments, received = self.validate()
        self.assertEqual(path, self.partial)
        self.assertEqual(received, 8192)
        self.assertEqual(len(segments), 2)

    def test_b_corruption_denied(self):
        data = bytearray(self.partial.read_bytes()); data[100] ^= 1
        self.partial.write_bytes(data)
        with self.assertRaises(RuntimeError): self.validate()

    def test_a_corruption_denied(self):
        self.first.write_bytes(bytes(len(self.raw)))
        with self.assertRaises(RuntimeError): self.validate()

    def test_uncommitted_tail_denied(self):
        with self.partial.open('ab') as stream: stream.write(b'not committed')
        with self.assertRaises(RuntimeError): self.validate()

    def test_gap_and_order_denied(self):
        self.manifest['progress']['segments'] = self.segments[1:3]
        with self.assertRaises(RuntimeError): self.validate()

    def test_response_error_denied(self):
        self.manifest = copy.deepcopy(self.manifest)
        self.manifest['progress']['segments'][0]['committed_response']['result'] = 1
        with self.assertRaises(RuntimeError): self.validate()

    def test_already_complete_denied(self):
        (self.directory / 'B.bin').write_bytes(self.raw)
        with self.assertRaises(RuntimeError): self.validate()

    def test_verified_manifest_denied(self):
        self.manifest['verified'] = True
        with self.assertRaises(RuntimeError): self.validate()

    def test_failed_release_can_resume_only_its_proven_prefix(self):
        self.manifest['state']='resume_failed'
        self.assertEqual(self.validate()[2],8192)

    def test_continuity_rejects_changed_persistent_generation(self):
        self.manifest['identity']=dict(uid_words=[1,2,3])
        expected=[dict(generation=1,active_sector=0),dict(generation=2,active_sector=1)]
        current=[dict(generation=3,active_sector=0,writes=7),dict(generation=2,active_sector=1,writes=9)]
        with patch.object(subject,'journal_expectations',return_value=(expected,[])),patch.object(subject,'journal_snapshot',return_value=current):
            with self.assertRaises(RuntimeError):subject.continuity(MagicMock(),Path('unused'),self.manifest,self.directory)

    def test_continuity_rejects_write_counter_change(self):
        self.manifest['identity']=dict(uid_words=[1,2,3])
        expected=[dict(generation=1,active_sector=0),dict(generation=2,active_sector=1)]
        current=[dict(generation=1,active_sector=0,writes=7),dict(generation=2,active_sector=1,writes=9)]
        changed=copy.deepcopy(current);changed[1]['writes']+=1
        with patch.object(subject,'journal_expectations',return_value=(expected,[])),patch.object(subject,'journal_snapshot',side_effect=[current,changed]):
            with self.assertRaises(RuntimeError):subject.continuity(MagicMock(),Path('unused'),self.manifest,self.directory)

    def test_hard_link_cannot_substitute_for_b(self):
        self.partial.unlink()
        self.partial.hardlink_to(self.first)
        self.manifest['progress'] = dict(capture='B', received=len(self.raw), segments=self.segments)
        with self.assertRaises(RuntimeError): self.validate()

    def test_failed_continuation_can_resume_its_committed_prefix(self):
        path, evidence, offset = self.validate()
        with path.open('ab') as stream: stream.write(self.raw[offset:offset+4096])
        evidence.append(self.segments[2])
        self.manifest['progress'] = dict(capture='B', received=12288, segments=evidence)
        self.assertEqual(self.validate()[2], 12288)

    def test_complete_partial_after_interruption_remains_valid(self):
        self.partial.write_bytes(self.raw)
        self.manifest['progress'] = dict(capture='B', received=len(self.raw), segments=self.segments)
        self.assertEqual(self.validate()[2], len(self.raw))
        self.assertTrue(subject.backup.compare(self.first, self.partial)['byte_identical'])


if __name__ == '__main__': unittest.main()
