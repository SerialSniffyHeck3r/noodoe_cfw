"""Actual ARM reader/provision validation against existing read-only NOR-A.

No device access. Builds the expected REC postimage offline; never changes the
capture or declares the still-pending physical A/B acquisition verified.
"""
from pathlib import Path
import argparse,sys,struct,subprocess,json,hashlib,time
HERE=Path(__file__).resolve().parent;P=HERE.parents[2]
sys.path.insert(0,str(P/'tools'));sys.path.insert(0,str(P.parents[1]/'.tools/analysis-python'))
from bootstrap_storage import create_plan
from resource_install import TC
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC,UC_ARM_REG_XPSR

def main():
    a=argparse.ArgumentParser(description=__doc__);a.add_argument('--nor-a',type=Path,required=True)
    a.add_argument('--recovery-file',type=Path,required=True);a.add_argument('--output',type=Path,required=True)
    a.add_argument('--hashes',action='store_true',help='Also execute complete128MiB SHA sweeps and cancellation')
    a.add_argument('--case',help='Run only this exact named case');args=a.parse_args()
    raw=args.nor_a.read_bytes();image=args.recovery_file.read_bytes();assert len(raw)==0x8000000 and len(image)==524288
    plan,after=create_plan(raw,4,image);out=args.output;out.mkdir(parents=True,exist_ok=False)
    sources=[P/'Middlewares/Noodoe/Storage/src/recovery_store.c',P/'Middlewares/Noodoe/Storage/src/bootstrap_storage.c',P/'Middlewares/Noodoe/Update/src/Update_SHA256.c',P/'Middlewares/Noodoe/Update/src/Recovery_Core.c',P/'Middlewares/Noodoe/Resources/src/Resources_Format.c',HERE/'real_nor_test.c']
    ld=out/'test.ld';ld.write_text('MEMORY { FLASH(rx) : ORIGIN = 0x10000000, LENGTH = 256K\n RAM(rwx) : ORIGIN = 0x20000000, LENGTH = 192K }\n SECTIONS { .text : { *(.text*) *(.rodata*) } > FLASH\n .data : { *(.data*) } > RAM\n .bss (NOLOAD) : { *(.bss*) *(COMMON) } > RAM\n /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) } }')
    elf=out/'real_nor.elf';cmd=[str(TC/'arm-none-eabi-gcc.exe'),str(next(q for q in Path(__file__).resolve().parents if (q/'tools/build.ps1').is_file())/'Drivers/BSP/src/noodoe_crc32.c'),'-I'+str(next(q for q in Path(__file__).resolve().parents if (q/'tools/build.ps1').is_file())/'Drivers/BSP/inc'),'-std=c11','-mcpu=cortex-m4','-mthumb','-mfloat-abi=soft','-Wall','-Wextra','-Werror','-Os','-ffreestanding','-fno-builtin','-ffunction-sections','-fdata-sections','-nostdlib']
    for d in ('Storage','Update','Resources'):cmd+=['-I',str(P/f'Middlewares/Noodoe/{d}/inc')]
    cmd+=list(map(str,sources))+['-T',str(ld),'-Wl,--gc-sections,-e,RealNor,-u,FullHash,-u,HashOwnership,-u,HashFinalChunk','-lgcc','-o',str(elf)]
    run=subprocess.run(cmd,capture_output=True,text=True);(out/'compile.log').write_text(run.stdout+run.stderr);assert run.returncode==0,run.stderr
    nm=subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),str(elf)],text=True)
    syms={p[2]:int(p[0],16) for line in nm.splitlines() if len(p:=line.split())==3}
    b=elf.read_bytes();phoff=struct.unpack_from('<I',b,28)[0];phsize,phnum=struct.unpack_from('<HH',b,42);rows=[]
    cases=[('captured-A-missing-REC',raw,4,'RealNor',image),('offline-draft-REC-ready',after,3,'RealNor',image),('hash-ownership',after,0,'HashOwnership',image),
           ('hash-final-chunk',after,0,'HashFinalChunk',hashlib.sha256(after[-4096:]).digest())]
    if args.hashes:
        changed=bytearray(after);changed[-1]^=1;changed=bytes(changed)
        assert hashlib.sha256(after).digest()!=hashlib.sha256(changed).digest()
        cases += [('hash-cancel',after,1,'FullHash',hashlib.sha256(after).digest()),
                  ('hash-entire-NOR',after,0,'FullHash',hashlib.sha256(after).digest()),
                  ('hash-final-byte-mutation',changed,0,'FullHash',hashlib.sha256(changed).digest())]
    for name,disk,state,function,payload in cases:
        if args.case and name!=args.case:continue
        print('Starting',name,flush=True)
        u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
        for address,n in ((0x10000000,0x40000),(0x20000000,0x30000),(0x90000000,0x8000000),(0xc0000000,0x100000),(0x11000000,0x100000)):u.mem_map(address,n)
        for i in range(phnum):
            t,off,va,_,size,_,_,_=struct.unpack_from('<8I',b,phoff+i*phsize)
            if t==1 and size:u.mem_write(va,b[off:off+size])
        u.mem_write(0x90000000,disk);u.mem_write(0x11000000,payload)
        stop=0x1003fff0;u.reg_write(UC_ARM_REG_SP,0x2002fff0);u.reg_write(UC_ARM_REG_LR,stop|1)
        u.reg_write(UC_ARM_REG_R0,state);u.reg_write(UC_ARM_REG_XPSR,0x1000000);start=time.monotonic()
        pc=syms[function]|1
        while True:
            u.emu_start(pc,stop,count=1_000_000_000,timeout=30_000_000)
            pc=u.reg_read(UC_ARM_REG_PC)
            if pc==stop:break
            progress=struct.unpack('<I',u.mem_read(syms['test_hash_progress'],4))[0]
            print(name,'progress',progress,'elapsed',round(time.monotonic()-start,1),flush=True)
            assert time.monotonic()-start<(1500 if function=='FullHash' else 120),(name,'bounded runtime exceeded',progress)
            pc|=1
        assert u.reg_read(UC_ARM_REG_PC)==stop,(name,'timeout');assert u.reg_read(UC_ARM_REG_R0)==0,(name,'line',u.reg_read(UC_ARM_REG_R0))
        assert bytes(u.mem_read(0x90000000,0x8000000))==disk,'Read-only validation mutated disk'
        row=dict(case=name,assertions=struct.unpack('<I',u.mem_read(syms['test_assertions'],4))[0],seconds=time.monotonic()-start)
        rows.append(row);print(row,flush=True)
    result=dict(cases=rows,source_A_sha256=hashlib.sha256(raw).hexdigest(),REC_sha256=hashlib.sha256(image).hexdigest(),uid_words=list(struct.unpack_from('<3I',image,16)),draft_plan=plan,
        caveat='Offline ARM execution against captured NOR-A and computed postimage only. Independent physical B, installation and radio behavior not validated.')
    (out/'results.json').write_text(json.dumps(result,indent=2))

if __name__=='__main__':main()
