"""Run real StorageService journal under ARM Unicorn; never touches hardware."""
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import re

HERE=Path(__file__).resolve().parent
PROJECT=HERE.parents[2]
CC=Path('C:/ST/STM32CubeIDE_1.18.1/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344/tools/bin')
sys.path.insert(0,str(PROJECT.parents[1]/'.tools/analysis-python'))
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_LR,UC_ARM_REG_PC,UC_ARM_REG_R0,UC_ARM_REG_SP,UC_ARM_REG_XPSR

output=HERE/'output';output.mkdir(exist_ok=True)
nor=(PROJECT/'Drivers/BSP/src/BSP_NOR.c').read_text(encoding='utf-8')
matches=list(re.finditer(r'^static uint32_t WriteRangeAllowed\([^\n]+\)\n\{[\s\S]*?^\}',nor,re.M))
if len(matches)!=1:raise RuntimeError('Production write range guard missing or ambiguous')
(output/'write_range.inc').write_text('/* Legacy fixture grants no fixed-file capabilities; actual BSP tests cover those. */\nstatic uint32_t ContainerRange(uint32_t c,uint32_t a,uint32_t n){(void)c;(void)a;(void)n;return 0;}\n'+matches[0].group()+'\n')
linker=output/'test.ld'
linker.write_text('MEMORY { FLASH(rx): ORIGIN = 0x10000000, LENGTH = 64K\n RAM(rwx): ORIGIN = 0x20000000, LENGTH = 256K }\nSECTIONS { .text : { *(.text*) *(.rodata*) } > FLASH\n.data : { *(.data*) } > RAM AT>FLASH\n.bss(NOLOAD) : { *(.bss*) *(COMMON) } > RAM\n/DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) } }\n')
results=[]
for optimization in ('O0','Os'):
    elf=output/f'journal-{optimization}.elf'
    command=[str(CC/'arm-none-eabi-gcc.exe'),str(next(q for q in Path(__file__).resolve().parents if (q/'tools/build.ps1').is_file())/'Drivers/BSP/src/noodoe_crc32.c'),'-I'+str(next(q for q in Path(__file__).resolve().parents if (q/'tools/build.ps1').is_file())/'Drivers/BSP/inc'),'-std=c11','-mcpu=cortex-m4','-mthumb',f'-{optimization}',
             '-Wall','-Wextra','-Werror','-ffreestanding','-fno-builtin','-ffunction-sections','-fdata-sections','-nostdlib',
             '-I'+str(output),'-I'+str(HERE/'stubs'),'-I'+str(PROJECT/'Middlewares/Noodoe/Storage/inc'),'-I'+str(PROJECT/'Middlewares/Noodoe/Storage/src'),
             '-I'+str(PROJECT/'Middlewares/Third_Party/FatFs/src'),str(HERE/'test_journal.c'),
             '-Wl,-T,'+str(linker),'-Wl,--gc-sections','-Wl,-e,StorageJournal_TestMain','-o',str(elf),'-lgcc']
    result=subprocess.run(command,capture_output=True,text=True)
    (output/f'compile-{optimization}.log').write_text(result.stdout+result.stderr)
    if result.returncode:raise RuntimeError(result.stderr)
    listing=subprocess.check_output([str(CC/'arm-none-eabi-nm.exe'),str(elf)],text=True)
    symbols={p[2]:int(p[0],16) for line in listing.splitlines() if len(p:=line.split())==3}
    emu=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS);emu.mem_map(0x10000000,0x10000);emu.mem_map(0x20000000,0x40000)
    data=elf.read_bytes();header=struct.unpack_from('<16sHHIIIIIHHHHHH',data)
    for i in range(header[10]):
        kind,offset,address,_,size,_,_,_=struct.unpack_from('<IIIIIIII',data,header[5]+i*header[9])
        if kind==1 and size:emu.mem_write(address,data[offset:offset+size])
    stop=0x1000FFF0;emu.mem_write(stop,b'\x00\xbe');emu.reg_write(UC_ARM_REG_SP,0x2003FFF0)
    emu.reg_write(UC_ARM_REG_LR,stop|1);emu.reg_write(UC_ARM_REG_XPSR,0x01000000)
    emu.emu_start(symbols['StorageJournal_TestMain']|1,stop,timeout=30_000_000,count=80_000_000)
    word=lambda name:struct.unpack('<I',emu.mem_read(symbols[name],4))[0]
    report={'optimization':optimization,'returned':emu.reg_read(UC_ARM_REG_PC)==stop,'result':emu.reg_read(UC_ARM_REG_R0),
            'assertions':word('g_assertions'),'failure_line':word('g_failure_line'),'mock_error':word('g_mock_error'),
            'source_sha256':hashlib.sha256((PROJECT/'Middlewares/Noodoe/Storage/src/StorageService.c').read_bytes()).hexdigest(),
            'hardware_access':False}
    results.append(report);print(json.dumps(report),flush=True)
    (output/'results.json').write_text(json.dumps(results,indent=2)+'\n')
    if not report['returned'] or report['result'] or report['failure_line'] or report['mock_error']:raise SystemExit(1)
