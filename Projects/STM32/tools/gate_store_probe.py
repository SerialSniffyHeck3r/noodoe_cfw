"""Execute the exact gate ELF FAT walker against a host NOR image, no hardware.

Only physical NOR reads are substituted. MCU SPI/voltage/timing is not tested.
"""
from pathlib import Path
import argparse,json,struct,subprocess,sys
P=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(P.parents[1]/'.tools/analysis-python'))
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS,UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_PC,UC_ARM_REG_LR,UC_ARM_REG_SP,UC_ARM_REG_XPSR
from resource_install import Fat,sha,require,TC
from cfw_storage_install import file_bytes

def probe(elf,raw,uid):
 require(len(raw)==0x8000000 and len(uid)==3,'NOR size/UID')
 fs=Fat(raw);binary=elf.read_bytes()
 listing=subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),'-S',str(elf)],text=True)
 syms={f[3]:(int(f[0],16),int(f[1],16))for row in listing.splitlines()if len(f:=row.split())==4}
 u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS);u.mem_map(0x08010000,0x10000);u.mem_map(0x20000000,0x30000)
 ph=struct.unpack_from('<I',binary,28)[0];ps,pn=struct.unpack_from('<HH',binary,42)
 for i in range(pn):
  kind,off,va,_,n,_,_,_=struct.unpack_from('<8I',binary,ph+i*ps)
  if kind==1 and n:u.mem_write(va,binary[off:off+n])
 store=syms['store'][0];callback=0x0801ffc0;stop=0x0801ffe0;uidptr=0x2002e000;dest=0x20020000
 u.mem_write(uidptr,struct.pack('<3I',*uid));u.mem_write(callback,b'\x70\x47');reads=[]
 def read_hook(uc,pc,size,unused):
  a=uc.reg_read(UC_ARM_REG_R1);out=uc.reg_read(UC_ARM_REG_R2);n=uc.reg_read(UC_ARM_REG_R3)
  require(0<n<=32768 and a+n<=len(raw) and 0x20000000<=out<=0x20030000-n,'ARM read outside allowed memory')
  uc.mem_write(out,raw[a:a+n]);reads.append((a,n));uc.reg_write(UC_ARM_REG_R0,0);uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
 u.hook_add(UC_HOOK_CODE,read_hook,begin=callback,end=callback)
 def call(name,*args):
  u.reg_write(UC_ARM_REG_SP,0x2002f000);u.reg_write(UC_ARM_REG_LR,stop|1);u.reg_write(UC_ARM_REG_XPSR,0x01000000)
  for reg,value in zip((UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3),args):u.reg_write(reg,value)
  if len(args)>4:u.mem_write(0x2002f000,struct.pack('<I',args[4]))
  u.emu_start(syms[name][0]|1,stop,count=10000000,timeout=2000000)
  require(u.reg_read(UC_ARM_REG_PC)==stop,'ARM gate call did not return: '+name)
  return u.reg_read(UC_ARM_REG_R0)
 call('GateStore_Init',store,callback|1,0,uidptr)
 for count in range(10000):
  state=call('GateStore_Process',store)
  if state:break
 fields=struct.unpack('<15I',u.mem_read(store,60));require(state==1 and fields[6]==0 and fields[13:15]==(31,3),'Gate audit failed: '+repr(fields))
 records=[]
 for i,name in enumerate(('CFWA.DAT','CFWB.DAT','CFWBOOT.DAT','CFWREC.DAT','NOODOE.RSC')):
  expected=file_bytes(fs,name)
  for off in (0,len(expected)-4096):
   require(call('GateStore_Read',store,i,off,dest,4096)==0,'ARM mapped read failed')
   require(bytes(u.mem_read(dest,4096))==expected[off:off+4096],'ARM logical file read differs')
  records.append(dict(name=name,bytes=len(expected),first_last_blocks_match=True))
 return dict(hardware=False,elf_sha256=sha(binary),nor_sha256=sha(raw),uid=uid,state=state,found=fields[13],identities=fields[14],polls=count+1,physical_read_calls=len(reads),files=records)

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--elf',type=Path,required=True);p.add_argument('--nor',type=Path,required=True);p.add_argument('--uid',nargs=3,type=lambda x:int(x,0),required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 result=probe(a.elf,a.nor.read_bytes(),a.uid);a.output.write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
