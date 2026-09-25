"""Execute production stock-recovery core/buttons on ARM; no hardware I/O."""
import hashlib,json,struct,subprocess,sys
from pathlib import Path
HERE=Path(__file__).resolve().parent;P=HERE.parents[2]
sys.path.insert(0,str(P.parents[1]/'.tools/analysis-python'))
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_LR,UC_ARM_REG_PC,UC_ARM_REG_R0,UC_ARM_REG_SP,UC_ARM_REG_XPSR
TC=Path('C:/ST/STM32CubeIDE_1.18.1/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344/tools/bin')
out=HERE/'output';out.mkdir(exist_ok=True)
stock=(P.parents[1]/'VERY_IMPORTANT_ORIGINAL_NOODOE_BOOTLOARDER/noodoe_full_flash_dump_A.bin').read_bytes()[0x10000:]
assert hashlib.sha256(stock).hexdigest()=='162faeecc64bf56d828570168785e66d1a7da0152ef912bf83f9a19094d91edf'
ld=out/'test.ld';ld.write_text('MEMORY { FLASH(rx): ORIGIN = 0x10000000, LENGTH = 128K\nRAM(rwx): ORIGIN = 0x20000000, LENGTH = 128K }\nSECTIONS { .text : { *(.text*) *(.rodata*) } > FLASH\n.data : { *(.data*) } > RAM\n.bss(NOLOAD) : { *(.bss*) *(COMMON) } > RAM\n/DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) } }\n')
sources=[HERE/'test_recovery.c',P/'Middlewares/Noodoe/Update/src/Recovery_Core.c',P/'Middlewares/Noodoe/Update/src/Update_SHA256.c',P/'App_Logic/Recovery/src/Recovery_Buttons.c']
reports=[]
for opt in ['O0','Os']:
 elf=out/f'{opt}.elf';cmd=[str(TC/'arm-none-eabi-gcc.exe'),str(next(q for q in Path(__file__).resolve().parents if (q/'tools/build.ps1').is_file())/'Drivers/BSP/src/noodoe_crc32.c'),'-I'+str(next(q for q in Path(__file__).resolve().parents if (q/'tools/build.ps1').is_file())/'Drivers/BSP/inc'),'-mcpu=cortex-m4','-mthumb','-std=c11','-'+opt,'-Wall','-Wextra','-Werror','-ffreestanding','-fno-builtin','-nostdlib','-I'+str(P/'Middlewares/Noodoe/Update/inc'),'-I'+str(P/'App_Logic/Recovery/inc'),*map(str,sources),'-T',str(ld),'-Wl,-e,RecoveryTest_Main','-lgcc','-o',str(elf)]
 r=subprocess.run(cmd,capture_output=True,text=True);(out/f'{opt}-build.log').write_text(r.stdout+r.stderr)
 if r.returncode:raise RuntimeError(r.stderr)
 nm=subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),str(elf)],text=True);symbols={p[2]:int(p[0],16)for line in nm.splitlines()if len(p:=line.split())==3}
 u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
 for a,n in [(0x10000000,0x20000),(0x20000000,0x20000),(0x11000000,0x70000),(0x12000000,0x70000)]:u.mem_map(a,n)
 u.mem_write(0x11000000,stock);data=elf.read_bytes();phoff=struct.unpack_from('<I',data,28)[0];phsize,phnum=struct.unpack_from('<HH',data,42)
 for i in range(phnum):
  kind,offset,va,_,size,_,_,_=struct.unpack_from('<8I',data,phoff+i*phsize)
  if kind==1 and size:u.mem_write(va,data[offset:offset+size])
 stop=0x1001fff0;u.reg_write(UC_ARM_REG_SP,0x2001fff0);u.reg_write(UC_ARM_REG_LR,stop|1);u.reg_write(UC_ARM_REG_XPSR,0x01000000)
 u.reg_write(UC_ARM_REG_PC,symbols['RecoveryTest_Main']|1)
 # Resume the same virtual CPU in short slices so concurrent managed builds
 # cannot turn a wall-clock timeout into a reported firmware assertion failure.
 for slice_index in range(180):
  u.emu_start(u.reg_read(UC_ARM_REG_PC)|1,stop,count=50000000,timeout=2000000)
  if u.reg_read(UC_ARM_REG_PC)==stop:break

 row=dict(optimization=opt,returned=u.reg_read(UC_ARM_REG_PC)==stop,failure_line=u.reg_read(UC_ARM_REG_R0) if u.reg_read(UC_ARM_REG_PC)==stop else None,assertions=struct.unpack('<I',u.mem_read(symbols['assertions'],4))[0],hardware=False)
 reports.append(row);print(json.dumps(row),flush=True);(out/'results.json').write_text(json.dumps(reports,indent=2))
 if not row['returned'] or row['failure_line']:raise SystemExit(1)
