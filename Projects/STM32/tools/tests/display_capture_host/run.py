"""Real capture C + GNU-wrapped original LVGL allocator under ARM Unicorn."""
import hashlib,json,struct,subprocess,sys,zlib
from pathlib import Path
HERE=Path(__file__).resolve().parent;PROJECT=HERE.parents[2]
CC=Path('C:/ST/STM32CubeIDE_1.18.1/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344/tools/bin')
sys.path.insert(0,str(PROJECT.parents[1]/'.tools/analysis-python'))
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_LR,UC_ARM_REG_PC,UC_ARM_REG_R0,UC_ARM_REG_SP,UC_ARM_REG_XPSR
output=HERE/'output';output.mkdir(exist_ok=True)
linker=output/'test.ld';linker.write_text('MEMORY { FLASH(rx): ORIGIN = 0x10000000, LENGTH = 64K\nRAM(rwx): ORIGIN = 0x20000000, LENGTH = 128K }\nSECTIONS { .text : { *(.text*) *(.rodata*) } > FLASH\n.data : { *(.data*) } > RAM AT>FLASH\n.bss(NOLOAD) : { *(.bss*) *(COMMON) } > RAM\n/DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) } }\n')
vendor=PROJECT/'Middlewares/Third_Party/LVGL'
sources=[HERE/'test_capture.c',PROJECT/'Drivers/BSP/src/bsp_display_capture.c',PROJECT/'Graphics/Port/src/graphics_eve_ramg_guard.c',vendor/'src/draw/eve/lv_draw_eve_ram_g.c']
reports=[]
for optimization in ('O0','Os'):
    elf=output/f'capture-{optimization}.elf'
    cmd=[str(CC/'arm-none-eabi-gcc.exe'),str(next(q for q in Path(__file__).resolve().parents if (q/'tools/build.ps1').is_file())/'Drivers/BSP/src/noodoe_crc32.c'),'-I'+str(next(q for q in Path(__file__).resolve().parents if (q/'tools/build.ps1').is_file())/'Drivers/BSP/inc'),'-mcpu=cortex-m4','-mthumb','-std=c11',f'-{optimization}','-Wall','-Wextra','-Werror','-ffreestanding','-fno-builtin','-ffunction-sections','-fdata-sections','-nostdlib','-DLV_CONF_INCLUDE_SIMPLE','-DNOODOE_INTEGRATED=1',
         '-I'+str(HERE/'stubs'),'-I'+str(PROJECT/'Drivers/BSP/inc'),'-I'+str(PROJECT/'Graphics/Port/inc'),'-I'+str(vendor),*map(str,sources),'-Wl,-T,'+str(linker),'-Wl,--gc-sections','-Wl,--wrap=lv_draw_eve_ramg_get_addr','-Wl,-e,Capture_TestMain','-o',str(elf),'-lgcc']
    compiled=subprocess.run(cmd,capture_output=True,text=True);(output/f'compile-{optimization}.log').write_text(compiled.stdout+compiled.stderr)
    if compiled.returncode:raise RuntimeError(compiled.stderr)
    listing=subprocess.check_output([str(CC/'arm-none-eabi-nm.exe'),str(elf)],text=True)
    symbols={p[2]:int(p[0],16) for line in listing.splitlines() if len(p:=line.split())==3}
    emu=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS);emu.mem_map(0x10000000,0x10000);emu.mem_map(0x20000000,0x20000);emu.mem_map(0xc0000000,0x100000)
    data=elf.read_bytes();header=struct.unpack_from('<16sHHIIIIIHHHHHH',data)
    for i in range(header[10]):
        kind,offset,address,_,size,_,_,_=struct.unpack_from('<IIIIIIII',data,header[5]+i*header[9])
        if kind==1 and size:emu.mem_write(address,data[offset:offset+size])
    stop=0x1000FFF0;emu.mem_write(stop,b'\x00\xbe');emu.reg_write(UC_ARM_REG_SP,0x2001FFF0);emu.reg_write(UC_ARM_REG_LR,stop|1);emu.reg_write(UC_ARM_REG_XPSR,0x01000000)
    # Includes one additional complete480x480 capture before asleep-download checks.
    emu.emu_start(symbols['Capture_TestMain']|1,stop,timeout=20_000_000,count=100_000_000)
    word=lambda name:struct.unpack('<I',emu.mem_read(symbols[name],4))[0]
    report={'optimization':optimization,'returned':emu.reg_read(UC_ARM_REG_PC)==stop,'result':emu.reg_read(UC_ARM_REG_R0),'assertions':word('g_assertions'),'failure_line':word('g_failure_line'),'mock_error':word('g_mock_error'),'crc_matches_zlib':word('g_test_crc')==zlib.crc32(bytes(((i+17)*7+3)&255 for i in range(4096))),
            'sources_sha256':{str(p.relative_to(PROJECT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources},'hardware_access':False}
    reports.append(report);print(json.dumps(report),flush=True);(output/'results.json').write_text(json.dumps(reports,indent=2)+'\n')
    if not report['returned'] or report['result'] or report['failure_line'] or report['mock_error'] or not report['crc_matches_zlib']:raise SystemExit(1)
