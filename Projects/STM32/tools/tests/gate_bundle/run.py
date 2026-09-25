"""Offline initial-container and fail-closed deployment tests; no hardware."""
from pathlib import Path
import hashlib,json,struct,sys,tempfile,unittest,zlib
from unittest.mock import patch
HERE=Path(__file__).resolve().parent;P=HERE.parents[2]
sys.path.insert(0,str(P/'tools'))
import gate_bundle as bundle
import bootstrap_recovery_install as installer
from gate_containers import check_container
from validate_image import InvalidImage
from bootstrap_storage import create_plan
from resource_install import swapped
UID=[1,2,3]
APP=bytearray(b'\xff'*0x60000)
struct.pack_into('<II',APP,0,0x2002ff00,0x08020401)
struct.pack_into('<III',APP,0x200,0x51534352,1,1)
APP[0x20c:0x22c]=hashlib.sha256(b'fixture resources').digest();APP=bytes(APP)
PAYLOAD={5:bundle.image_container(APP,UID,0),6:bundle.image_container(APP,UID,1),7:bundle.initial_journal(APP,UID)}
def reseal(data,offset=0):
    struct.pack_into('<I',data,offset+4088,zlib.crc32(data[offset:offset+4088]));return bytes(data)
class Tests(unittest.TestCase):
    def rejected(self,data,kind=5,uid=UID):
        with self.assertRaises((ValueError,RuntimeError,InvalidImage)):check_container(data,kind,uid)
    def test_exact_formats_and_kinds(self):
        for kind,payload in PAYLOAD.items():
            result=check_container(payload,kind,UID)
            self.assertEqual(result['image_sha256'],hashlib.sha256(APP).hexdigest())
            self.assertEqual(result['generation'],1)
            self.rejected(payload,kind,[1,2,4]);self.rejected(payload[:-1],kind)
        self.rejected(PAYLOAD[5],6);self.rejected(PAYLOAD[6],5)
        self.rejected(PAYLOAD[5],5,[0,0,0]);self.rejected(PAYLOAD[7],5)
    def test_corrupt_image_contracts(self):
        # Recompute record CRC to prove these are semantic checks, not only CRC.
        for off,val in ((8,4095),(12,0x100000),(16,2),(28,0x08010000),
                        (32,0x70000),(36,0),(72,0),(76,2),(80,0),(116,2),
                        (124,1),(128,0),(4092,0)):
            data=bytearray(PAYLOAD[5]);struct.pack_into('<I',data,off,val)
            self.rejected(reseal(data))
        for off in (40,84,4096+101,0x70000,0x7f000+32,0x7f000+4088):
            data=bytearray(PAYLOAD[5]);data[off]^=1;self.rejected(bytes(data))
        for off,val in ((8,1),(12,0),(16,2),(28,2)):
            data=bytearray(PAYLOAD[5]);struct.pack_into('<I',data,0x7f000+off,val)
            self.rejected(reseal(data,0x7f000))
    def test_vector_and_resource_mismatch(self):
        for off,val in ((0,0x20030000),(4,0x08010001),(4,0x08020400),(0x200,0)):
            app=bytearray(APP);struct.pack_into('<I',app,off,val)
            self.rejected(bundle.image_container(bytes(app),UID,0))
    def test_initial_journal_contract(self):
        for off,val in ((8,0),(12,3),(16,1),(20,0),(24,1),(28,1),(96,2),
                        (108,0),(112,1),(116,2),(120,0)):
            data=bytearray(PAYLOAD[7]);struct.pack_into('<I',data,off,val)
            self.rejected(reseal(data),7)
        for off in (64,4096,65535):
            data=bytearray(PAYLOAD[7]);data[off]^=1;self.rejected(bytes(data),7)
    def test_named_prepare_requires_actual_budgets(self):
        with tempfile.TemporaryDirectory() as d:
            f=Path(d)/'container';f.write_bytes(PAYLOAD[5])
            with self.assertRaises((ValueError,RuntimeError,InvalidImage)):
                installer.prepare_payload('gate-a',UID,container=f)
            failed=dict(budget_passed=False,failures=['flash below required reserve'])
            with patch('memory_report.report',return_value=failed):
                with self.assertRaisesRegex((ValueError,RuntimeError,InvalidImage),'memory gate failed'):
                    installer.prepare_payload('gate-a',UID,container=f,product_elf=Path('release'),debug_elf=Path('debug'))
    def test_bundle_failure_does_not_publish(self):
        with tempfile.TemporaryDirectory() as d:
            out=Path(d)/'not-created'
            with patch.object(bundle,'report',return_value=dict(budget_passed=False,failures=['insufficient flash'])):
                with self.assertRaisesRegex((ValueError,RuntimeError,InvalidImage),'Memory gates failed'):
                    bundle.build(Path('absent gate'),Path('absent product'),Path('absent debug'),UID,out)
            self.assertFalse(out.exists())
    def test_full_fat_createplan_preserves_others(self):
        logical=bytearray(0x8000000);struct.pack_into('<H',logical,11,4096);logical[13]=8
        struct.pack_into('<H',logical,14,1);logical[16]=2;struct.pack_into('<H',logical,17,512)
        logical[21]=0xf8;struct.pack_into('<H',logical,22,2);struct.pack_into('<I',logical,32,0x7f80)
        logical[510:512]=b'\x55\xaa';logical[0x1000:0x1003]=logical[0x3000:0x3003]=b'\xf8\xff\xff'
        raw=bytes(swapped(logical));del logical
        for kind,data in PAYLOAD.items():
            with self.assertRaises((ValueError,RuntimeError,InvalidImage)):create_plan(raw,kind,data)
            plan,after=create_plan(raw,kind,data,uid=UID)
            self.assertEqual(after[0x07f70000:],raw[0x07f70000:])
            self.assertEqual(bytes(swapped(after[plan['address']:plan['address']+len(data)])),data)
            with self.assertRaises((ValueError,RuntimeError,InvalidImage)):create_plan(after,kind,data,uid=UID)
            raw=after
    def test_actual_current_elf_gate_rejects_before_output(self):
        release=P/'Release/FuckNudo_Noodoe_CFW_Project.elf'
        debug=P/'Debug/FuckNudo_Noodoe_CFW_Project.elf'
        if not release.exists():self.skipTest('Managed Release build unavailable')
        from memory_report import report
        reports=[report(release,'Product','Release')]
        if reports[0]['budget_passed']:
            if not debug.exists():self.skipTest('Release passes but Debug has no linked ELF')
            reports.append(report(debug,'Product','Debug'))
        if all(r['budget_passed'] for r in reports):self.skipTest('Both actual budgets now pass; failure injection above remains valid')
        with tempfile.TemporaryDirectory() as d:
            out=Path(d)/'not-created';source=Path(d)/'fixture.dat';source.write_bytes(PAYLOAD[5])
            with self.assertRaisesRegex((ValueError,RuntimeError,InvalidImage),'Memory gates failed'):
                bundle.build(Path('gate intentionally unused'),release,debug,UID,out)
            self.assertFalse(out.exists())
            with self.assertRaisesRegex((ValueError,RuntimeError,InvalidImage),'memory gate failed'):
                installer.prepare_payload('gate-a',UID,container=source,product_elf=release,debug_elf=debug)
        self.actual_reports=reports
if __name__=='__main__':
    result=unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(Tests))
    output=HERE/'output';output.mkdir(exist_ok=True)
    (output/'results.json').write_text(json.dumps(dict(tests=result.testsRun,failures=len(result.failures),errors=len(result.errors),skipped=len(result.skipped),hardware_access=False),indent=2)+'\n')
    raise SystemExit(not result.wasSuccessful())
