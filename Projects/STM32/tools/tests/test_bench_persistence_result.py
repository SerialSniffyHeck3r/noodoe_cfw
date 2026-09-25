"""Regional comparison and quiesce ownership tests, with no device access."""
from pathlib import Path
import sys,tempfile,unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import bench_persistence_result as v
from cfw_storage_install import record
from resource_install import swapped

UID=[1,2,3]
def ride(entries):
    b=bytearray(b'\xff'*262144)
    for slot,generation in entries.items():b[slot*4096:(slot+1)*4096]=record(2,UID,b'ride',generation)
    return bytes(swapped(b))

class Comparison(unittest.TestCase):
    def setUp(self):
        self.old=ride({0:10});self.r=dict(name='CFWRIDE.DAT',offset=0,length=len(self.old))
    def compare(self,new):return v.compare_region(self.r,self.old,new,UID,b'')
    def test_unchanged(self):self.assertTrue(self.compare(self.old)['exact'])
    def test_new_completed_sequence(self):self.assertEqual(self.compare(ride({0:10,1:11,2:12}))['delta']['new_generation'],12)
    def test_generation_jump(self):
        with self.assertRaises(ValueError):self.compare(ride({0:10,1:12}))
    def test_previous_overwrite(self):
        with self.assertRaises(ValueError):self.compare(ride({0:11}))
    def test_partial_record(self):
        b=bytearray(ride({0:10,1:11}));b[8191]^=1
        with self.assertRaises(ValueError):self.compare(bytes(b))
    def test_nonride_change(self):
        with self.assertRaises(ValueError):v.compare_region(dict(name='CFWPIC.DAT',offset=0,length=4),b'abcd',b'abce',UID,b'')

class Fake:
    def __init__(self,deny=False):self.quiesce_owned=False;self.calls=[];self.deny=deny
    def quiesce(self,elf,enable):
        self.calls.append(enable)
        if self.deny:raise RuntimeError('Another owner')
        self.quiesce_owned=enable;return True
    def descriptor(self):return dict(state=0,last_request_seq=1)

class Lease(unittest.TestCase):
    def run_fake(self,m,selected):
        with tempfile.TemporaryDirectory() as d:
            result=dict(identity={},regions=[])
            with patch.object(v,'journal_snapshot',return_value=[]),patch.object(v,'identity',return_value={}):
                v.run_reads(m,Path('unused'),b'',UID,b'',selected,result,Path(d)/'result.json')
            return result
    def test_success_release(self):
        m=Fake();r=self.run_fake(m,[]);self.assertEqual(m.calls,[True,False]);self.assertTrue(r['verified'])
    def test_failure_release(self):
        m=Fake()
        with patch.object(v,'read_segment',side_effect=RuntimeError('read failed')):
            with self.assertRaises(RuntimeError):self.run_fake(m,[dict(name='FAT',offset=0,length=4096)])
        self.assertEqual(m.calls,[True,False]);self.assertFalse(m.quiesce_owned)
    def test_other_owner_not_released(self):
        m=Fake(True)
        with self.assertRaises(RuntimeError):self.run_fake(m,[])
        self.assertEqual(m.calls,[True])

if __name__=='__main__':unittest.main()
