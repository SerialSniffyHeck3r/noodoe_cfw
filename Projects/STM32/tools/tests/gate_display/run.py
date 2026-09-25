"""Execute the real HSI recovery display init with an asynchronous EVE model.

This verifies polling/failure policy, not real SPI timing or visual quality.
"""
from pathlib import Path
import sys, subprocess, struct, json
H=Path(__file__).resolve().parent;P=H.parents[2];O=H/'output';O.mkdir(exist_ok=True)
sys.path[:0]=[str(P/'tools'),str(P.parents[1]/'.tools/analysis-python')]
from bootstrap_build import TC
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS,UC_HOOK_MEM_WRITE
from unicorn.arm_const import UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC,UC_ARM_REG_R0,UC_ARM_REG_XPSR
(O/'clock.c').write_text('#include <stdint.h>\nstatic uint32_t tick;uint32_t GateBoard_Millis(void){return tick++;}\n')
(O/'test.ld').write_text('''MEMORY { FLASH(rx): ORIGIN = 0x10000000, LENGTH = 64K
 RAM(rwx): ORIGIN = 0x20000000, LENGTH = 128K }
 SECTIONS { .text : { *(.text*) *(.rodata*) } > FLASH
 .data : { *(.data*) } > RAM
 .bss(NOLOAD) : { *(.bss*) *(COMMON) } > RAM
 /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) } }''')
results=[]
for opt in ('O0','Os'):
 elf=O/(opt+'.elf')
 cmd=[str(TC/'arm-none-eabi-gcc.exe'),'-mcpu=cortex-m4','-mthumb','-'+opt,'-DSTM32F429xx','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-nostartfiles','--specs=nosys.specs',
      *['-I'+str(P/x) for x in ('RecoveryGate/include','Drivers/BSP/inc','Middlewares/Noodoe/Update/inc','Drivers/CMSIS/Include','Drivers/CMSIS/Device/ST/STM32F4xx/Include')],str(P/'RecoveryGate/src/gate_display.c'),str(O/'clock.c'),'-T',str(O/'test.ld'),'-Wl,-e,GateBoard_DisplayInit','-o',str(elf)]
 subprocess.run(cmd,check=True,capture_output=True)
 syms={w[2]:int(w[0],16) for l in subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),str(elf)],text=True).splitlines() if len(w:=l.split())==3}
 data=elf.read_bytes();off=struct.unpack_from('<I',data,28)[0];size,count=struct.unpack_from('<HH',data,42)
 for mode,expected in [('ready',0),('panel4',0),('delayed-reset',0),('bad-id',3),('stuck-reset',4),('bad-panel',8)]:
  u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
  for a,n in [(0x10000000,0x10000),(0x20000000,0x20000),(0x08000000,0x10000),(0x40000000,0x30000)]:u.mem_map(a,n)
  for i in range(count):
   kind,pos,va,_,n,_,_,_=struct.unpack_from('<8I',data,off+i*size)
   if kind==1 and n:u.mem_write(va,data[pos:pos+n])
  u.mem_write(0x0800c080,b'\x04\x00' if mode=='panel4' else b'\x03\x00')
  u.mem_write(0x40021c10,struct.pack('<I',12)) # PH3/PH2 high -> strap6
  for a in (0x40013008,0x40013408):u.mem_write(a,struct.pack('<I',3))
  state=dict(tx=[],answer=0,panel=0,cpu_reads=0)
  def write(uc,access,address,n,value,unused):
   if address==0x40020018 and value&(1<<20):state['tx']=[]
   elif address==0x4001300c:
    tx=state['tx'];tx.append(value&255);answer=0
    if len(tx)>=5 and not tx[0]&128:
     a=((tx[0]&63)<<16)|(tx[1]<<8)|tx[2];idx=len(tx)-5
     if a==0x302000:answer=0 if mode=='bad-id' else 0x7c
     elif a==0x302020:
      state['cpu_reads']+=1
      answer=2 if mode=='stuck-reset' or mode=='delayed-reset' and state['cpu_reads']<=24 else 0
     elif a==0x302574:answer=(0xffc>>(8*idx))&255
    state['answer']=answer
   elif address==0x4001340c:state['panel']=0 if mode=='bad-panel' else 0x9c if value==0xa5a5 else 0
  # Supply RX independently from the CPU's preceding DR write.
  from unicorn import UC_HOOK_MEM_READ
  def read(uc,access,address,n,value,unused):
   if address in (0x4001300c,0x4001340c):uc.mem_write(address,struct.pack('<I',state['answer'] if address==0x4001300c else state['panel']))
  u.hook_add(UC_HOOK_MEM_WRITE,write);u.hook_add(UC_HOOK_MEM_READ,read)
  stop=0x1000fff0;u.reg_write(UC_ARM_REG_SP,0x2001fff0);u.reg_write(UC_ARM_REG_LR,stop|1);u.reg_write(UC_ARM_REG_XPSR,0x1000000)
  u.emu_start(syms['GateBoard_DisplayInit']|1,stop,count=10000000)
  d=struct.unpack('<8I',u.mem_read(syms['g_gate_display'],32))
  assert u.reg_read(UC_ARM_REG_PC)==stop
  assert d[2]==expected and bool(d[3])==(expected==0),(opt,mode,d)
  assert bool(u.reg_read(UC_ARM_REG_R0))==bool(expected)
  if mode=='delayed-reset':assert state['cpu_reads']==25
  if mode=='stuck-reset':assert 100<state['cpu_reads']<1000
  results.append(dict(optimization=opt,scenario=mode,passed=True,diagnostics=d,cpu_reset_reads=state['cpu_reads']))
(O/'results.json').write_text(json.dumps(results,indent=2));print(json.dumps(results,indent=2))
