"""Actual ARM Gregorian code checked against Python's independent datetime calendar."""
from pathlib import Path
import datetime as dt,hashlib,json,random,struct,subprocess,sys
H=Path(__file__).resolve().parent;P=H.parents[2]
sys.path.insert(0,str(P.parents[1]/'.tools/analysis-python'))
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_LR,UC_ARM_REG_SP,UC_ARM_REG_PC,UC_ARM_REG_XPSR
TC=Path('C:/ST/STM32CubeIDE_1.18.1/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344/tools/bin')
O=H/'output';O.mkdir(exist_ok=True)
(O/'test.ld').write_text('MEMORY { FLASH(rx): ORIGIN=0x10000000,LENGTH=64K\n RAM(rwx): ORIGIN=0x20000000,LENGTH=64K }\n SECTIONS { .text:{ *(.text*) *(.rodata*) } > FLASH\n .data:{ *(.data*) } > RAM\n .bss(NOLOAD):{ *(.bss*) *(COMMON) } > RAM }'.replace('ORIGIN=','ORIGIN = ').replace(',LENGTH=',', LENGTH = ').replace('.text:', '.text : ').replace('.data:', '.data : ').replace('.bss(NOLOAD):','.bss (NOLOAD) : '))
source=P/'Drivers/BSP/src/BSP_Calendar.c';reports=[]
for opt in ('O0','Os'):
 elf=O/f'{opt}.elf';cmd=[str(TC/'arm-none-eabi-gcc.exe'),'-mcpu=cortex-m4','-mthumb','-std=c11','-'+opt,'-ffreestanding','-fno-builtin','-nostdlib','-Wall','-Wextra','-Werror','-I'+str(P/'Drivers/BSP/inc'),str(source),'-T',str(O/'test.ld'),'-Wl,-e,BSP_Calendar_Weekday','-lgcc','-o',str(elf)]
 r=subprocess.run(cmd,capture_output=True,text=True);assert r.returncode==0,r.stderr
 nm=subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),str(elf)],text=True);syms={x[2]:int(x[0],16) for line in nm.splitlines() if len(x:=line.split())==3}
 u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS);u.mem_map(0x10000000,0x10000);u.mem_map(0x20000000,0x10000)
 data=elf.read_bytes();off=struct.unpack_from('<I',data,28)[0];stride,count=struct.unpack_from('<HH',data,42)
 for i in range(count):
  kind,offset,va,pa,filesz,memsz,flags,align=struct.unpack_from('<8I',data,off+i*stride)
  if kind==1 and filesz:u.mem_write(va,data[offset:offset+filesz])
 u.reg_write(UC_ARM_REG_XPSR,0x1000000);assertions=0
 def call(name,*args):
  u.reg_write(UC_ARM_REG_SP,0x2000F000);u.reg_write(UC_ARM_REG_LR,0x1000fff1)
  for reg,value in zip((UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3),args):u.reg_write(reg,value&0xffffffff)
  u.emu_start(syms[name]|1,0x1000fff0,count=100000);assert u.reg_read(UC_ARM_REG_PC)==0x1000fff0
  return u.reg_read(UC_ARM_REG_R0)
 for year in range(1,10000):
  feb=(dt.date(year,3,1)-dt.timedelta(days=1)).day
  assert call('BSP_Calendar_DaysInMonth',year,2)==feb;assertions+=1
  for month,day in ((1,1),(2,feb),(3,1),(12,31)):
   assert call('BSP_Calendar_Weekday',year,month,day)==dt.date(year,month,day).isoweekday(),(year,month,day);assertions+=1
 rng=random.Random(41);vectors=[(dt.datetime(2000,2,28,23,59,59),1),(dt.datetime(2100,2,28,23,59,59),1),(dt.datetime(2400,2,28,23,59,59),1),(dt.datetime(2099,12,31,23,59,59),1),(dt.datetime(2024,3,1),-1),(dt.datetime(1,1,1),-1),(dt.datetime(9999,12,31,23,59,59),1),(dt.datetime(2000,1,1),-(1<<63)),(dt.datetime(2000,1,1),(1<<63)-1)]
 for _ in range(1000):vectors.append((dt.datetime(rng.randrange(2,9999),rng.randrange(1,13),rng.randrange(1,29),rng.randrange(24),rng.randrange(60),rng.randrange(60)),rng.randrange(-100000000,100000001)))
 for date,delta in vectors:
  u.mem_write(0x20000000,struct.pack('<7I',date.year,date.month,date.day,0,date.hour,date.minute,date.second));u.mem_write(0x20000100,b'\xA5'*28)
  u.mem_write(0x2000F000,struct.pack('<I',0x20000100));ok=call('BSP_Calendar_AddSeconds',0x20000000,0,delta&0xffffffff,(delta>>32)&0xffffffff)
  try:expected=date+dt.timedelta(seconds=delta)
  except OverflowError:expected=None
  if expected is None:assert ok==0 and bytes(u.mem_read(0x20000100,28))==b'\xA5'*28
  else:assert ok==1 and struct.unpack('<7I',u.mem_read(0x20000100,28))==(expected.year,expected.month,expected.day,expected.isoweekday(),expected.hour,expected.minute,expected.second),(date,delta)
  assertions+=1
 for year,month,day in ((0,1,1),(10000,1,1),(2025,2,29),(2100,2,29),(2026,0,1),(2026,13,1),(2026,4,31)):
  assert call('BSP_Calendar_Weekday',year,month,day)==0;assertions+=1
 reports.append({'optimization':opt,'status':'pass','assertions':assertions,'reference':'Python datetime years1..9999, leap/month/weekday + signed arithmetic','source_sha256':hashlib.sha256(source.read_bytes()).hexdigest()});print(json.dumps(reports[-1]),flush=True)
(O/'results.json').write_text(json.dumps(reports,indent=2)+'\n')
