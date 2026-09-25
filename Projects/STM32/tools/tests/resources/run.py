"""Execute production FAT12, resource loader and commit-last installer on ARM."""
from pathlib import Path
import json,struct,subprocess,sys,zlib
H=Path(__file__).resolve().parent;P=H.parents[2];O=H/'output';O.mkdir(exist_ok=True)
sys.path[:0]=[str(P/'tools'),str(P.parents[1]/'.tools/analysis-python')]
from resource_install import make_plan,check_slot,swapped,Fat,TC,SLOT
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS,UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC,UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_XPSR
raw=(P.parents[1]/'analysis/2026-09-12-integrated-bringup/nor-full-backup-02/A.bin').read_bytes()
slot=(P/'Resources/slot.bin').read_bytes()
# The donor has legacy directory entries whose FAT chains are already broken.
# Preflight must refuse that real image, before any live provisioning write.
try:make_plan(raw,slot)
except ValueError as error:(O/'donor-preflight.json').write_text(json.dumps({'allowed':False,'reason':str(error)},indent=2))
else:raise AssertionError('Expected known donor inconsistency')
# A synthetic FAT12 fixture exercises successful installation without silently
# repairing the donor. One existing40KiB file and reserved/staging bytes survive.
b=swapped(raw);b[0x1000:0x9000]=bytes(0x8000)
for base in [0x1000,0x3000]:
 b[base:base+3]=bytes.fromhex('f8ffff')
 for c,n in [(3000,3001),(3001,0xfff)]:
  off=base+c+c//2;old=struct.unpack_from('<H',b,off)[0];struct.pack_into('<H',b,off,(old&15)|(n<<4) if c&1 else (old&0xf000)|n)
e=bytearray(32);e[:11]=b'EXIST   BIN';e[11]=0x20;struct.pack_into('<H',e,26,3000);struct.pack_into('<I',e,28,40000);b[0x5000:0x5020]=e
raw=bytes(swapped(b));plan,expected=make_plan(raw,slot);start=plan['regions'][1]['offset']
(O/'test.ld').write_text('MEMORY { FLASH(rx): ORIGIN = 0x10000000, LENGTH = 512K\nRAM(rwx): ORIGIN = 0x20000000, LENGTH = 256K }\nSECTIONS { .text : { *(.text*) *(.rodata*) *(.resource_requirement) } > FLASH\n.data : { *(.data*) } > RAM AT>FLASH\n.bss(NOLOAD) : { *(.bss*) *(COMMON) } > RAM\n . = ALIGN(8); end = .;\n/DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) } }')
incs=[H/'stubs',P/'Drivers/BSP/inc',P/'RecoveryGate/include',P/'Middlewares/Third_Party/FatFs/src']+[P/f'Middlewares/Noodoe/{x}/inc' for x in ['Storage','Resources','Update']]
sources=[H/'test.c']+[P/x for x in ['Middlewares/Noodoe/Storage/src/storage_disk.c','Middlewares/Noodoe/Storage/src/StorageService.c','Middlewares/Third_Party/FatFs/src/ff.c','Middlewares/Noodoe/Resources/src/Resources.c','Middlewares/Noodoe/Resources/src/Resources_Format.c','Middlewares/Noodoe/Resources/src/ResourceStore.c','Middlewares/Noodoe/Resources/src/ResourceRepair.c','Middlewares/Noodoe/Update/src/Update_SHA256.c']]
reports=[]
for opt in ['Os','O0']:
    elf=O/(opt+'.elf');cmd=[str(TC/'arm-none-eabi-gcc.exe'),str(next(q for q in Path(__file__).resolve().parents if (q/'tools/build.ps1').is_file())/'Drivers/BSP/src/noodoe_crc32.c'),'-I'+str(next(q for q in Path(__file__).resolve().parents if (q/'tools/build.ps1').is_file())/'Drivers/BSP/inc'),'-mcpu=cortex-m4','-mthumb','-std=c11','-'+opt,'-DNOODOE_PRODUCT=1','-Wall','-Wextra','-ffunction-sections','-fdata-sections','-nostartfiles','--specs=nano.specs','--specs=nosys.specs',*['-I'+str(x) for x in incs],*map(str,sources),'-Wl,-T,'+str(O/'test.ld'),'-Wl,--gc-sections','-Wl,-e,TestInstall','-Wl,--undefined=TestLoad','-Wl,--undefined=TestInit','-Wl,--undefined=TestCompatibility','-Wl,--undefined=TestWire','-o',str(elf),'-lgcc']
    r=subprocess.run(cmd,capture_output=True,text=True);(O/(opt+'.log')).write_text(r.stdout+r.stderr);assert r.returncode==0,r.stderr
    nm=subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),str(elf)],text=True);syms={w[2]:int(w[0],16) for l in nm.splitlines() if len(w:=l.split())==3}
    data=elf.read_bytes();header=struct.unpack_from('<16sHHIIIIIHHHHHH',data)
    def machine(media,prepared=slot):
        u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
        for a,n in [(0x10000000,0x80000),(0x20000000,0x40000),(0x90000000,0x8000000),(0xa0000000,0x80000),(0xc0000000,0x4000000)]:u.mem_map(a,n)
        u.mem_write(0x90000000,bytes(media));u.mem_write(0xa0000000,prepared)
        for i in range(header[10]):
            kind,off,a,_,n,_,_,_=struct.unpack_from('<8I',data,header[5]+i*header[9])
            if kind==1 and n:u.mem_write(a,data[off:off+n])
        u.reg_write(UC_ARM_REG_SP,0x2003fff0);u.reg_write(UC_ARM_REG_XPSR,0x1000000)
        return u
    def setword(u,name,value):u.mem_write(syms[name],struct.pack('<I',value))
    def word(u,name):return struct.unpack('<I',u.mem_read(syms[name],4))[0]
    def call(u,name,arg=0):
        u.reg_write(UC_ARM_REG_R0,arg)
        u.reg_write(UC_ARM_REG_LR,0x1007fff1);u.emu_start(syms[name]|1,0x1007fff0,count=800000000)
        assert u.reg_read(UC_ARM_REG_PC)==0x1007fff0,'Instruction limit '+name
        return u.reg_read(UC_ARM_REG_R0)
    # Refuse writes without a confirmed journal, or while trial/reset owns assets.
    for label,ready,flags in [('no-boot-journal',0,0),('trial',1,1),('reset-pending',1,8)]:
        v=machine(raw);setword(v,'boot_ready',ready);setword(v,'boot_flags',flags)
        setword(v,'cluster',plan['first_cluster']);setword(v,'root_index',plan['root_index'])
        assert call(v,'TestInstall')==23 and not word(v,'writes'),label
    u=machine(raw);setword(u,'cluster',plan['first_cluster']);setword(u,'root_index',plan['root_index'])
    assert call(u,'TestInstall')==0,struct.unpack('<15I',u.mem_read(syms['g_resource_install'],60))
    actual=bytes(u.mem_read(0x90000000,0x8000000));assert actual==expected,'C installer differs from offline FAT plan'
    assert call(u,'TestLoad')==0
    tests=['no-boot-journal-no-write','trial-no-write','reset-pending-no-write','create-exact-diff','all18-assets','1000-idempotent-ready-requests']
    # Independently record every physical erase/program in a real inactive update.
    v=machine(actual);assert call(v,'TestLoad')==0;setword(v,'desired_slot',1);setword(v,'desired_command',2)
    operations=[]
    def write_hook(uc,address,size,_):
        a=uc.reg_read(UC_ARM_REG_R0)
        if address==syms['BSP_NOR_Erase4K']:operations.append((a,b'\xff'*4096,True))
        else:operations.append((a,bytes(uc.mem_read(uc.reg_read(UC_ARM_REG_R1),uc.reg_read(UC_ARM_REG_R2))),False))
    for fn in ['BSP_NOR_Erase4K','BSP_NOR_Program']:v.hook_add(UC_HOOK_CODE,write_hook,begin=syms[fn],end=syms[fn])
    assert call(v,'TestInstall')==0
    two=bytes(v.mem_read(0x90000000,0x8000000));assert check_slot(swapped(two[start+SLOT:start+2*SLOT]))==slot[16:48]
    require_a=actual[start:start+SLOT];sim=bytearray(actual);power_cases=0
    for at,content,erase in operations:
        sim[at:at+len(content)]=content
        assert sim[start:start+SLOT]==require_a,'Active slot overwritten'
        # Every loss boundary retains the byte-identical valid A slot.
        check_slot(swapped(sim[start:start+SLOT]));power_cases+=1
    assert bytes(sim)==two,'Physical trace differs from final NOR'
    tests+=['inactive-update-commit-last',f'{power_cases}-power-loss-boundaries-preserve-A']
    # Backward compatibility is selected by the old APP's exact asset SHA,
    # not by this build's18-entry count. Retain its first12 entries unchanged.
    import hashlib
    old=bytearray(slot);_,last,n,_=struct.unpack_from('<4I',old,48+11*16);old_total=last+n
    struct.pack_into('<2I',old,8,old_total,12)
    old[48+12*16:4088]=b'\xff'*(4088-48-12*16)
    old[4096+old_total:]=b'\xff'*(SLOT-4096-old_total)
    old[16:48]=hashlib.sha256(old[48:48+12*16]+old[4096:4096+old_total]).digest()
    struct.pack_into('<I',old,4088,zlib.crc32(old[:4088]));check_slot(old)
    v=machine(actual,bytes(old));assert call(v,'TestLoad')==0
    setword(v,'desired_slot',1);setword(v,'desired_command',2);assert call(v,'TestInstall')==0
    assert call(v,'TestCompatibility')==0
    tests.append('old12-entry-APP-resource-compatibility-with-new18-entry-loader')
    # A future APP can require a different asset ID installed in B, while A
    # remains the current immutable font/patch arena until explicit reboot.
    import hashlib
    variant=bytearray(slot);variant[4096]^=1
    ident,off,n,_=struct.unpack_from('<4I',variant,48);struct.pack_into('<I',variant,60,zlib.crc32(variant[4096+off:4096+off+n]))
    total,count=struct.unpack_from('<2I',variant,8);variant[16:48]=hashlib.sha256(variant[48:48+count*16]+variant[4096:4096+total]).digest()
    struct.pack_into('<I',variant,4088,zlib.crc32(variant[:4088]));check_slot(variant)
    v=machine(actual,bytes(variant));assert call(v,'TestLoad')==0
    setword(v,'desired_slot',1);setword(v,'desired_command',2);assert call(v,'TestInstall')==0
    assert call(v,'TestCompatibility')==0;assert struct.unpack('<I',v.mem_read(syms['g_resources']+8,4))[0]==4
    tests.append('future-APP-matching-inactive-resource-ID')
    for cancel in [0,1,2]:
        v=machine(actual,bytes(variant));result=call(v,'TestWire',cancel)
        assert result==0,('wire',cancel,result)
        got=bytes(v.mem_read(0x90000000,0x8000000))
        assert got[:start+SLOT]==actual[:start+SLOT] and got[start+2*SLOT:]==actual[start+2*SLOT:]
        if not cancel:assert check_slot(swapped(got[start+SLOT:start+2*SLOT]))==variant[16:48]
        tests.append('wire-IGN-off-preserves-A-cancel-'+str(cancel))

    cases={}
    # Fresh CPU ensures no prior resource cache can mask a damaged file.
    cases['missing']= (raw,5)
    cases['B-valid-A-header-damaged']=(two[:start]+b'\0'+two[start+1:],4)
    for label,offset in [('payload-crc',4096+13),('header-crc',48),('version',4),('commit',4092)]:
        b=bytearray(actual);b[start+(offset^1)]^=0x80;cases[label]=(bytes(b),5)
    b=bytearray(actual);logical=swapped(b);struct.pack_into('<I',logical,0x5000+plan['root_index']*32+28,65536);cases['truncated-file']=(bytes(swapped(logical)),5)
    for name,(media,state) in cases.items():
        v=machine(media);setword(v,'expected_state',state);result=call(v,'TestLoad');assert result==0,(name,result);assert not word(v,'writes');tests.append(name)
    # Corrupt input cannot unlock or write. Existing files cannot be recreated.
    bad=bytearray(slot);bad[4096]^=1;v=machine(raw,bytes(bad));setword(v,'cluster',plan['first_cluster']);setword(v,'root_index',plan['root_index'])
    assert call(v,'TestInstall')!=0 and not word(v,'writes');tests.append('bad-input-no-write')
    v=machine(actual);setword(v,'cluster',plan['first_cluster']);setword(v,'root_index',plan['root_index']);assert call(v,'TestInstall')!=0 and not word(v,'writes');tests.append('create-existing-no-write')
    reports.append(dict(optimization=opt,tests=tests,status='PASS'));(O/'results.json').write_text(json.dumps(reports,indent=2));print(json.dumps(reports[-1]),flush=True)
