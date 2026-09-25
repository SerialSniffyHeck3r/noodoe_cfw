"""Actual ARM renderer + upstream sine table; compare emitted EVE vertices with geometry."""
import json,math,struct,subprocess,sys
from pathlib import Path
H=Path(__file__).resolve().parent;P=H.parents[2];out=H/'output';out.mkdir(exist_ok=True)
sys.path.insert(0,str(P.parents[1]/'.tools/analysis-python'))
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_R0,UC_ARM_REG_XPSR,UC_ARM_REG_PC
TC=Path('C:/ST/STM32CubeIDE_1.18.1/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344/tools/bin')
(out/'test.ld').write_text('MEMORY { FLASH(rx): ORIGIN=0x10000000, LENGTH=256K\n RAM(rwx): ORIGIN=0x20000000, LENGTH=128K }\n SECTIONS { .text : { *(.text*) *(.rodata*) } > FLASH\n .data : { *(.data*) } > RAM\n .bss(NOLOAD) : { *(.bss*) *(COMMON) } > RAM }'.replace('ORIGIN=','ORIGIN = ').replace('LENGTH=','LENGTH = '))
reports=[]
for opt in ('O0','Os'):
 elf=out/(opt+'.elf');lv=P/'Middlewares/Third_Party/LVGL'
 cmd=[str(TC/'arm-none-eabi-gcc.exe'),'-mcpu=cortex-m4','-mthumb','-std=c11','-'+opt,'-ffunction-sections','-fdata-sections','-ffreestanding','-nostartfiles','--specs=nano.specs','--specs=nosys.specs','-DLV_CONF_INCLUDE_SIMPLE','-DNOODOE_PRODUCT=1','-DNOODOE_INTEGRATED=1','-I'+str(P/'Graphics/Port/inc'),'-I'+str(lv),str(P/'Graphics/Port/src/graphics_subpixel_arc.c'),str(lv/'src/misc/lv_math.c'),str(H/'test_arc.c'),'-T',str(out/'test.ld'),'-Wl,-e,test_arc','-Wl,--gc-sections','-lc','-lgcc','-o',str(elf)]
 r=subprocess.run(cmd,capture_output=True,text=True);assert r.returncode==0,r.stdout+r.stderr
 nm=subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),str(elf)],text=True);syms={x.split()[2]:int(x.split()[0],16) for x in nm.splitlines() if len(x.split())==3}
 u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS);u.mem_map(0x10000000,0x40000);u.mem_map(0x20000000,0x20000)
 data=elf.read_bytes();off=struct.unpack_from('<I',data,28)[0];size,num=struct.unpack_from('<HH',data,42)
 for i in range(num):
  kind,offset,va,pa,n,memsz,flags,align=struct.unpack_from('<8I',data,off+i*size)
  if kind==1 and n:u.mem_write(va,data[offset:offset+n])
 u.reg_write(UC_ARM_REG_XPSR,0x1000000);u.reg_write(UC_ARM_REG_SP,0x2001fff0);u.reg_write(UC_ARM_REG_LR,0x1003fff1)
 u.emu_start(syms['test_arc']|1,0x1003fff0,count=500000000)
 assert u.reg_read(UC_ARM_REG_PC)==0x1003fff0,'instruction limit'
 assert u.reg_read(UC_ARM_REG_R0)==0,f'C assertion line {u.reg_read(UC_ARM_REG_R0)}'
 points=struct.unpack('<10001I',u.mem_read(syms['endpoints'],40004));error=0
 for value in range(1,10001):
  command=points[value];x=((command>>15)&32767)/16;y=(command&32767)/16
  angle=math.radians((13500+value*27000//10000)/100)
  error=max(error,math.hypot(x-240-231*math.cos(angle),y-240-231*math.sin(angle)))
 assert error<.16,error
 assert len(set(points[1:]))>9000,'fractional vertices lost'
 report={'optimization':opt,'endpoint_samples':10000,'unique_endpoints':len(set(points[1:])),'max_endpoint_error_px':error,'status':'PASS'};reports.append(report);print(json.dumps(report),flush=True)
(out/'results.json').write_text(json.dumps(reports,indent=2)+'\n')
