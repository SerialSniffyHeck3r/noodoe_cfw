"""Execute production ARM cleanup code against physical, torn-write NOR models.
No debugger/serial/hardware access. Every reset loses the entire C state.
"""
from pathlib import Path
import argparse,struct,sys,subprocess,zlib,hashlib,json,time
P=Path(__file__).resolve().parents[2];HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(P.parents[1]/'.tools/analysis-python'))
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS,UC_HOOK_CODE
from unicorn.arm_const import *
sys.path.insert(0,str(P/'tools'))
from recovery_gate_build import TC
FF=b'\xff'*4096
def put(p,o,*values):struct.pack_into('<'+'I'*len(values),p,o,*values)
def seal(p,commit=0x31544d43):put(p,4088,zlib.crc32(p[:4088]),commit);return bytes(p)
def block():return bytearray(FF)
def pair(data):b=bytearray(data);b[::2],b[1::2]=data[1::2],data[::2];return bytes(b)
def fixture():
 m=bytearray(0x9000);struct.pack_into('<H',m,11,4096);m[13]=8;struct.pack_into('<H',m,14,1);m[16]=2;struct.pack_into('<H',m,17,512);struct.pack_into('<H',m,22,2);put(m,32,0x7f80);m[510:512]=b'\x55\xaa'
 def fat(c,v):
  o=4096+c+c//2;x=int.from_bytes(m[o:o+2],'little');x=(x&15)|(v<<4) if c&1 else (x&0xf000)|v;m[o:o+2]=x.to_bytes(2,'little')
 fat(0,0xff8);fat(1,0xfff);pages={};ranges=[];first=2720 # exercises FAT12 cross-sector word
 names=['CFWA    DAT','CFWB    DAT','CFWBOOT DAT','CFWREC  DAT','NOODOE  RSC','CFWLOG  DAT','CFWCFG  DAT','CFWRIDE DAT','CFWPIC  DAT']
 sizes=[0x80000,0x80000,0x10000,0x80000,0x100000,0x40000,0x20000,0x40000,0x100000]
 for f,(name,size) in enumerate(zip(names,sizes)):
  o=0x5000+f*32;m[o:o+11]=name.encode();m[o+11]=32;struct.pack_into('<H',m,o+26,first);put(m,o+28,size)
  for c in range(first,first+size//32768):fat(c,c+1 if c+1<first+size//32768 else 0xfff)
  a=0x9000+(first-2)*32768;ranges.append((a,size));h=block();off=0
  if f<2:put(h,0,0x31444947,1,f,0x80000,1,2,3,1);off=0x7f000
  elif f==2:
   put(h,0,0x314a4247,2,1,1,0,0xffffffff,0,0);h[32:96]=bytes(64);put(h,96,1,2,3,1,0,1,1,0xffffffff,0);h[132:184]=bytes(52)
  elif f==3:
   put(h,0,0x3152434e,1,4096,0x80000,1,2,3,0x00100005,0x70000,0x000e0000);h[40:72]=bytes.fromhex('162faeecc64bf56d828570168785e66d1a7da0152ef912bf83f9a19094d91edf')
  elif f==4:
   put(h,0,0x31534352,1,4,1);put(h,48,1,0,4,0);h[16:48]=hashlib.sha256(h[48:64]+b'TEST').digest();pages[a+4096]=pair(b'TEST'+FF[4:])
  elif f==5:put(h,0,0x31494c4e,1,0x40000,64,1,2,3);off=0x3f000
  else:
   purpose=f-5;put(h,0,0x314a4643,1,purpose,1,8,1,2,3)
   if f==8:put(h,64,0x31465043,1)
  pages[a+off]=pair(seal(h,0x434d5431 if f==4 else 0x31544d43));first+=size//32768
 # Stock-owned file and a legacy WALL file must remain allocated/content-identical.
 for i,name in enumerate(['STOCK   JPG','WALL0   JPG']):
  c=5+i;fat(c,0xfff);o=0x5000+(9+i)*32;m[o:o+11]=name.encode();m[o+11]=32;struct.pack_into('<H',m,o+26,c);put(m,o+28,32768)
  for off in range(0,32768,4096):pages[0x9000+(c-2)*32768+off]=bytes([41+i])*4096
 m[0x3000:0x5000]=m[0x1000:0x3000]
 for a in range(0,0x9000,4096):pages[a]=pair(m[a:a+4096])
 return pages,ranges,m
class PowerCut(Exception):pass
class Model:
 def __init__(self,elf,symbols,pages,journal=None,cut=None,tear=False):
  self.pages=dict(pages);self.journal=bytearray(b'\xff'*0xf000 if journal is None else journal);self.mutations=[];self.cut=cut;self.tear=tear
  self.u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS);self.u.mem_map(0x10000000,0x20000);self.u.mem_map(0x20000000,0x40000)
  self.symbols=symbols
  phoff=struct.unpack_from('<I',elf,28)[0];phsize,phnum=struct.unpack_from('<HH',elf,42)
  for i in range(phnum):
   kind,off,va,_,size,*_=struct.unpack_from('<8I',elf,phoff+i*phsize)
   if kind==1 and size:self.u.mem_write(va,elf[off:off+size])
  for name in ['TestRead','TestErase','TestProgram','TestJournalRead','TestJournalProgram']:
   a=symbols[name]&~1;self.u.hook_add(UC_HOOK_CODE,self.hook,name,begin=a,end=a)
 def read(self,a,n):return b''.join(self.pages.get(k,FF) for k in range(a&~4095,(a+n+4095)&~4095,4096))[a%4096:a%4096+n]
 def hook(self,u,address,size,name):
  a,p,n=[u.reg_read(r) for r in (UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3)]
  if name=='TestRead':u.mem_write(p,self.read(a,n))
  elif name=='TestJournalRead':u.mem_write(p,bytes(self.journal[a:a+n]))
  else:
   iscut=self.cut==len(self.mutations);self.mutations.append((name,a,n if name!='TestErase' else 4096))
   if iscut and not self.tear:raise PowerCut()
   if name=='TestErase':
    old=self.pages.get(a,FF);self.pages[a]=FF if not iscut else FF[:2048]+old[2048:]
   elif name=='TestProgram':
    v=bytes(u.mem_read(p,n));old=bytearray(self.pages.get(a&~4095,FF));k=n//2 if iscut else n
    for i in range(k):old[a%4096+i]&=v[i]
    self.pages[a&~4095]=bytes(old)
   else:
    v=bytes(u.mem_read(p,n));k=max(1,n//2) if iscut else n
    for i in range(k):self.journal[a+i]&=v[i]
   if iscut:raise PowerCut()
  u.reg_write(UC_ARM_REG_R0,0);u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
 def call(self,name,arg=0):
  u=self.u;stop=0x1001fff0;u.reg_write(UC_ARM_REG_SP,0x2003fff0);u.reg_write(UC_ARM_REG_LR,stop|1);u.reg_write(UC_ARM_REG_XPSR,0x01000000);u.reg_write(UC_ARM_REG_R0,arg)
  u.emu_start(self.symbols[name]|1,stop,count=300_000_000)
  if u.reg_read(UC_ARM_REG_PC)!=stop:raise AssertionError('execution budget '+name)
  return u.reg_read(UC_ARM_REG_R0)
 def run(self):
  for name in ['TestInit','TestAudit']:assert self.call(name)==0,(name,self.call('TestError'))
  if self.call('TestState')==1:assert self.call('TestApprove')==0
  for _ in range(2000):
   assert self.call('TestStep')==0,('step',self.call('TestError'))
   if self.call('TestState')==4:return
  raise AssertionError('state machine stalled')
def build(out):
 out.mkdir(parents=True,exist_ok=True);ld=out/'test.ld';ld.write_text('MEMORY { FLASH(rx): ORIGIN = 0x10000000, LENGTH = 128K\nRAM(rwx): ORIGIN = 0x20000000, LENGTH = 256K }\nSECTIONS { .text : { *(.text*) *(.rodata*) } > FLASH\n.data : { *(.data*) } > RAM\n.bss(NOLOAD) : { *(.bss*) *(COMMON) } > RAM\n/DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) } }')
 sources=[HERE/'harness.c',P/'UninstallBootstrap/src/uninstall_core.c',*[P/'RecoveryGate/src'/f for f in ['gate_store.c','gate_format.c','gate_policy.c','event_log.c']],P/'Drivers/BSP/src/noodoe_crc32.c',P/'Middlewares/Noodoe/Storage/src/recovery_store.c',P/'Middlewares/Noodoe/Resources/src/Resources_Format.c',P/'Middlewares/Noodoe/Update/src/Update_SHA256.c',P/'Middlewares/Noodoe/Update/src/Recovery_Core.c']
 inc=['UninstallBootstrap/include','RecoveryGate/include','Drivers/BSP/inc','Middlewares/Noodoe/Update/inc','Middlewares/Noodoe/Storage/inc','Middlewares/Noodoe/Resources/inc']
 elf=out/'test.elf';cmd=[str(TC/'arm-none-eabi-gcc.exe'),'-mcpu=cortex-m4','-mthumb','-Os','-DNDEBUG','-DNOODOE_UNINSTALL=1','-ffunction-sections','-fdata-sections','-nostartfiles','--specs=nosys.specs','--specs=nano.specs',*['-I'+str(P/x) for x in inc],*map(str,sources),'-T',str(ld),'-Wl,-e,TestInit','-Wl,--gc-sections',*['-Wl,--undefined='+x for x in ['TestAudit','TestApprove','TestStep','TestState','TestBytes','TestError']],'-o',str(elf)]
 r=subprocess.run(cmd,capture_output=True,text=True);(out/'compile.log').write_text(r.stdout+r.stderr);assert not r.returncode,r.stderr
 nm=subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),str(elf)],text=True);symbols={p[2]:int(p[0],16) for line in nm.splitlines() if len(p:=line.split())==3};return elf.read_bytes(),symbols
def main():
 a=argparse.ArgumentParser();a.add_argument('--output',type=Path,required=True);a.add_argument('--all-cuts',action='store_true');args=a.parse_args();elf,sym=build(args.output)
 pages,ranges,meta=fixture();baseline=Model(elf,sym,pages);baseline.run();final=baseline.pages
 for a,n in ranges:assert baseline.read(a,n)==b'\xff'*n
 for a,b in pages.items():
  if a>=0x9000 and not any(x<=a<x+n for x,n in ranges):assert final.get(a,FF)==b
 logical=pair(baseline.read(0,0x9000));assert logical[0:4096]==meta[0:4096];assert logical[0x1000:0x3000]==logical[0x3000:0x5000]
 assert logical[0x5000+9*32:]==meta[0x5000+9*32:]
 for i in range(9):assert logical[0x5000+i*32]==0xe5;assert logical[0x5000+i*32+1:0x5000+(i+1)*32]==meta[0x5000+i*32+1:0x5000+(i+1)*32]
 muts=baseline.mutations;cuts=list(range(len(muts))) if args.all_cuts else sorted(set([0,1,15,143,144,159,160,len(muts)-1]+[i for i,x in enumerate(muts) if x[0]=='TestErase']))
 # Fixture's unallocated sectors are alreadyFF: each present content sector,
 # both FAT mirrors and directory erase is individually interrupted.
 results=[];start=time.monotonic()
 for cut in cuts:
  for tear in [False,True]:
   m=Model(elf,sym,pages,cut=cut,tear=tear)
   try:m.run();raise AssertionError('cut not reached')
   except PowerCut:pass
   resumed=Model(elf,sym,m.pages,m.journal);resumed.run()
   for address in set(final)|set(resumed.pages):assert resumed.pages.get(address,FF)==final.get(address,FF),(cut,tear,hex(address))
   results.append(dict(cut=cut,mutation=muts[cut],torn=tear,result='pass'))
  print(json.dumps(dict(cut=cut,passed=len(results))),flush=True)
 # Read-only rejection: wrong UID, conflicting owner, FAT mirror corruption.
 for case in ['uid','crosslink','mirror','unknown-file','journal-corrupt']:
  q=dict(pages);j=None;uid=0
  if case=='uid':uid=9
  elif case=='mirror':b=bytearray(q[0x3000]);b[20]^=1;q[0x3000]=bytes(b)
  elif case=='crosslink':b=bytearray(pair(q[0x5000]));b[32+26:32+28]=b[26:28];q[0x5000]=pair(b)
  elif case=='unknown-file':q[ranges[6][0]]=FF
  else:j=bytearray(baseline.journal);j[52]^=1
  m=Model(elf,sym,q,j);r=m.call('TestInit',uid)
  if not r:r=m.call('TestAudit')
  assert r and not m.mutations,case
  results.append(dict(case=case,result='pass'))
 report=dict(hardware=False,seconds=round(time.monotonic()-start,2),tests=results,erased_bytes=sum(n for a,n in ranges),mutations=len(muts));(args.output/'results.json').write_text(json.dumps(report,indent=2));print('PASS',len(results),flush=True)
if __name__=='__main__':main()
