"""Execute production log codec/writer in ARM; cut every erase/program/commit."""
from pathlib import Path
import sys,subprocess,struct,json,zlib
H=Path(__file__).resolve().parent;P=H.parents[2]
sys.path[:0]=[str(P/'tools'),str(P.parents[1]/'.tools/analysis-python')]
from recovery_gate_build import TC
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_XPSR,UC_ARM_REG_R0,UC_ARM_REG_PC
O=H/'output';O.mkdir(exist_ok=True)
ld=O/'test.ld';ld.write_text('MEMORY { FLASH(rx): ORIGIN = 0x10000000, LENGTH = 128K\n RAM(rwx): ORIGIN = 0x20000000, LENGTH = 512K }\nSECTIONS { .text : { *(.text*) *(.rodata*) } > FLASH\n .data : { *(.data*) } > RAM\n .bss(NOLOAD) : { *(.bss*) *(COMMON) } > RAM\n /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) } }')
rows=[]
for opt in ('O0','Os'):
 elf=O/(opt+'.elf')
 cmd=[str(TC/'arm-none-eabi-gcc.exe'),str(next(q for q in Path(__file__).resolve().parents if (q/'tools/build.ps1').is_file())/'Drivers/BSP/src/noodoe_crc32.c'),'-I'+str(next(q for q in Path(__file__).resolve().parents if (q/'tools/build.ps1').is_file())/'Drivers/BSP/inc'),'-mcpu=cortex-m4','-mthumb','-'+opt,'-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-ffunction-sections','-fdata-sections','-I'+str(P/'RecoveryGate/include'),str(H/'test.c'),str(P/'RecoveryGate/src/event_log.c'),str(P/'RecoveryGate/src/gate_policy.c'),str(P/'RecoveryGate/src/gate_format.c'),'-nostartfiles','--specs=nosys.specs','--specs=nano.specs','-Wl,-T,'+str(ld),'-o',str(elf)]
 r=subprocess.run(cmd,capture_output=True,text=True);(O/(opt+'.log')).write_text(r.stdout+r.stderr)
 if r.returncode:raise RuntimeError(r.stderr)
 syms={s[2]:int(s[0],16)for l in subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),str(elf)],text=True).splitlines()if len(s:=l.split())==3}
 data=elf.read_bytes();ph=struct.unpack_from('<I',data,28)[0];ps,pn=struct.unpack_from('<HH',data,42)
 for name,mode in [('Test_Cut',i)for i in range(1,20)]+[('Test_Rotate',0),('Test_Fault',0),('Test_Format',0)]+[('Test_Identity',i)for i in range(3)]+[('Test_Rollback',i)for i in range(4)]:
  u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS);u.mem_map(0x10000000,0x20000);u.mem_map(0x20000000,0x80000)
  for i in range(pn):
   k,o,a,_,n,_,_,_=struct.unpack_from('<8I',data,ph+i*ps)
   if k==1 and n:u.mem_write(a,data[o:o+n])
  stop=0x1001fff0;u.reg_write(UC_ARM_REG_SP,0x2007fff0);u.reg_write(UC_ARM_REG_LR,stop|1);u.reg_write(UC_ARM_REG_XPSR,0x1000000);u.reg_write(UC_ARM_REG_R0,mode)
  u.emu_start(syms[name]|1,stop,count=300000000)
  if name=='Test_Format':
   expected=bytearray(b'\xff'*4096)
   fields={0:0x314a4247,4:2,8:19,12:3,16:1,20:0xffffffff,24:2,28:0,96:1,100:2,104:3,108:12,112:13,116:1,120:1,124:0,128:11,164:100,168:99,172:4,176:456,180:87}
   for offset,value in fields.items():struct.pack_into('<I',expected,offset,value)
   expected[32:64]=bytes(range(32));expected[64:96]=bytes(range(32,64));expected[132:164]=bytes(range(64,96))
   struct.pack_into('<II',expected,4088,zlib.crc32(expected[:4088]),0x31544d43)
   assert bytes(u.mem_read(syms['identity'],4096))==expected,'ARM codec differs from independent LE fixture'
  failure=struct.unpack('<I',u.mem_read(syms['failure'],4))[0]
  row=dict(opt=opt,test=name,cut=mode,failure_line=failure,returned=u.reg_read(UC_ARM_REG_PC)==stop,passed=not failure and u.reg_read(UC_ARM_REG_PC)==stop);rows.append(row)
  if not row['passed']:print(row);raise SystemExit(1)
(O/'results.json').write_text(json.dumps(dict(hardware=False,passed=True,cases=rows),indent=2))
print(f'{len(rows)} ARM scenarios passed; no hardware access')
