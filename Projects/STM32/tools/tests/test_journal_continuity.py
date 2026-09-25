"""Power-loss/identity/proof boundary tests; no target access."""
from pathlib import Path
import sys,unittest,tempfile,json,zlib
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import journal_continuity as s
from cfw_storage_install import record
from resource_install import swapped,sha

UID=[1,2,3]
def disk(updates):
    b=bytearray(b'\xff'*s.LENGTH)
    for slot,generation in updates.items():b[slot*4096:(slot+1)*4096]=record(2,UID,b'ride',generation)
    return bytes(swapped(b))

class DeltaTests(unittest.TestCase):
    def setUp(self):self.old=disk({0:10});self.new=disk({0:10,1:11})
    def test_next_committed_record(self):self.assertEqual(s.audit_delta(self.old,self.new,UID)['changed_sectors'],[1])
    def test_skipped_slot(self):
        with self.assertRaises(ValueError):s.audit_delta(self.old,disk({0:10,2:11}),UID)
    def test_generation_jump(self):
        with self.assertRaises(ValueError):s.audit_delta(self.old,disk({0:10,1:12}),UID)
    def test_previous_latest_overwritten(self):
        with self.assertRaises(ValueError):s.audit_delta(self.old,disk({0:11}),UID)
    def test_uid_mismatch(self):
        with self.assertRaises(ValueError):s.audit_delta(self.old,self.new,[1,2,4])
    def test_uncommitted_record(self):
        bad=bytearray(self.new);bad[8191]^=1
        with self.assertRaises(ValueError):s.audit_delta(self.old,bytes(bad),UID)
    def test_payload_corruption(self):
        bad=bytearray(self.new);bad[4200]^=1
        with self.assertRaises(ValueError):s.audit_delta(self.old,bytes(bad),UID)
    def test_wrap_generation(self):
        a=disk({63:0xffffffff});b=disk({63:0xffffffff,0:0});self.assertEqual(s.audit_delta(a,b,UID)['new_generation'],0)
    def test_two_records(self):
        self.assertEqual(s.audit_delta(self.old,disk({0:10,1:11,2:12}),UID)['new_generation'],12)
    def test_same_bytes_not_delta(self):
        with self.assertRaises(ValueError):s.audit_delta(self.old,self.old,UID)

class ProofTests(unittest.TestCase):
    def setUp(self):
        self.t=tempfile.TemporaryDirectory();self.addCleanup(self.t.cleanup);self.p=Path(self.t.name)
        self.old=disk({0:10});self.new=disk({0:10,1:11});self.raw=b'prefix00'+self.old+b'tail';self.address=8
        self.captures=[]
        for seq in (1,2):
            p=self.p/f'{seq}.bin';p.write_bytes(self.new);crc=zlib.crc32(self.new)
            c=dict(state=2,result=0,init_result=0,active_seq=seq,response_seq=seq,response_offset=8,response_length=s.LENGTH,completed=s.LENGTH,
                   uid0=1,uid1=2,uid2=3,nor_capacity=0x8000000,jedec_id=0xc2201b,crc32=crc)
            self.captures.append(dict(offset=8,length=s.LENGTH,sequence=seq,sha256=sha(self.new),crc32=crc,committed_response=c,reads=[dict(path=str(p),match=True,crc32=crc)]))
        self.ext=patch.object(s,'extent',return_value=8);self.ext.start();self.addCleanup(self.ext.stop)
    def test_exact_region_only(self):
        result,audit=s.derive(self.raw,UID,self.captures);self.assertEqual(result,b'prefix00'+self.new+b'tail');self.assertEqual(audit['previous_generation'],10)
    def test_same_request_rejected(self):
        self.captures[1]['sequence']=1;self.captures[1]['committed_response'].update(active_seq=1,response_seq=1)
        with self.assertRaises(ValueError):s.derive(self.raw,UID,self.captures)
    def test_same_file_rejected(self):
        self.captures[1]['reads'][0]['path']=self.captures[0]['reads'][0]['path']
        with self.assertRaises(ValueError):s.derive(self.raw,UID,self.captures)
    def test_crc_evidence_corruption(self):
        self.captures[1]['committed_response']['crc32']^=1
        with self.assertRaises(ValueError):s.derive(self.raw,UID,self.captures)
    def test_proof_revalidated(self):
        _,audit=s.derive(self.raw,UID,self.captures);proof=dict(schema=1,kind='CFWRIDE_COMMITTED_JOURNAL_DELTA',uid_words=UID,captures=self.captures,audit=audit)
        path=self.p/'proof.json';path.write_text(json.dumps(proof));self.assertEqual(s.apply_proof(self.raw,UID,path)[0],b'prefix00'+self.new+b'tail')
        audit['new_generation']+=1;path.write_text(json.dumps(proof))
        with self.assertRaises(ValueError):s.apply_proof(self.raw,UID,path)

if __name__=='__main__':unittest.main()
