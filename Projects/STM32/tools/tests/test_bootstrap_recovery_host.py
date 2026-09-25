"""Offline host boundary validation; imports must not contact the target."""
from pathlib import Path
import struct,sys,unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from bootstrap_recovery_install import decode
from bootstrap_storage import recovery_image
from bootstrap_nor_verify import completed_snapshot

class MailboxBoundaryTests(unittest.TestCase):
    def fixture(self):
        b=bytearray(128);struct.pack_into('<4I',b,0,0x31575342,1,0xc0100000,1048576)
        struct.pack_into('<I',b,124,0xc0010100);return b
    def test_valid(self):
        self.assertEqual(decode(bytes(self.fixture()))['buffer'],0xc0100000)
    def test_extent_overflow_rejected(self):
        b=self.fixture();struct.pack_into('<I',b,12,0xffffffff)
        with self.assertRaises(ValueError):decode(bytes(b))
    def test_metadata_outside_sdram_rejected(self):
        b=self.fixture();struct.pack_into('<I',b,124,0x08010000)
        with self.assertRaises(ValueError):decode(bytes(b))
    def test_truncated_mailbox_rejected(self):
        with self.assertRaises(ValueError):decode(bytes(self.fixture()[:-1]))
    def test_unapproved_stock_never_packaged(self):
        with self.assertRaises(ValueError):recovery_image(bytes(0x70000),[1,2,3])
    def snapshot(self,seq=7,digest='aa',state=8):
        return dict(sequence=seq,ack=seq,state=state,error=0,bytes=0x8000000),digest
    def test_hash_completion_uses_two_post_ack_reads(self):
        reads=iter([self.snapshot(),self.snapshot()]);self.assertEqual(completed_snapshot(lambda:next(reads),7),self.snapshot())
        with self.assertRaises(StopIteration):next(reads)
    def test_hash_completion_rejects_torn_digest(self):
        reads=iter([self.snapshot(digest='old'),self.snapshot(digest='new')])
        with self.assertRaises(ValueError):completed_snapshot(lambda:next(reads),7)
    def test_hash_completion_rejects_changed_owner(self):
        reads=iter([self.snapshot(),self.snapshot(seq=8)])
        with self.assertRaises(ValueError):completed_snapshot(lambda:next(reads),7)
    def test_hash_completion_rejects_changed_state(self):
        reads=iter([self.snapshot(),self.snapshot(state=9)])
        with self.assertRaises(ValueError):completed_snapshot(lambda:next(reads),7)

if __name__=='__main__':unittest.main()
