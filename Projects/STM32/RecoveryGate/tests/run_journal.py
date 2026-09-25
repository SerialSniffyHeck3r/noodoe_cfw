"""ARM execution of real gate policy/format/copy core; no hardware."""
from pathlib import Path
import argparse,json,struct,subprocess,sys,time
P=Path(__file__).resolve().parents[2];HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(P.parents[1]/'.tools/analysis-python'))
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_LR,UC_ARM_REG_PC,UC_ARM_REG_R0,UC_ARM_REG_SP,UC_ARM_REG_XPSR
TC=Path('C:/ST/STM32CubeIDE_1.18.1/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344/tools/bin')

out=HERE/'journal-output';out.mkdir(exist_ok=True)
ld=out/'test.ld';ld.write_text('MEMORY { FLASH(rx): ORIGIN = 0x10000000, LENGTH = 128K\nRAM(rwx): ORIGIN = 0x20000000, LENGTH = 128K }\nSECTIONS { .text : { *(.text*) *(.rodata*) } > FLASH\n.data : { *(.data*) } > RAM\n.bss(NOLOAD) : { *(.bss*) *(COMMON) } > RAM\n/DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) } }')
sources=[HERE/'test_journal.c',*[P/'RecoveryGate/src'/f for f in ('gate_policy.c','gate_format.c','gate_main.c')]]
reports=[]
for opt in ('O0','Os'):
 elf=out/(opt+'.elf');cmd=[str(TC/'arm-none-eabi-gcc.exe'),str(next(q for q in Path(__file__).resolve().parents if (q/'tools/build.ps1').is_file())/'Drivers/BSP/src/noodoe_crc32.c'),'-I'+str(next(q for q in Path(__file__).resolve().parents if (q/'tools/build.ps1').is_file())/'Drivers/BSP/inc'),'-mcpu=cortex-m4','-mthumb','-std=c11','-'+opt,'-Wall','-Wextra','-Wno-misleading-indentation','-DGATE_JOURNAL_TESTING','-ffunction-sections','-fdata-sections','-ffreestanding','-fno-builtin','-nostdlib','-I'+str(P/'RecoveryGate/include'),*['-I'+str(P/x) for x in ('Middlewares/Noodoe/Update/inc','Middlewares/Noodoe/Storage/inc','Middlewares/Noodoe/Resources/inc','Drivers/BSP/inc')],*map(str,sources),'-T',str(ld),'-Wl,-e,TestJournal','-Wl,--gc-sections','-lgcc','-o',str(elf)]
 r=subprocess.run(cmd,capture_output=True,text=True);(out/(opt+'-build.log')).write_text(r.stdout+r.stderr)
 if r.returncode:raise RuntimeError(r.stderr)
 nm=subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),str(elf)],text=True);symbols={p[2]:int(p[0],16)for line in nm.splitlines()if len(p:=line.split())==3}
 cases=[('TestJournal',mode) for mode in range(20)]
 for entry,mode in cases:
  u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
  for addr,n in [(0x10000000,0x20000),(0x20000000,0x20000),(0x11000000,0x60000),(0x12000000,0x60000)]:u.mem_map(addr,n)
  data=elf.read_bytes();phoff=struct.unpack_from('<I',data,28)[0];phsize,phnum=struct.unpack_from('<HH',data,42)
  for i in range(phnum):
   kind,offset,va,_,size,_,_,_=struct.unpack_from('<8I',data,phoff+i*phsize)
   if kind==1 and size:u.mem_write(va,data[offset:offset+size])
  source=bytearray((i*37+11)&255 for i in range(0x60000));struct.pack_into('<II',source,0,0x2002ff00,0x08020401);struct.pack_into('<III',source,0x200,0x51534352,1,1);u.mem_write(0x11000000,bytes(source))
  stop=0x1001fff0;u.reg_write(UC_ARM_REG_SP,0x2001fff0);u.reg_write(UC_ARM_REG_LR,stop|1);u.reg_write(UC_ARM_REG_XPSR,0x01000000);u.reg_write(UC_ARM_REG_PC,symbols[entry]|1);u.reg_write(UC_ARM_REG_R0,mode)
  began=time.monotonic()
  for _ in range(240):
   u.emu_start(u.reg_read(UC_ARM_REG_PC)|1,stop,count=50000000,timeout=2000000)
   if u.reg_read(UC_ARM_REG_PC)==stop:break
  row=dict(optimization=opt,entry=entry,cut=mode,returned=u.reg_read(UC_ARM_REG_PC)==stop,failure_line=u.reg_read(UC_ARM_REG_R0),assertions=struct.unpack('<I',u.mem_read(symbols['assertions'],4))[0],seconds=round(time.monotonic()-began,2),hardware=False)
  reports.append(row);print(json.dumps(row),flush=True);(out/'results.json').write_text(json.dumps(reports,indent=2))
  if not row['returned']or row['failure_line']:raise SystemExit(1)
