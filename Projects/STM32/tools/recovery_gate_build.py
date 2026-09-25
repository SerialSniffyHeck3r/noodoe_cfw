"""Build independent S4 RecoveryGate. No Cube regeneration or hardware access."""
from pathlib import Path
import argparse,concurrent.futures,hashlib,json,subprocess
P=Path(__file__).resolve().parents[1]
TC=Path('C:/ST/STM32CubeIDE_1.18.1/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344/tools/bin')
def build(out):
 out=out.resolve();out.mkdir(parents=True,exist_ok=True)
 includes=[P/x for x in ['RecoveryGate/include','Drivers/BSP/inc','Drivers/CMSIS/Include','Drivers/CMSIS/Device/ST/STM32F4xx/Include','Middlewares/Noodoe/Update/inc','Middlewares/Noodoe/Storage/inc','Middlewares/Noodoe/Resources/inc']]
 sources=list((P/'RecoveryGate/src').glob('*.c'))+[P/'RecoveryGate/startup/gate_reset.s',P/'Drivers/BSP/src/bsp_watchdog.c']
 sources += [P/'Drivers/BSP/src/noodoe_crc32.c']
 sources += [P/'Middlewares/Noodoe/Update/src'/f for f in ['Update_SHA256.c','Update_Metadata.c','Recovery_Core.c']]
 sources += [P/'Middlewares/Noodoe/Storage/src/recovery_store.c',P/'Middlewares/Noodoe/Resources/src/Resources_Format.c']
 flags=['-mcpu=cortex-m4','-mthumb','-mfpu=fpv4-sp-d16','-mfloat-abi=hard','-Oz','-g3','-ffunction-sections','-fdata-sections','-fstack-usage','-Wall','-Wextra','-Werror','-DSTM32F429xx','-DNOODOE_PRODUCT=0','-DNOODOE_RECOVERY_GATE=1','--specs=nano.specs']+['-I'+str(i) for i in includes]
 def compile(src):
  obj=out/(src.stem+'.o');r=subprocess.run([str(TC/'arm-none-eabi-gcc.exe'),*flags,'-c',str(src),'-o',str(obj)],capture_output=True,text=True)
  (out/(src.stem+'.log')).write_text(r.stdout+r.stderr)
  if r.returncode:raise RuntimeError(str(src)+'\n'+r.stderr)
  return obj
 with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:objects=list(pool.map(compile,sources))
 elf=out/'recovery_gate.elf';binary=out/'recovery_gate.bin'
 cmd=[str(TC/'arm-none-eabi-gcc.exe'),*flags,'-nostartfiles','--specs=nosys.specs',*map(str,objects),'-Wl,-T,'+str(P/'RecoveryGate/linker/gate.ld'),'-Wl,--gc-sections','-Wl,-Map,'+str(out/'recovery_gate.map'),'-Wl,--print-memory-usage','-o',str(elf)]
 r=subprocess.run(cmd,capture_output=True,text=True);(out/'link.log').write_text(r.stdout+r.stderr)
 if r.returncode:raise RuntimeError(r.stdout+r.stderr)
 subprocess.run([str(TC/'arm-none-eabi-objcopy.exe'),'-O','binary','--gap-fill=0xFF',str(elf),str(binary)],check=True)
 data=binary.read_bytes();assert len(data)<=0x10000
 result={'base':hex(0x08010000),'bytes':len(data),'sector_bytes':0x10000,'free_bytes':0x10000-len(data),'sha256':hashlib.sha256(data).hexdigest(),'sources':len(sources),'hardware_tested':False,'depends_on':['HSI','LSI/IWDG','internal SRAM','SPI5 NOR'],'not_used':['RTOS','SDRAM','LVGL','Bluetooth','HSE']}
 (out/'manifest.json').write_text(json.dumps(result,indent=2)+'\n');print(r.stdout);print(json.dumps(result,indent=2));return result
if __name__=='__main__':
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--output',type=Path,required=True);args=a.parse_args();build(args.output)
