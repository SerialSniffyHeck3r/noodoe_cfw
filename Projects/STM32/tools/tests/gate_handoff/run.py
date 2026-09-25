"""Run the actual gate ELF's mailbox publisher and sealed handoff in ARM.

No physical I/O is stubbed: these functions operate on internal SRAM only.
The old publisher loses journal sequence/attempts because Request rejects the
CRC after those fields were edited. Preserve that exact integration regression.
"""
from pathlib import Path
import argparse,json,struct,subprocess,sys,zlib
P=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(P/'tools'))
sys.path.insert(0,str(P.parents[1]/'.tools/analysis-python'))
from recovery_gate_build import TC
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_XPSR,UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_PC
p=argparse.ArgumentParser();p.add_argument('--elf',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
data=a.elf.read_bytes()
syms={v[3]:int(v[0],16)for line in subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),'-S',str(a.elf)],text=True).splitlines()if len(v:=line.split())==4}
u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS);u.mem_map(0x08010000,0x10000);u.mem_map(0x20000000,0x30000)
ph=struct.unpack_from('<I',data,28)[0];ps,pn=struct.unpack_from('<HH',data,42)
for n in range(pn):
    kind,off,va,_,size,_,_,_=struct.unpack_from('<8I',data,ph+n*ps)
    if kind==1 and size:u.mem_write(va,data[off:off+size])
stop=0x0801fff0
def call(name,*args):
    u.reg_write(UC_ARM_REG_SP,0x2002f000);u.reg_write(UC_ARM_REG_LR,stop|1);u.reg_write(UC_ARM_REG_XPSR,0x1000000)
    for reg,value in zip((UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2),args):u.reg_write(reg,value)
    u.emu_start(syms[name]|1,stop,count=1000000)
    assert u.reg_read(UC_ARM_REG_PC)==stop
    return u.reg_read(UC_ARM_REG_R0)
results=[]
for reason,sequence,attempts in ((0,2,1),(0,0xffffffff,3),(1,4,2),(1,0,0),(2,10,0),(3,19,1),(4,23,2)):
    call('Mailbox',reason,sequence,attempts)
    raw=bytes(u.mem_read(syms['g_recovery_mailbox'],32));w=struct.unpack('<8I',raw)
    ok=(w[:6]==(0x31455447,1,reason,sequence,attempts,0)and w[6]==zlib.crc32(raw[:24])and w[7]==(w[6]^0xffffffff))
    results.append(dict(reason=reason,sequence=sequence,attempts=attempts,actual=list(w),passed=ok))
result=dict(elf=str(a.elf.resolve()),passed=all(x['passed']for x in results),hardware=False,cases=results)
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
if not result['passed']:raise SystemExit(1)
