"""Execute the actual Diagnostic ELF's early Gate handoff, without hardware."""
from pathlib import Path
import argparse,json,struct,subprocess,sys,zlib
P=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(P.parents[1]/'.tools/analysis-python'))
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS,UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_LR,UC_ARM_REG_PC,UC_ARM_REG_R0,UC_ARM_REG_SP,UC_ARM_REG_XPSR
TC=Path('C:/ST/STM32CubeIDE_1.18.1/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344/tools/bin')
a=argparse.ArgumentParser();a.add_argument('--elf',type=Path,required=True);args=a.parse_args()
data=args.elf.read_bytes()
nm=subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),str(args.elf)],text=True)
symbols={v[2]:int(v[0],16) for l in nm.splitlines() if len(v:=l.split())==3}
def seal(words):
 b=struct.pack('<6I',*words);c=zlib.crc32(b);return b+struct.pack('<II',c,(~c)&0xffffffff)
rows=[]
for scenario in ['diagnostic','product','trial','unknown-flag','wrong-epoch','bad-request-crc','bad-context-crc']:
 u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
 for addr,size in [(0x08000000,0x80000),(0x20000000,0x30000),(0x10000000,0x10000)]:u.mem_map(addr,size)
 phoff=struct.unpack_from('<I',data,28)[0];phsize,phnum=struct.unpack_from('<HH',data,42)
 for i in range(phnum):
  kind,off,va,pa,n,_,_,_=struct.unpack_from('<8I',data,phoff+i*phsize)
  if kind==1 and n:u.mem_write(va,data[off:off+n])
 request=bytearray(seal([0x31455447,1,0,43,1,0]))
 flag={'product':0,'trial':1,'unknown-flag':32}.get(scenario,16)
 context=bytearray(seal([0x32425447,1,44 if scenario=='wrong-epoch' else 43,flag,0xd1a60900,10]))
 if scenario=='bad-request-crc':request[24]^=1
 if scenario=='bad-context-crc':context[24]^=1
 u.mem_write(0x2002ff00,bytes(request));u.mem_write(0x2002ff60,bytes(context))
 stop=0x0807fff0;u.reg_write(UC_ARM_REG_SP,0x2002fe00);u.reg_write(UC_ARM_REG_LR,stop|1);u.reg_write(UC_ARM_REG_XPSR,0x01000000)
 u.hook_add(UC_HOOK_CODE,lambda uc,address,size,user:uc.emu_stop(),begin=symbols['Diagnostic_ResetToGate'],end=symbols['Diagnostic_ResetToGate'])
 u.emu_start(symbols['AppRecovery_EarlyRun']|1,stop,count=100000)
 pc=u.reg_read(UC_ARM_REG_PC)
 # Reaching LR is valid; invalid records stop before any reset side effect.
 passed=(pc==stop) if scenario=='diagnostic' else (pc==symbols['Diagnostic_ResetToGate'] and u.reg_read(UC_ARM_REG_R0)==1)
 rows.append(dict(scenario=scenario,passed=passed,pc=hex(pc),hardware=False))
 assert passed,rows[-1]
print(json.dumps(rows,indent=2))
