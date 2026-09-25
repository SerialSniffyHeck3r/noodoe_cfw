"""Execute production log codec/writer in ARM; cut every erase/program/commit."""
from pathlib import Path
import sys,subprocess,struct,json
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
 incs=[P/'RecoveryGate/include',P/'Core/Inc',P/'Drivers/BSP/inc',P/'Drivers/CMSIS/Include',P/'Drivers/CMSIS/Device/ST/STM32F4xx/Include',P/'Drivers/STM32F4xx_HAL_Driver/Inc',P/'Middlewares/Third_Party/FreeRTOS/Source/include',P/'Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F',P/'Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2']+list((P/'App_Logic').glob('*/inc'))+list((P/'Middlewares/Noodoe').glob('*/inc'))
 cmd=[str(TC/'arm-none-eabi-gcc.exe'),str(next(q for q in Path(__file__).resolve().parents if (q/'tools/build.ps1').is_file())/'Drivers/BSP/src/noodoe_crc32.c'),'-I'+str(next(q for q in Path(__file__).resolve().parents if (q/'tools/build.ps1').is_file())/'Drivers/BSP/inc'),'-mcpu=cortex-m4','-mthumb','-'+opt,'-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-DNOODOE_PRODUCT=1','-DSTM32F429xx','-DUSE_HAL_DRIVER','-ffunction-sections','-fdata-sections',*['-I'+str(x) for x in incs],str(H/'test.c'),str(P/'App_Logic/Recovery/src/App_Recovery_Product.c'),str(P/'RecoveryGate/src/event_log.c'),str(P/'RecoveryGate/src/gate_policy.c'),'-nostartfiles','--specs=nosys.specs','--specs=nano.specs','-Wl,--gc-sections,-u,Test_Confirm,-u,Test_LinkWithoutApproval,-u,Test_ApprovalBeforeHealthy,-u,Test_Timeout,-T,'+str(ld),'-o',str(elf)]
 r=subprocess.run(cmd,capture_output=True,text=True);(O/(opt+'.log')).write_text(r.stdout+r.stderr)
 if r.returncode:raise RuntimeError(r.stderr)
 syms={s[2]:int(s[0],16)for l in subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),str(elf)],text=True).splitlines()if len(s:=l.split())==3}
 data=elf.read_bytes();ph=struct.unpack_from('<I',data,28)[0];ps,pn=struct.unpack_from('<HH',data,42)
 for name,mode in [(name,0) for name in ('Test_Confirm','Test_LinkWithoutApproval','Test_ApprovalBeforeHealthy')]+[('Test_Timeout',i) for i in range(4)]:
  u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS);u.mem_map(0x10000000,0x20000);u.mem_map(0x20000000,0x80000);u.mem_map(0xe000e000,0x1000)
  for i in range(pn):
   k,o,a,_,n,_,_,_=struct.unpack_from('<8I',data,ph+i*ps)
   if k==1 and n:u.mem_write(a,data[o:o+n])
  stop=0x1001fff0;u.reg_write(UC_ARM_REG_SP,0x2007fff0);u.reg_write(UC_ARM_REG_LR,stop|1);u.reg_write(UC_ARM_REG_XPSR,0x1000000);u.reg_write(UC_ARM_REG_R0,mode)
  reset=[False]
  from unicorn import UC_HOOK_MEM_WRITE
  def write_hook(uc,access,address,size,value,data):
   if address==0xe000ed0c and value&4:reset[0]=True;uc.emu_stop()
  u.hook_add(UC_HOOK_MEM_WRITE,write_hook)
  u.emu_start(syms[name]|1,stop,count=3000000)
  if name=='Test_Timeout':
   reason=struct.unpack('<I',u.mem_read(syms['g_recovery_mailbox']+8,4))[0]
   assert reset[0] and reason==(5 if mode in (0,3) else 6),(reset,reason)

  failure=struct.unpack('<I',u.mem_read(syms['failure'],4))[0]
  row=dict(opt=opt,test=name,cut=mode,failure_line=failure,returned=(reset[0] if name=='Test_Timeout' else u.reg_read(UC_ARM_REG_PC)==stop),passed=not failure and (reset[0] if name=='Test_Timeout' else u.reg_read(UC_ARM_REG_PC)==stop));rows.append(row)
  if not row['passed']:print(row);raise SystemExit(1)
(O/'results.json').write_text(json.dumps(dict(hardware=False,passed=True,cases=rows),indent=2))
print(f'{len(rows)} ARM scenarios passed; no hardware access')
