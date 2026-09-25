import sys, tempfile, unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from verified_flash import reconcile

class Board:
    def __init__(self,folder,reads):self.folder=Path(folder);self.reads=list(reads);self.calls=[]
    def dump(self,name,address,size,resume=False):
        self.calls.append((address,size,resume));return self.reads.pop(0)

class VerifiedFlashTests(unittest.TestCase):
    def test_no_reread_when_identical(self):
        with tempfile.TemporaryDirectory() as folder:
            b=Board(folder,[]);self.assertEqual(reconcile(b,'x',0x8000000,b'abcd',b'abcd'),(b'abcd',b'abcd'));self.assertEqual(b.calls,[])
    def test_only_inconsistent_page_is_reread(self):
        with tempfile.TemporaryDirectory() as folder:
            a=b'a'*4096+b'good';wrong=b'a'*4096+b'xxxx';b=Board(folder,[b'good',b'good'])
            self.assertEqual(reconcile(b,'x',0x8000000,a,wrong),(a,a))
            self.assertEqual(b.calls,[(0x8001000,4,False)]*2)
            self.assertEqual((Path(folder)/'x-verified-a.bin').read_bytes(),a)
    def test_disagreeing_fresh_reads_stop(self):
        with tempfile.TemporaryDirectory() as folder:
            with self.assertRaises(RuntimeError):reconcile(Board(folder,[b'a',b'b']),'x',0x8000000,b'a',b'b')
    def test_reference_never_overwrites_actual_different_memory(self):
        with tempfile.TemporaryDirectory() as folder:
            with self.assertRaises(RuntimeError):reconcile(Board(folder,[b'a',b'a']),'x',0x8000000,b'a',b'b',expected=True)
    def test_new_unobserved_page_stops(self):
        with tempfile.TemporaryDirectory() as folder:
            with self.assertRaises(RuntimeError):reconcile(Board(folder,[b'c',b'c']),'x',0x8000000,b'a',b'b')
    def test_ram_and_out_of_range_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            for address in [0x20000000,0x7ffffff,0x807ffff]:
                with self.assertRaises(RuntimeError):reconcile(Board(folder,[]),'x',address,b'abcd',b'abcd')
    def test_two_actual_reference_reads_allow_recovery(self):
        with tempfile.TemporaryDirectory() as folder:
            self.assertEqual(reconcile(Board(folder,[b'good',b'good']),'x',0x8000000,b'xxxx',b'good',expected=True),(b'good',b'good'))

if __name__=='__main__':unittest.main()
