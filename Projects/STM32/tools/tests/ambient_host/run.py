"""Run actual ALS BSP/service C against mocked HAL on ARM; no device access."""
import hashlib,json,struct,subprocess,sys
from pathlib import Path
HERE=Path(__file__).resolve().parent;PROJECT=HERE.parents[2]
CC=Path('C:/ST/STM32CubeIDE_1.18.1/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344/tools/bin')
sys.path.insert(0,str(PROJECT.parents[1]/'.tools/analysis-python'))
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_LR,UC_ARM_REG_PC,UC_ARM_REG_R0,UC_ARM_REG_SP,UC_ARM_REG_XPSR
out=HERE/'output';out.mkdir(exist_ok=True);linker=out/'test.ld'
linker.write_text('MEMORY { FLASH(rx): ORIGIN = 0x10000000, LENGTH = 64K\nRAM(rwx): ORIGIN = 0x20000000, LENGTH = 128K }\nSECTIONS { .text : { *(.text*) *(.rodata*) } > FLASH\n.data : { *(.data*) } > RAM AT>FLASH\n.bss(NOLOAD) : { *(.bss*) *(COMMON) } > RAM\n/DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) } }\n')
incs=['Core/Inc','Drivers/BSP/inc','Drivers/BSP/src','Middlewares/Noodoe/Ambient/inc','Middlewares/Noodoe/Ambient/src','Drivers/STM32F4xx_HAL_Driver/Inc','Drivers/CMSIS/Device/ST/STM32F4xx/Include','Drivers/CMSIS/Include'];reports=[]
base=[str(CC/'arm-none-eabi-gcc.exe'),'-mcpu=cortex-m4','-mthumb','-std=c11','-Wall','-Wextra','-Werror','-DSTM32F429xx','-DUSE_HAL_DRIVER','-ffreestanding','-fno-builtin','-ffunction-sections','-fdata-sections',*['-I'+str(PROJECT/p) for p in incs]]
sources=['Drivers/BSP/src/BSP_Ambient.c','Drivers/BSP/inc/BSP_Ambient.h','Middlewares/Noodoe/Ambient/src/AmbientService.c','Middlewares/Noodoe/Ambient/inc/AmbientService.h','tools/tests/ambient_host/test_ambient.c','tools/tests/ambient_host/run.py','Drivers/BSP/src/BSP_AmbientBitbang.c','Drivers/BSP/inc/BSP_AmbientBitbang.h','tools/tests/ambient_host/test_bitbang.c']
sources+=['Drivers/BSP/src/BSP_AmbientAddress.c','Drivers/BSP/inc/BSP_AmbientAddress.h','Drivers/BSP/src/bsp_ambient_bitbang_private.h','tools/tests/ambient_host/test_address.c']
for source in sources:
    if '/test_' in source or not source.endswith('.c'):continue
    prod=subprocess.run([*base,'-Os','-c',str(PROJECT/source),'-o',str(out/(Path(source).stem+'-production.o'))],capture_output=True,text=True)
    (out/(Path(source).stem+'-production.log')).write_text(prod.stdout+prod.stderr)
    if prod.returncode:raise RuntimeError(prod.stderr)
for fixture,entry in [('test_ambient.c','Ambient_TestMain'),('test_bitbang.c','Ambient_BitbangTestMain'),('test_address.c','Ambient_AddressTestMain')]:
 for optimization in ('O0','Os'):
     elf=out/f'{Path(fixture).stem}-{optimization}.elf'
     cmd=[*base,f'-{optimization}','-nostdlib',str(HERE/fixture),*([str(PROJECT/'Middlewares/Noodoe/Ambient/src/AmbientDiagnostics.c')] if fixture=='test_ambient.c' else []),'-Wl,-T,'+str(linker),'-Wl,--gc-sections','-Wl,-e,'+entry,'-o',str(elf),'-lgcc']
     compiled=subprocess.run(cmd,capture_output=True,text=True);(out/f'compile-{Path(fixture).stem}-{optimization}.log').write_text(compiled.stdout+compiled.stderr)
     if compiled.returncode:raise RuntimeError(compiled.stderr)
     listing=subprocess.check_output([str(CC/'arm-none-eabi-nm.exe'),str(elf)],text=True);symbols={p[2]:int(p[0],16) for line in listing.splitlines() if len(p:=line.split())==3}
     emu=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
     for address,size in ((0x10000000,0x10000),(0x20000000,0x20000),(0x40000000,0x30000),(0xE000E000,0x2000),(0xE0001000,0x1000)):emu.mem_map(address,size)
     data=elf.read_bytes();header=struct.unpack_from('<16sHHIIIIIHHHHHH',data)
     for i in range(header[10]):
         kind,offset,address,_,size,_,_,_=struct.unpack_from('<IIIIIIII',data,header[5]+i*header[9])
         if kind==1 and size:emu.mem_write(address,data[offset:offset+size])
     stop=0x1000FFF0;emu.mem_write(stop,b'\x00\xbe');emu.reg_write(UC_ARM_REG_SP,0x2001FFF0);emu.reg_write(UC_ARM_REG_LR,stop|1);emu.reg_write(UC_ARM_REG_XPSR,0x01000000)
     # The address fixture executes four real bus transactions per positive
     # case. Give its larger finite suite more instruction budget; production
     # timing limits remain part of the C under test and are not relaxed here.
     emu.emu_start(symbols[entry]|1,stop,timeout=30_000_000,
                   count=100_000_000 if fixture=='test_address.c' else 30_000_000)
     word=lambda name:struct.unpack('<I',emu.mem_read(symbols[name],4))[0]
     report={'fixture':fixture,'optimization':optimization,'returned':emu.reg_read(UC_ARM_REG_PC)==stop,'result':emu.reg_read(UC_ARM_REG_R0),'assertions':word('g_assertions'),'failure_line':word('g_failure_line'),'mock_error':word('g_mock_error'),'hardware_access':False,'production_compile':'pass','sources':{p:hashlib.sha256((PROJECT/p).read_bytes()).hexdigest() for p in sources}}
     reports.append(report);print(json.dumps(report),flush=True);(out/'results.json').write_text(json.dumps(reports,indent=2)+'\n')
     if not report['returned'] or report['result'] or report['failure_line'] or report['mock_error']:raise SystemExit(1)
