"""Check production resource work spans and app-facing counters on ARM."""
from pathlib import Path
import json,struct,subprocess,sys
H=Path(__file__).resolve().parent;P=H.parents[2];O=H/'resource-progress-output';O.mkdir(exist_ok=True)
sys.path[:0]=[str(P/'tools'),str(P.parents[1]/'.tools/analysis-python')]
from bootstrap_build import TC
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_XPSR,UC_ARM_REG_PC,UC_ARM_REG_R0
(O/'test.ld').write_text('MEMORY { FLASH(rx) : ORIGIN = 0x10000000, LENGTH = 128K\n RAM(rwx) : ORIGIN = 0x20000000, LENGTH = 192K }\n SECTIONS { .text : { *(.text*) *(.rodata*) } > FLASH\n .data : { *(.data*) } > RAM\n .bss (NOLOAD) : { *(.bss*) *(COMMON) } > RAM\n /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) } }')
rows=[]
for opt in ('O0','Os'):
 elf=O/(opt+'.elf')
 args=[str(TC/'arm-none-eabi-gcc.exe'),'-mcpu=cortex-m4','-mthumb','-'+opt,'-ffunction-sections','-fdata-sections','-nostartfiles','--specs=nano.specs','--specs=nosys.specs','-Wall','-Wextra','-Werror']
 for inc in ('Middlewares/Noodoe/Storage/inc','Middlewares/Noodoe/Update/inc','RecoveryGate/include'):args+=['-I',str(P/inc)]
 args += [str(H/'resource_progress.c'),'-T',str(O/'test.ld'),'-Wl,--gc-sections,-e,ResourceProgress','-o',str(elf)]
 r=subprocess.run(args,text=True,capture_output=True);assert not r.returncode,r.stderr
 symbols={f[2]:int(f[0],16) for line in subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),str(elf)],text=True).splitlines() if len(f:=line.split())==3}
 data=elf.read_bytes();phoff=struct.unpack_from('<I',data,28)[0];phsize,count=struct.unpack_from('<HH',data,42)
 u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS);u.mem_map(0x10000000,0x20000);u.mem_map(0x20000000,0x30000)
 for i in range(count):
  t,offset,addr,_,size,_,_,_=struct.unpack_from('<8I',data,phoff+i*phsize)
  if t==1 and size:u.mem_write(addr,data[offset:offset+size])
 stop=0x1001fff0;u.reg_write(UC_ARM_REG_SP,0x2002fff0);u.reg_write(UC_ARM_REG_LR,stop|1);u.reg_write(UC_ARM_REG_XPSR,0x1000000)
 u.emu_start(symbols['ResourceProgress']|1,stop,count=1000000)
 assert u.reg_read(UC_ARM_REG_PC)==stop and u.reg_read(UC_ARM_REG_R0)==0
 rows.append({'optimization':opt,'work_spans_and_snapshot_counters':'passed','hardware_access':False})
(O/'results.json').write_text(json.dumps(rows,indent=2));print(json.dumps(rows,indent=2))
