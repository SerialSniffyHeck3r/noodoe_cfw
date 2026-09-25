"""Actual SettingsService/codec in ARM O0/Os; no device or filesystem writes."""
import hashlib,json,struct,subprocess,sys
from pathlib import Path
HERE=Path(__file__).resolve().parent;PROJECT=HERE.parents[2]
CC=Path('C:/ST/STM32CubeIDE_1.18.1/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344/tools/bin')
sys.path.insert(0,str(PROJECT.parents[1]/'.tools/analysis-python'))
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_LR,UC_ARM_REG_PC,UC_ARM_REG_R0,UC_ARM_REG_SP,UC_ARM_REG_XPSR
out=HERE/'output';out.mkdir(exist_ok=True);linker=out/'test.ld'
linker.write_text('MEMORY { FLASH(rx): ORIGIN = 0x10000000, LENGTH = 256K\nRAM(rwx): ORIGIN = 0x20000000, LENGTH = 128K }\nSECTIONS { .text : { *(.text*) *(.rodata*) } > FLASH\n.data : { *(.data*) } > RAM AT>FLASH\n.bss(NOLOAD) : { *(.bss*) *(COMMON) } > RAM\n . = ALIGN(8); end = .;\n/DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) } }\n')
incs=[HERE/'stubs',*list((PROJECT/'App_Logic').glob('*/inc')),*list((PROJECT/'Middlewares/Noodoe').glob('*/inc')),PROJECT/'App_Logic/Settings/src',PROJECT/'Graphics/UI/inc',PROJECT/'Drivers/BSP/inc',PROJECT/'Middlewares/Third_Party/FatFs/src'];reports=[]
for optimization in ('O0','Os'):
    elf=out/f'settings-{optimization}.elf'
    sources=['App_Logic/UI/src/ui_theme.c','App_Logic/UI/src/Fuel_Policy.c','App_Logic/UI/src/phone_indicators.c','App_Logic/UI/src/screen_warning_overlay.c','App_Logic/UI/src/ui_off_stages.c','Middlewares/Noodoe/Bluetooth/src/RadioSelfTest.c','Drivers/BSP/src/noodoe_crc32.c']
    cmd=[str(CC/'arm-none-eabi-gcc.exe'),'-mcpu=cortex-m4','-mthumb','-std=c11',f'-{optimization}','-Wall','-ffunction-sections','-fdata-sections','-nostartfiles','--specs=nano.specs','--specs=nosys.specs',*['-I'+str(p) for p in incs],str(HERE/'test.c'),*[str(PROJECT/f) for f in sources],'-Wl,-T,'+str(linker),'-Wl,--gc-sections','-Wl,-e,Test','-o',str(elf),'-lgcc']
    compiled=subprocess.run(cmd,capture_output=True,text=True);(out/f'compile-{optimization}.log').write_text(compiled.stdout+compiled.stderr)
    if compiled.returncode:raise RuntimeError(compiled.stderr)
    listing=subprocess.check_output([str(CC/'arm-none-eabi-nm.exe'),str(elf)],text=True);symbols={p[2]:int(p[0],16) for line in listing.splitlines() if len(p:=line.split())==3}
    emu=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS);emu.mem_map(0x10000000,0x40000);emu.mem_map(0x20000000,0x20000)
    data=elf.read_bytes();header=struct.unpack_from('<16sHHIIIIIHHHHHH',data)
    for i in range(header[10]):
        kind,offset,address,_,size,_,_,_=struct.unpack_from('<IIIIIIII',data,header[5]+i*header[9])
        if kind==1 and size:emu.mem_write(address,data[offset:offset+size])
    stop=0x1000FFF0;emu.mem_write(stop,b'\x00\xbe');emu.reg_write(UC_ARM_REG_SP,0x2001FFF0);emu.reg_write(UC_ARM_REG_LR,stop|1);emu.reg_write(UC_ARM_REG_XPSR,0x01000000)
    emu.emu_start(symbols['Test']|1,stop,timeout=30_000_000,count=40_000_000)
    word=lambda name:struct.unpack('<I',emu.mem_read(symbols[name],4))[0]
    report={'optimization':optimization,'returned':emu.reg_read(UC_ARM_REG_PC)==stop,'result':emu.reg_read(UC_ARM_REG_R0),'assertions':word('assertions'),'failure_line':emu.reg_read(UC_ARM_REG_R0),'hardware_access':False,'source_sha256':{name:hashlib.sha256((PROJECT/name).read_bytes()).hexdigest() for name in sources}}
    reports.append(report);print(json.dumps(report),flush=True);(out/'results.json').write_text(json.dumps(reports,indent=2)+'\n')
    if not report['returned'] or report['result'] or report['failure_line']:raise SystemExit(1)
