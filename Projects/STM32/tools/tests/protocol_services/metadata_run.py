"""실제 metadata engine C의 ARM 실행과 제품 backend의 SRAM 의존성을 검사한다.

flash erase/program/IRQ의 물리 동작은 C 모델이며 실제 장치 검증이 아니다.
제품 backend는 별도로 Cortex-M4 object로 컴파일하고 RAM section relocation을
검사한다. 프로젝트/장치 빌드 명령을 실행하지 않는다.
"""
import hashlib
import json
import re
import struct
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parents[2]
sys.path.insert(0, str(PROJECT.parents[1] / '.tools/analysis-python'))
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_R0, UC_ARM_REG_XPSR, UC_ARM_REG_PC

TC = Path('C:/ST/STM32CubeIDE_1.18.1/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344/tools/bin')
OUT = HERE / 'metadata_output'
OUT.mkdir(parents=True, exist_ok=True)
SOURCE = PROJECT / 'Middlewares/Noodoe/Update/src/Update_Metadata.c'
HEADER = PROJECT / 'Middlewares/Noodoe/Update/inc/Update_Metadata.h'
TEST = HERE / 'metadata_test.c'
(OUT / 'metadata.ld').write_text('''MEMORY {
 FLASH(rx): ORIGIN = 0x10000000, LENGTH = 128K
 RAM(rwx): ORIGIN = 0x20000000, LENGTH = 192K
}
SECTIONS {
 .text : { *(.text*) *(.rodata*) } > FLASH
 .data : { *(.data*) *(.RamFunc*) } > RAM
 .bss (NOLOAD) : { *(.bss*) *(COMMON) } > RAM
 /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) }
}
''')
base = [str(TC / 'arm-none-eabi-gcc.exe'), '-mcpu=cortex-m4', '-mthumb',
        '-mfloat-abi=soft', '-std=c11', '-ffreestanding', '-fno-builtin',
        '-fdata-sections', '-ffunction-sections', '-fno-unwind-tables',
        '-Wall', '-Wextra', '-Werror', '-I', str(HEADER.parent)]
results = []
for opt in ('-O0', '-Os'):
    elf = OUT / (opt[1:] + '.elf')
    command = [*base, opt, '-DUPDATE_METADATA_TESTING', '-nostdlib', str(SOURCE), str(TEST),
               '-T', str(OUT / 'metadata.ld'), '-Wl,--gc-sections', '-Wl,-e,test_main', '-o', str(elf)]
    build = subprocess.run(command, capture_output=True, text=True)
    (OUT / (opt[1:] + '-build.log')).write_text(build.stdout + build.stderr, encoding='utf-8')
    if build.returncode:
        raise RuntimeError(build.stdout + build.stderr)
    nm = subprocess.check_output([str(TC / 'arm-none-eabi-nm.exe'), str(elf)], text=True)
    symbols = {line.split()[2]: int(line.split()[0], 16)
               for line in nm.splitlines() if len(line.split()) == 3}
    data = elf.read_bytes()
    phoff = struct.unpack_from('<I', data, 28)[0]
    phsize, phnum = struct.unpack_from('<HH', data, 42)
    uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
    uc.mem_map(0x10000000, 0x20000)
    uc.mem_map(0x20000000, 0x30000)
    for i in range(phnum):
        kind, offset, va, pa, filesz, memsz, flags, align = struct.unpack_from('<8I', data, phoff + i*phsize)
        if kind == 1 and filesz:
            uc.mem_write(va, data[offset:offset+filesz])
    uc.reg_write(UC_ARM_REG_XPSR, 0x1000000)
    uc.reg_write(UC_ARM_REG_SP, 0x2002fff0)
    stop = 0x1001fff0
    uc.reg_write(UC_ARM_REG_LR, stop | 1)
    uc.emu_start(symbols['test_main'] | 1, stop, timeout=60000000, count=500000000)
    diag = {name: struct.unpack('<I', uc.mem_read(symbols['g_test_'+name],4))[0]
            for name in ('suite','failure_line','assertions')}
    if uc.reg_read(UC_ARM_REG_PC) != stop or uc.reg_read(UC_ARM_REG_R0):
        raise RuntimeError(f'{opt}: actual ARM C failure: {diag}, PC={uc.reg_read(UC_ARM_REG_PC):08X}')
    # 제품의 실제 CMSIS/peripheral backend는 fake backend와 별도로 컴파일한다.
    obj = OUT / (opt[1:] + '-target.o')
    target = subprocess.run([*base,opt,'-DSTM32F429xx',
                            '-I',str(PROJECT/'Drivers/BSP/inc'),
                            '-I',str(PROJECT/'Drivers/CMSIS/Include'),
                            '-I',str(PROJECT/'Drivers/CMSIS/Device/ST/STM32F4xx/Include'),
                            '-c',str(SOURCE),'-o',str(obj)],capture_output=True,text=True)
    if target.returncode:
        raise RuntimeError(target.stdout+target.stderr)
    dump = subprocess.check_output([str(TC/'arm-none-eabi-objdump.exe'),'-t','-r','-d',str(obj)],text=True)
    (OUT/(opt[1:]+'-target.asm.txt')).write_text(dump,encoding='utf-8')
    reloc_dump = subprocess.check_output([str(TC/'arm-none-eabi-objdump.exe'),'-r',str(obj)],text=True)
    (OUT/(opt[1:]+'-target-relocations.txt')).write_text(reloc_dump,encoding='utf-8')
    # SRAM 함수에서 text/libc/HAL에 걸린 relocation은 허용하지 않는다.
    section = ''
    relocations = []
    for line in reloc_dump.splitlines():
        match = re.match(r'RELOCATION RECORDS FOR \[(.+)\]:',line)
        if match:
            section=match.group(1)
        elif not line.strip():
            section=''
        elif section.startswith('.RamFunc') and re.match(r'^[0-9a-f]{8}\s+R_ARM_',line):
            target_name=line.split()[2]
            relocations.append(target_name)
            if not target_name.startswith(('.bss.','.RamFunc')) and target_name not in ('BSP_Watchdog_RamCheckpoint',):
                raise RuntimeError(f'{opt}: RAM critical relocation to non-RAM target: {line}')
    if not relocations:
        raise RuntimeError('RAM relocation inspection found no entries')
    # Resolve the only external RAM call against its actual production object;
    # a name whitelist alone cannot prove flash-busy execution safety.
    watchdog=PROJECT/'Drivers/BSP/src/BSP_Watchdog.c';wdobj=OUT/(opt[1:]+'-watchdog.o')
    subprocess.run([*base,opt,'-DSTM32F429xx','-I',str(PROJECT/'Drivers/BSP/inc'),
        '-I',str(PROJECT/'Drivers/CMSIS/Include'),'-I',str(PROJECT/'Drivers/CMSIS/Device/ST/STM32F4xx/Include'),
        '-c',str(watchdog),'-o',str(wdobj)],check=True,capture_output=True)
    symbols=subprocess.check_output([str(TC/'arm-none-eabi-objdump.exe'),'-t',str(wdobj)],text=True)
    assert re.search(r'\.RamFunc\.watchdog\s+[0-9a-f]+\s+BSP_Watchdog_RamCheckpoint',symbols),symbols
    wdrel=subprocess.check_output([str(TC/'arm-none-eabi-objdump.exe'),'-r',str(wdobj)],text=True)
    (OUT/(opt[1:]+'-watchdog-relocations.txt')).write_text(wdrel)
    active=False
    for line in wdrel.splitlines():
        if line.startswith('RELOCATION RECORDS FOR'):active='[.RamFunc.watchdog]' in line
        elif not line.strip():active=False
        elif active and re.match(r'^[0-9a-f]{8}\s+R_ARM_',line):
            name=line.split()[2]
            assert name.startswith(('.bss.','.RamFunc')) or name in ('g_bsp_watchdog','BSP_Watchdog_RamCheckpoint'),line
    sizes=subprocess.check_output([str(TC/'arm-none-eabi-size.exe'),'-A',str(obj)],text=True)
    (OUT/(opt[1:]+'-target-size.txt')).write_text(sizes,encoding='utf-8')
    results.append(dict(optimization=opt,assertions=diag['assertions'],status='pass',
                        target_compile='pass',ram_relocations=sorted(set(relocations))))
report=dict(results=results,
            sources={str(path.relative_to(PROJECT)):hashlib.sha256(path.read_bytes()).hexdigest()
                     for path in (SOURCE,HEADER,TEST,Path(__file__))},
            limits='실제 ARM 엔진 + 가짜 flash 오류 모델. STM32 전압/BSY/IRQ/erase/캐시/전원 중단 및 실물 OTA는 검증하지 않음. 제품 backend compile/RAM relocation 확인은 실행 검증과 다름.')
(OUT/'results.json').write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf-8')
print(json.dumps(report,indent=2,ensure_ascii=False))
