"""Reject incomplete/tampered install evidence before another allocation."""
import json,sys,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from bootstrap_install_chain import apply_install,apply_pending_batch
from resource_install import sha

class ChainTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name);self.uid=[1,2,3]
        self.before=bytes(0xb000);b=bytearray(self.before);b[0x1000]=1;b[0x9000]=2;self.after=bytes(b)
        self.plan=dict(address=0x9000,kind=5,name='CFWA.DAT')
        (self.root/'payload.bin').write_bytes(bytes(0x2000))
        self.write('plan.json',self.plan);self.write('changed-sectors.json',[0x1000,0x9000])
        for off in (0x1000,0x9000):
            (self.root/f'{off:08x}-before.bin').write_bytes(self.before[off:off+4096])
            (self.root/f'{off:08x}-after.bin').write_bytes(self.after[off:off+4096])
        regions=[dict(offset=o,length=n,before_sha256=sha(self.before[o:o+n]),after_sha256=sha(self.after[o:o+n])) for o,n in ((0,0x9000),(0x9000,0x2000))]
        self.write('regions.json',regions)
        self.result=dict(state='regions_verified',identity=dict(uid_words=self.uid),kind='gate-a',expected_whole_nor_sha256=sha(self.after),after=[dict(offset=r['offset'],length=r['length'],sha256=r['after_sha256']) for r in regions])
        self.write('result.json',self.result)
        self.proof=dict(state='two_independent_device_hashes_match_expected',identity=dict(uid_words=self.uid),expected_sha256=sha(self.after),passes=[dict(sequence=i,state=8,error=0,bytes=0x8000000,sha256=sha(self.after)) for i in (4,5)])
        (self.root/'whole-nor-verification').mkdir();self.write('whole-nor-verification/result.json',self.proof)
        # FAT allocation itself is independently tested; here isolate evidence boundaries.
        m=patch('bootstrap_install_chain.create_plan',return_value=(self.plan,self.after));m.start();self.addCleanup(m.stop)
    def write(self,name,value): (self.root/name).write_text(json.dumps(value))
    def apply(self,**kw): return apply_install(self.before,self.uid,self.root,**kw)
    def test_complete_evidence(self):self.assertEqual(self.apply()[0],self.after)
    def test_missing_global_proof(self):
        (self.root/'whole-nor-verification/result.json').unlink()
        with self.assertRaises(FileNotFoundError):self.apply()
        self.assertFalse(self.apply(require_global=False)[1]['physical_whole_nor_verified'])
    def test_wrong_uid(self):
        self.result['identity']['uid_words']=[4,5,6];self.write('result.json',self.result)
        with self.assertRaises(ValueError):self.apply()
    def test_incomplete_install(self):
        self.result['state']='writing';self.write('result.json',self.result)
        with self.assertRaises(ValueError):self.apply()
    def test_sector_tamper(self):
        (self.root/'00009000-after.bin').write_bytes(bytes(4096))
        with self.assertRaises(ValueError):self.apply()
    def test_region_physical_read_mismatch(self):
        self.result['after'][1]['sha256']='bad';self.write('result.json',self.result)
        with self.assertRaises(ValueError):self.apply()
    def test_repeated_hash_is_not_two_sweeps(self):
        self.proof['passes'][1]['sequence']=4;self.write('whole-nor-verification/result.json',self.proof)
        with self.assertRaises(ValueError):self.apply()
    def test_partial_sweep(self):
        self.proof['passes'][1]['bytes']=4096;self.write('whole-nor-verification/result.json',self.proof)
        with self.assertRaises(ValueError):self.apply()
    def test_different_sweep_digest(self):
        self.proof['passes'][0]['sha256']='bad';self.write('whole-nor-verification/result.json',self.proof)
        with self.assertRaises(ValueError):self.apply()
    def test_bad_plan(self):
        self.write('plan.json',dict(self.plan,address=0xa000))
        with self.assertRaises(ValueError):self.apply()
    def test_missing_sector(self):
        self.write('changed-sectors.json',[0x1000])
        with self.assertRaises(ValueError):self.apply()
    def test_pending_batch_has_fixed_prefix(self):
        with self.assertRaises(ValueError):apply_pending_batch(self.before,self.uid,[],'gate-journal')
        with self.assertRaises(ValueError):apply_pending_batch(self.before,self.uid,[self.root],'recovery')
        (self.root/'whole-nor-verification/result.json').unlink()
        data,records=apply_pending_batch(self.before,self.uid,[self.root],'gate-b')
        self.assertEqual(data,self.after)
        self.assertFalse(records[0]['physical_whole_nor_verified'])
if __name__=='__main__':unittest.main()
