"""Actual ARM C reader/provisioner against pinned stock bytes and mocked NOR.
Never accesses ST-LINK. Artifacts live under this test's output directory.
"""
from pathlib import Path
import sys,struct,subprocess,json,hashlib,io,zlib
HERE=Path(__file__).resolve().parent;P=HERE.parents[2];sys.path.insert(0,str(P/'tools'));sys.path.insert(0,str(P.parents[1]/'.tools/analysis-python'))
from bootstrap_storage import recovery_image,create_plan
from cfw_storage_install import record
from resource_install import swapped,TC
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC,UC_ARM_REG_XPSR
log_only='--log-only' in sys.argv
OUT=HERE/('log-output' if log_only else 'output');OUT.mkdir(exist_ok=True)
stock=(P.parents[1]/'VERY_IMPORTANT_ORIGINAL_NOODOE_BOOTLOARDER/noodoe_full_flash_dump_A.bin').read_bytes()[0x10000:]
from gate_bundle import image_container,initial_journal,log_container
app=bytearray(b'\xff'*0x60000);struct.pack_into('<II',app,0,0x2002ff00,0x08020401);struct.pack_into('<III',app,0x200,0x51534352,1,1);app[0x20c:0x22c]=hashlib.sha256(b'fixture resources').digest();app=bytes(app)
gate=[image_container(app,[1,2,3],i) for i in range(2)]+[initial_journal(app,[1,2,3])]
image=recovery_image(stock,[1,2,3]);rsc=(P/'Resources/NOODOE.RSC').read_bytes()
from PIL import Image
jpeg=io.BytesIO();Image.new('RGB',(1,1)).save(jpeg,format='JPEG');jpeg=jpeg.getvalue()
pic=bytearray(b'\xff'*1048576);pic[:4096]=record(3,[1,2,3],struct.pack('<2I',0x31465043,1))
for slot in range(3):
    off=65536+slot*327680;pic[off:off+4096]=record(16+slot,[1,2,3],struct.pack('<3I',0x314a5043,len(jpeg),zlib.crc32(jpeg)),1);pic[off+4096:off+4096+len(jpeg)]=jpeg
pic=bytes(pic)
empty=bytearray(0x8000000);struct.pack_into('<H',empty,11,4096);empty[13]=8;struct.pack_into('<H',empty,14,1);empty[16]=2;struct.pack_into('<H',empty,17,512);empty[21]=0xf8;struct.pack_into('<H',empty,22,2);struct.pack_into('<I',empty,32,0x7f80);empty[510:512]=b'\x55\xaa';empty[0x1000:0x1003]=b'\xf8\xff\xff';empty[0x3000:0x3003]=b'\xf8\xff\xff';raw=bytes(swapped(empty))
plan,valid=create_plan(raw,4,image)
sources=[P/'Middlewares/Noodoe/Storage/src/recovery_store.c',P/'Middlewares/Noodoe/Storage/src/bootstrap_storage.c',P/'Middlewares/Noodoe/Storage/src/bootstrap_resources.c',P/'Middlewares/Noodoe/Update/src/Update_SHA256.c',P/'Middlewares/Noodoe/Update/src/Recovery_Core.c',P/'Middlewares/Noodoe/Resources/src/Resources_Format.c',P/'RecoveryGate/src/gate_store.c',P/'RecoveryGate/src/gate_format.c',P/'RecoveryGate/src/gate_policy.c',P/'RecoveryGate/src/event_log.c',HERE/'test.c']
(OUT/'test.ld').write_text('MEMORY { FLASH(rx) : ORIGIN = 0x10000000, LENGTH = 256K\n RAM(rwx) : ORIGIN = 0x20000000, LENGTH = 192K }\n SECTIONS { .text : { *(.text*) *(.rodata*) } > FLASH\n .data : { *(.data*) } > RAM\n .bss (NOLOAD) : { *(.bss*) *(COMMON) } > RAM\n /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) } }')
STOP=0x1003fff0;rows=[]
for opt in ('-O0','-Os'):
    elf=OUT/f'test{opt}.elf'
    args=[str(TC/'arm-none-eabi-gcc.exe'),str(next(q for q in Path(__file__).resolve().parents if (q/'tools/build.ps1').is_file())/'Drivers/BSP/src/noodoe_crc32.c'),'-I'+str(next(q for q in Path(__file__).resolve().parents if (q/'tools/build.ps1').is_file())/'Drivers/BSP/inc'),'-std=c11','-mcpu=cortex-m4','-mthumb','-mfloat-abi=soft','-Wall','-Wextra','-Werror',opt,'-ffreestanding','-fno-builtin','-ffunction-sections','-fdata-sections','-nostdlib']
    for d in ('Storage','Update','Resources'):args+=['-I',str(P/f'Middlewares/Noodoe/{d}/inc')]
    args+=['-I',str(P/'RecoveryGate/include')]
    args+=list(map(str,sources))+['-T',str(OUT/'test.ld'),'-Wl,--gc-sections,-e,Install,-u,Reader,-u,Size,-u,Inspect,-u,Bench,-u,GateReader','-lgcc','-o',str(elf)]
    run=subprocess.run(args,capture_output=True,text=True);(OUT/f'compile{opt}.log').write_text(run.stdout+run.stderr);assert not run.returncode,run.stdout+run.stderr
    nm=subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),str(elf)],text=True);syms={s[2]:int(s[0],16) for line in nm.splitlines() if len(s:=line.split())==3}
    b=elf.read_bytes();phoff=struct.unpack_from('<I',b,28)[0];phsize,phnum=struct.unpack_from('<HH',b,42)
    def machine(disk,payload):
        u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
        for a,n in ((0x10000000,0x40000),(0x20000000,0x30000),(0x90000000,0x8000000),(0xc0000000,0x100000),(0x11000000,0x100000)):u.mem_map(a,n)
        for i in range(phnum):
            t,o,va,_,size,_,_,_=struct.unpack_from('<8I',b,phoff+i*phsize)
            if t==1 and size:u.mem_write(va,b[o:o+size])
        u.mem_write(0x90000000,disk);u.mem_write(0x11000000,payload);return u
    def call(u,name,arg):
        u.reg_write(UC_ARM_REG_SP,0x2002fff0);u.reg_write(UC_ARM_REG_LR,STOP|1);u.reg_write(UC_ARM_REG_R0,arg);u.reg_write(UC_ARM_REG_XPSR,0x1000000)
        u.emu_start(syms[name]|1,STOP,count=2_000_000_000,timeout=60_000_000)
        assert u.reg_read(UC_ARM_REG_PC)==STOP,(name,'timeout');assert not u.reg_read(UC_ARM_REG_R0),(name,'line',u.reg_read(UC_ARM_REG_R0))
        return struct.unpack('<I',u.mem_read(syms['test_assertions'],4))[0]
    cases=[]
    for name,disk,expected in ([] if log_only else [('missing',raw,4),('valid',valid,3)]):
        u=machine(disk,image);cases.append((name,call(u,'Reader',expected)))
    for name,off,value in ([] if log_only else [('FAT mirror',0x3001,1),('UID',plan['address']+17,1),('body hash',plan['address']+4097,1)]):
        damaged=bytearray(valid);damaged[off]^=value;u=machine(bytes(damaged),image);cases.append((name,call(u,'Reader',5)))
    installs=((1,record(1,[1,2,3])+b'\xff'*(131072-4096)),(7,gate[2])) if opt=='-O0' else ((4,image),(0,rsc),(1,record(1,[1,2,3])+b'\xff'*(131072-4096)),(2,record(2,[1,2,3])+b'\xff'*(262144-4096)),(3,pic),(5,gate[0]),(6,gate[1]),(7,gate[2]))
    installs=((8,log_container([1,2,3])),) if log_only else installs+((8,log_container([1,2,3])),)
    for kind,payload in installs:
        print(opt,'install',kind,flush=True)
        u=machine(raw,payload);cases.append(('install-'+str(kind),call(u,'Install',kind)))
        _,expected=create_plan(raw,kind,payload,uid=[1,2,3]);actual=bytes(u.mem_read(0x90000000,0x8000000));assert actual==expected,'Unexpected mutation outside plan'
        if opt=='-Os' or log_only:
            u=machine(expected,payload);cases.append(('inspect-'+str(kind),call(u,'Inspect',kind)))
    if opt=='-Os' and not log_only:
        for kind,payload in ((4,image),(5,gate[0]),(6,gate[1]),(7,gate[2])):
            print(opt,'SWD bench',kind,flush=True);u=machine(raw,payload);cases.append(('SWD bench-'+str(kind),call(u,'Bench',kind)))
            _,expected=create_plan(raw,kind,payload,uid=[1,2,3])
            assert bytes(u.mem_read(0x90000000,0x8000000))==expected,'SWD path mutation differs from exact file plan'
    if opt=='-Os' and not log_only:
        allfiles=raw;gate_plan=None
        for kind,payload in ((0,rsc),(4,image),(5,gate[0]),(6,gate[1]),(7,gate[2])):
            pp,allfiles=create_plan(allfiles,kind,payload,uid=[1,2,3])
            if kind==5:gate_plan=pp
        print(opt,'isolated GateStore',flush=True)
        for name,disk,scenario in [('empty',raw,0),('five files',allfiles,1)]:
            u=machine(disk,gate[0]);cases.append(('GateStore '+name,call(u,'GateReader',scenario)))
        for name,off,scenario in [('bad identity',gate_plan['address']+0x7f000+17,2),('FAT mirror',0x3001,3)]:
            damaged=bytearray(allfiles);damaged[off]^=1
            u=machine(bytes(damaged),gate[0]);cases.append(('GateStore '+name,call(u,'GateReader',scenario)))
    rows.append(dict(optimization=opt,cases=cases,assertions=sum(n for _,n in cases),sources_sha256={str(p.relative_to(P)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}))
    print(rows[-1],flush=True)
(OUT/'results.json').write_text(json.dumps(dict(results=rows,limits='Actual ARM code with memory NOR stubs. No actual RF, power loss, flash timings or full256MiB backup transfer simulated.'),indent=2))
