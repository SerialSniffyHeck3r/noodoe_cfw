from pathlib import Path
import sys,subprocess,struct,json
H=Path(__file__).resolve().parent;P=H.parents[2];O=H/'output';O.mkdir(exist_ok=True)
sys.path[:0]=[str(P/'tools'),str(P.parents[1]/'.tools/analysis-python')]
from bootstrap_build import TC
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS
from unicorn.arm_const import *
(O/'test.ld').write_text("MEMORY { FLASH(rx): ORIGIN = 0x10000000, LENGTH = 128K\nRAM(rwx): ORIGIN = 0x20000000, LENGTH = 192K }\nSECTIONS { .text : { *(.text*) *(.rodata*) } > FLASH\n.data : { *(.data*) } > RAM\n.bss(NOLOAD) : { *(.bss*) *(COMMON) } > RAM\n/DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) } }")
results=[]
for opt in ('O0','Os'):
 elf=O/(opt+'.elf')
 sources=[H/'test.c',P/'Middlewares/Noodoe/Update/src/Recovery_Core.c',P/'Middlewares/Noodoe/Update/src/Update_SHA256.c',P/'Middlewares/Noodoe/Storage/src/recovery_store.c']
 subprocess.run([str(TC/'arm-none-eabi-gcc.exe'),str(next(q for q in Path(__file__).resolve().parents if (q/'tools/build.ps1').is_file())/'Drivers/BSP/src/noodoe_crc32.c'),'-I'+str(next(q for q in Path(__file__).resolve().parents if (q/'tools/build.ps1').is_file())/'Drivers/BSP/inc'),'-mcpu=cortex-m4','-mthumb','-'+opt,'-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-nostartfiles','--specs=nosys.specs',*['-I'+str(P/x)for x in ('Middlewares/Noodoe/Update/inc','Middlewares/Noodoe/Storage/inc')],*map(str,sources),'-T',str(O/'test.ld'),'-Wl,--gc-sections,-e,test_main','-o',str(elf)],check=True)
 syms={w[2]:int(w[0],16)for l in subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),str(elf)],text=True).splitlines()if len(w:=l.split())==3}
 u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
 for a,n in ((0x10000000,0x20000),(0x20000000,0x30000),(0x08000000,0x10000)):u.mem_map(a,n)
 u.mem_write(0x08000000,(P.parents[1]/'VERY_IMPORTANT_ORIGINAL_NOODOE_BOOTLOARDER/noodoe_full_flash_dump_A.bin').read_bytes()[:65536])
 d=elf.read_bytes();off=struct.unpack_from('<I',d,28)[0];size,count=struct.unpack_from('<HH',d,42)
 for i in range(count):
  kind,pos,va,_,n,_,_,_=struct.unpack_from('<8I',d,off+i*size)
  if kind==1 and n:u.mem_write(va,d[pos:pos+n])
 stop=0x1001fff0;u.reg_write(UC_ARM_REG_SP,0x2002fff0);u.reg_write(UC_ARM_REG_LR,stop|1);u.reg_write(UC_ARM_REG_XPSR,0x01000000)
 u.reg_write(UC_ARM_REG_PC,syms['test_main']|1)
 for turn in range(30):
  u.emu_start(u.reg_read(UC_ARM_REG_PC)|1,stop,count=30000000,timeout=2000000)
  if u.reg_read(UC_ARM_REG_PC)==stop:break
 assert u.reg_read(UC_ARM_REG_PC)==stop
 row=dict(optimization=opt,failure_line=u.reg_read(UC_ARM_REG_R0),assertions=struct.unpack('<I',u.mem_read(syms['assertions'],4))[0],hardware=False,synthetic015=True)
 print(row);results.append(row);assert not row['failure_line']
(O/'results.json').write_text(json.dumps(results,indent=2))
