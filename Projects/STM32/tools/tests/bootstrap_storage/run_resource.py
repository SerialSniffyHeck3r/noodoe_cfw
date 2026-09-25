"""Actual ARM empty-B writer tests. No hardware, no fake success reports."""
from pathlib import Path
import sys,subprocess,json,struct
H=Path(__file__).resolve().parent;P=H.parents[2];sys.path.insert(0,str(P/'tools'));sys.path.insert(0,str(P.parents[1]/'.tools/analysis-python'))
from resource_install import swapped,make_plan,TC
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_R0,UC_ARM_REG_PC,UC_ARM_REG_XPSR
O=H/'resource-output';O.mkdir(exist_ok=True)
empty=bytearray(0x8000000);struct.pack_into('<H',empty,11,4096);empty[13]=8;struct.pack_into('<H',empty,14,1);empty[16]=2;struct.pack_into('<H',empty,17,512);empty[21]=0xf8;struct.pack_into('<H',empty,22,2);struct.pack_into('<I',empty,32,0x7f80);empty[510:512]=b'\x55\xaa';empty[0x1000:0x1003]=b'\xf8\xff\xff';empty[0x3000:0x3003]=b'\xf8\xff\xff'
payload=(P/'Resources/slot.bin').read_bytes();plan,before=make_plan(bytes(swapped(empty)),payload,1,0);base=plan['regions'][1]['offset'];_,after=make_plan(before,payload,2,1)
ld=O/'test.ld';ld.write_text('MEMORY { FLASH(rx): ORIGIN=0x10000000, LENGTH=256K\nRAM(rwx): ORIGIN=0x20000000, LENGTH=192K }\nSECTIONS { .text : { *(.text*) *(.rodata*) } > FLASH\n.data : { *(.data*) } > RAM\n.bss(NOLOAD) : { *(.bss*) *(COMMON) } > RAM\n/DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) } }'.replace('ORIGIN=','ORIGIN = ').replace('LENGTH=','LENGTH = '))
sources=[P/'Middlewares/Noodoe/Storage/src'/n for n in ('bootstrap_storage.c','bootstrap_resources.c','recovery_store.c')]+[P/'Middlewares/Noodoe/Update/src'/n for n in ('Update_SHA256.c','Recovery_Core.c')]+[P/'Middlewares/Noodoe/Resources/src/Resources_Format.c',P/'RecoveryGate/src/gate_format.c',P/'RecoveryGate/src/gate_policy.c',P/'RecoveryGate/src/event_log.c',H/'test.c']
rows=[]
for opt in ('O0','Os'):
 elf=O/(opt+'.elf');cmd=[str(TC/'arm-none-eabi-gcc.exe'),str(next(q for q in Path(__file__).resolve().parents if (q/'tools/build.ps1').is_file())/'Drivers/BSP/src/noodoe_crc32.c'),'-I'+str(next(q for q in Path(__file__).resolve().parents if (q/'tools/build.ps1').is_file())/'Drivers/BSP/inc'),'-mcpu=cortex-m4','-mthumb','-std=c11','-'+opt,'-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-ffreestanding','-fno-builtin','-nostdlib']
 for d in ('Storage','Update','Resources'):cmd+=['-I',str(P/f'Middlewares/Noodoe/{d}/inc')]
 cmd+=['-I',str(P/'RecoveryGate/include'),*map(str,sources),'-T',str(ld),'-Wl,--gc-sections,-e,ResourceB','-lgcc','-o',str(elf)]
 r=subprocess.run(cmd,capture_output=True,text=True);(O/(opt+'-build.log')).write_text(r.stdout+r.stderr)
 if r.returncode:raise RuntimeError(r.stderr)
 nm=subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),str(elf)],text=True);syms={s[2]:int(s[0],16)for l in nm.splitlines()if len(s:=l.split())==3};binary=elf.read_bytes()
 for case in range(7):
  disk=bytearray(before);data=bytearray(payload)
  if case==1:disk[base+0x80000]=0
  if case==2:disk[base+4097]^=1
  if case==3:data[4096]^=1
  if case==4:data[4]=2
  u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
  for a,n in ((0x10000000,0x40000),(0x20000000,0x30000),(0x90000000,0x8000000),(0xc0000000,0x100000),(0x11000000,0x100000)):u.mem_map(a,n)
  ph=struct.unpack_from('<I',binary,28)[0];ps,pn=struct.unpack_from('<HH',binary,42)
  for i in range(pn):
   t,o,va,_,n,_,_,_=struct.unpack_from('<8I',binary,ph+i*ps)
   if t==1 and n:u.mem_write(va,binary[o:o+n])
  u.mem_write(0x90000000,bytes(disk));u.mem_write(0x11000000,bytes(data));stop=0x1003fff0
  u.reg_write(UC_ARM_REG_SP,0x2002fff0);u.reg_write(UC_ARM_REG_LR,stop|1);u.reg_write(UC_ARM_REG_R0,case);u.reg_write(UC_ARM_REG_XPSR,0x01000000)
  u.emu_start(syms['ResourceB']|1,stop,count=2_000_000_000,timeout=60_000_000)
  if u.reg_read(UC_ARM_REG_PC)!=stop or u.reg_read(UC_ARM_REG_R0):raise RuntimeError((opt,case,hex(u.reg_read(UC_ARM_REG_PC)),u.reg_read(UC_ARM_REG_R0)))
  observed=bytes(u.mem_read(0x90000000,0x8000000))
  if case==0:assert observed==after
  elif case<=4:assert observed==bytes(disk)
  else:assert observed[:base+0x80000]==before[:base+0x80000] and observed[base+0x100000:]==before[base+0x100000:] and observed[base+0x80000+4092:base+0x80000+4096]==b'\xff'*4
  row=dict(optimization=opt,case=case,passed=True,hardware=False);rows.append(row);print(row,flush=True);(O/'results.json').write_text(json.dumps(rows,indent=2))
