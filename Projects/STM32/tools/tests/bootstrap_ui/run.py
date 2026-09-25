"""Execute UI, local cancellation and the shared IGN gesture as actual ARM C."""
from pathlib import Path
import sys,struct,subprocess,json
H=Path(__file__).resolve().parent;P=H.parents[2];O=H/'output';O.mkdir(exist_ok=True)
sys.path[:0]=[str(P/'tools'),str(P.parents[1]/'.tools/analysis-python')]
from bootstrap_build import TC
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC,UC_ARM_REG_R0,UC_ARM_REG_XPSR
ld=O/'test.ld';ld.write_text('''MEMORY { FLASH(rx): ORIGIN = 0x10000000, LENGTH = 128K
RAM(rwx): ORIGIN = 0x20000000, LENGTH = 128K }
SECTIONS { .text : { *(.text*) *(.rodata*) } > FLASH
.data : { *(.data*) } > RAM
.bss(NOLOAD) : { *(.bss*) *(COMMON) end = .; } > RAM
/DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) } }''')
rows=[]
for opt in ('O0','Os'):
 elf=O/(opt+'.elf')
 sources=[H/'test.c',H/'session_test.c',P/'RecoveryGate/src/gate_menu.c',H/'screen_test.c',P/'Graphics/UI/src/bootstrap_screen.c',P/'App_Logic/Bootstrap/src/bootstrap_ui.c',P/'App_Logic/Bootstrap/src/bootstrap_confirm.c',P/'App_Logic/Bootstrap/src/bootstrap_target.c',P/'RecoveryGate/src/gate_policy.c',P/'Middlewares/Noodoe/Update/src/Update_Service.c',P/'Middlewares/Noodoe/Update/src/Update_SHA256.c']
 cmd=[str(TC/'arm-none-eabi-gcc.exe'),str(next(q for q in Path(__file__).resolve().parents if (q/'tools/build.ps1').is_file())/'Drivers/BSP/src/noodoe_crc32.c'),'-I'+str(next(q for q in Path(__file__).resolve().parents if (q/'tools/build.ps1').is_file())/'Drivers/BSP/inc'),'-DNOODOE_BOOTSTRAP=1','-I'+str(P/'Graphics/UI/inc'),'-mcpu=cortex-m4','-mthumb','-'+opt,'-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-ffunction-sections','-fdata-sections','-nostartfiles','--specs=nano.specs','--specs=nosys.specs',*['-I'+str(P/x) for x in ('App_Logic/Bootstrap/inc','RecoveryGate/include','Middlewares/Noodoe/Update/inc')],*map(str,sources),'-T',str(ld),'-Wl,--gc-sections','-Wl,-e,TestUI','-Wl,-u,TestCancel','-Wl,-u,TestGesture','-Wl,-u,TestTarget','-Wl,-u,TestProgress','-Wl,-u,TestScreen','-Wl,-u,TestInstallLifetime','-Wl,-u,TestSession','-Wl,-u,TestGateMenu','-Wl,-u,TestRomScale','-o',str(elf)]
 r=subprocess.run(cmd,capture_output=True,text=True);assert not r.returncode,r.stderr
 syms={w[2]:int(w[0],16) for l in subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),str(elf)],text=True).splitlines() if len(w:=l.split())==3}
 data=elf.read_bytes();off=struct.unpack_from('<I',data,28)[0];size,count=struct.unpack_from('<HH',data,42)
 for name,argument in [(n,0) for n in ('TestUI','TestCancel','TestGesture','TestProgress','TestInstallLifetime','TestSession','TestGateMenu','TestRomScale')]+[('TestTarget',n) for n in range(9)]+[('TestScreen',n) for n in range(8)]:
  u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
  u.mem_map(0x10000000,0x20000);u.mem_map(0x20000000,0x20000)
  u.mem_map(0x11000000,0x70000)
  fixture=bytearray(b'\xff'*0x70000);struct.pack_into('<II',fixture,0,0x2002ff00,0x08010101)
  struct.pack_into('<II',fixture,0x10000,0x2002ff00,0x08020101)
  struct.pack_into('<III',fixture,0x10200,0x51534352,1,1);u.mem_write(0x11000000,bytes(fixture))
  for i in range(count):
   kind,pos,va,_,n,_,_,_=struct.unpack_from('<8I',data,off+i*size)
   if kind==1 and n:u.mem_write(va,data[pos:pos+n])
  stop=0x1001fff0;u.reg_write(UC_ARM_REG_SP,0x2001fff0);u.reg_write(UC_ARM_REG_LR,stop|1);u.reg_write(UC_ARM_REG_XPSR,0x1000000)
  u.reg_write(UC_ARM_REG_R0,argument);u.emu_start(syms[name]|1,stop,count=300000000)
  assert u.reg_read(UC_ARM_REG_PC)==stop and u.reg_read(UC_ARM_REG_R0)==0,(opt,name,argument,u.reg_read(UC_ARM_REG_R0))
  rows.append(dict(optimization=opt,test=name,argument=argument,passed=True))
(O/'result.json').write_text(json.dumps(rows,indent=2),encoding='utf-8');print(json.dumps(rows,indent=2))
