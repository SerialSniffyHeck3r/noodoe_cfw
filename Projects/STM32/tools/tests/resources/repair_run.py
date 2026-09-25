"""Execute reviewed donor repair through production ARM ResourceStore/Repair.
Requires the fixture ELF produced by run.py, never accesses hardware.
"""
from pathlib import Path
import hashlib,json,struct,subprocess,sys
H=Path(__file__).resolve().parent;P=H.parents[2];R=P.parents[1];O=R/'analysis/2026-09-16-storage-root-cause'
sys.path[:0]=[str(P/'tools'),str(R/'.tools/analysis-python')]
from stock_fat_repair import batch_bytes,SAFE_END
from resource_install import make_plan,Fat,TC
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC,UC_ARM_REG_R0,UC_ARM_REG_XPSR
before=(R/'analysis/2026-09-16-memory-optimization/nor-before/A.bin').read_bytes();after=(O/'repair-plan/after.bin').read_bytes();plan=json.loads((O/'repair-plan/plan.json').read_text())
slot=(P/'Resources/slot.bin').read_bytes();reports=[]
for opt in ['Os','O0']:
    elf=H/'output'/(opt+'.elf');data=elf.read_bytes();header=struct.unpack_from('<16sHHIIIIIHHHHHH',data)
    nm=subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),str(elf)],text=True);syms={w[2]:int(w[0],16) for l in nm.splitlines() if len(w:=l.split())==3}
    assert 'ResourceRepair_Process' in syms,'Rebuild resources/run.py first'
    def machine(media,payload):
        u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
        for a,n in [(0x10000000,0x80000),(0x20000000,0x40000),(0x90000000,0x8000000),(0xa0000000,0x80000),(0xc0000000,0x4000000)]:u.mem_map(a,n)
        u.mem_write(0x90000000,media);u.mem_write(0xa0000000,bytes(payload))
        for i in range(header[10]):
            kind,off,a,_,n,_,_,_=struct.unpack_from('<8I',data,header[5]+i*header[9])
            if kind==1 and n:u.mem_write(a,data[off:off+n])
        u.reg_write(UC_ARM_REG_SP,0x2003fff0);u.reg_write(UC_ARM_REG_XPSR,0x1000000);return u
    def word(u,name,n=None):
        if n is not None:u.mem_write(syms[name],struct.pack('<I',n))
        return struct.unpack('<I',u.mem_read(syms[name],4))[0]
    def call(u,name):
        u.reg_write(UC_ARM_REG_LR,0x1007fff1);u.emu_start(syms[name]|1,0x1007fff0,count=800000000)
        assert u.reg_read(UC_ARM_REG_PC)==0x1007fff0,'Instruction budget';return u.reg_read(UC_ARM_REG_R0)
    media=before;addresses=plan['changed_sectors']
    for i in range(0,len(addresses),120):
        payload=batch_bytes(before,after,addresses[i:i+120]);u=machine(media,payload);word(u,'desired_command',3)
        assert call(u,'TestInstall')==0,tuple(u.mem_read(syms['g_resource_install'],60))
        media=bytes(u.mem_read(0x90000000,0x8000000))
        v=machine(media,payload);word(v,'desired_command',3);assert call(v,'TestInstall')==0 and word(v,'writes')==0,'Exact replay wrote again'
    assert media==after,'Production repair differs from reviewed image';Fat(media)
    tests=['125-sector exact donor repair','all readable files/hash-validated recoveries preserved','idempotent completed replay no-write']
    # Refuse the entire batch before erase on any malformed record or stale input.
    payload=batch_bytes(before,after,addresses[:2])
    variants={}
    bad=bytearray(payload);bad[16]^=1;variants['batch-sha']=(before,bad)
    for label,mutate in [('reserved-boundary',lambda p:struct.pack_into('<I',p,64,SAFE_END)),('duplicate-sector',lambda p:struct.pack_into('<I',p,64+4164,addresses[0])),('replacement-sha',lambda p:p.__setitem__(64+68,p[64+68]^1))]:
        bad=bytearray(payload);mutate(bad);total=struct.unpack_from('<I',bad,12)[0];bad[16:48]=hashlib.sha256(bad[64:64+total]).digest();variants[label]=(before,bad)
    stale=bytearray(before);stale[addresses[1]]^=1;variants['stale-last-preimage']=(bytes(stale),payload)
    # Even correctly hashed input may not repurpose existing nonblank file data.
    occupied=next(x for x in range(0x9000,SAFE_END,4096) if before[x:x+4096] not in (bytes(4096),b'\xff'*4096))
    replacement=bytearray(before);replacement[occupied]^=1
    variants['nonblank-data-protection']=(before,batch_bytes(before,bytes(replacement),[occupied]))
    for label,(image,prepared) in variants.items():
        v=machine(image,prepared);word(v,'desired_command',3);assert call(v,'TestInstall')!=0 and word(v,'writes')==0,label;tests.append(label+' no-write')
    # A fully repaired image must support unmodified strict resource provisioning.
    resource_plan,expected=make_plan(media,slot);u=machine(media,slot);word(u,'cluster',resource_plan['first_cluster']);word(u,'root_index',resource_plan['root_index'])
    assert call(u,'TestInstall')==0
    assert bytes(u.mem_read(0x90000000,0x8000000))==expected
    assert call(u,'TestLoad')==0;tests.append('repaired FAT -> original installer -> all12 assets READY')
    result={'optimization':opt,'status':'PASS','hardware':False,'tests':tests,'repaired_sha256':hashlib.sha256(media).hexdigest(),'resource_first_cluster':resource_plan['first_cluster']};reports.append(result);(O/'repair-tests.json').write_text(json.dumps(reports,indent=2));print(json.dumps(result),flush=True)
