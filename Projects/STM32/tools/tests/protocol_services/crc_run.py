"""실제 BSP CRC C를 ARM으로 실행한다. 레지스터 모형은 실기 검증을 대신하지 않는다."""
import hashlib
import json
import struct
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parents[2]
REVERSING = PROJECT.parents[1]
sys.path.insert(0, str(REVERSING / '.tools/analysis-python'))
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS
from unicorn.arm_const import (
    UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_R0, UC_ARM_REG_R1,
    UC_ARM_REG_XPSR, UC_ARM_REG_PC,
)

TC = Path('C:/ST/STM32CubeIDE_1.18.1/STM32CubeIDE/plugins/'
          'com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344/tools/bin')
OUT = HERE / 'crc_output'
OUT.mkdir(parents=True, exist_ok=True)
SOURCE = PROJECT / 'Drivers/BSP/src/BSP_CRC.c'
HEADER = PROJECT / 'Drivers/BSP/inc/BSP_CRC.h'
STOP = 0x1001FFF0
INPUT = 0x11000000
LINKER = OUT / 'crc_test.ld'
LINKER.write_text('''MEMORY {
 FLASH(rx): ORIGIN = 0x10000000, LENGTH = 128K
 RAM(rwx): ORIGIN = 0x20000000, LENGTH = 128K
}
SECTIONS {
 .text : { *(.text*) *(.rodata*) } > FLASH
 .data : { *(.data*) } > RAM
 .bss (NOLOAD) : { *(.bss*) *(COMMON) } > RAM
 /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) }
}
''')


def call(uc, symbols, name, r0=0, r1=0, limit=200_000_000):
    """C ABI로 실제 Thumb 함수를 호출하고 반환/명령 수 제한을 검사한다."""
    uc.reg_write(UC_ARM_REG_XPSR, 0x1000000)
    uc.reg_write(UC_ARM_REG_SP, 0x2001FFF0)
    uc.reg_write(UC_ARM_REG_LR, STOP | 1)
    uc.reg_write(UC_ARM_REG_R0, r0)
    uc.reg_write(UC_ARM_REG_R1, r1)
    uc.emu_start(symbols[name] | 1, STOP, count=limit, timeout=50_000_000)
    if uc.reg_read(UC_ARM_REG_PC) != STOP:
        raise RuntimeError(f'{name}: 실행 제한 안에 반환하지 않았다')
    return uc.reg_read(UC_ARM_REG_R0)


def read32(uc, address):
    """ARM little-endian 결과 word를 읽으며 대상 메모리는 수정하지 않는다."""
    return struct.unpack('<I', uc.mem_read(address, 4))[0]


def load_elf(path):
    """ELF32의 실제 load segment를 적재한다. C 계산을 Python으로 대체하지 않는다."""
    nm = subprocess.check_output([str(TC / 'arm-none-eabi-nm.exe'), str(path)], text=True)
    symbols = {line.split()[2]: int(line.split()[0], 16)
               for line in nm.splitlines() if len(line.split()) == 3}
    data = path.read_bytes()
    phoff = struct.unpack_from('<I', data, 28)[0]
    phsize, phnum = struct.unpack_from('<HH', data, 42)
    uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
    uc.mem_map(0x10000000, 0x20000)
    uc.mem_map(0x20000000, 0x20000)
    uc.mem_map(INPUT, 0x80000)
    for index in range(phnum):
        kind, offset, va, _, filesz, _, _, _ = struct.unpack_from('<8I', data, phoff + index * phsize)
        if kind == 1 and filesz:
            uc.mem_write(va, data[offset:offset + filesz])
    return uc, symbols


# 제품 CMSIS 경로도 별도로 컴파일한다. 모의 헤더가 제품 레지스터 오타를 숨기지 않는다.
common = ['-std=c11', '-mcpu=cortex-m4', '-mthumb', '-mfloat-abi=soft',
          '-Wall', '-Wextra', '-Werror', '-ffreestanding', '-fno-builtin',
          '-I', str(PROJECT / 'Drivers/BSP/inc')]
production = subprocess.run([
    str(TC / 'arm-none-eabi-gcc.exe'), *common, '-Os', '-DSTM32F429xx',
    '-I', str(PROJECT / 'Drivers/CMSIS/Include'),
    '-I', str(PROJECT / 'Drivers/CMSIS/Device/ST/STM32F4xx/Include'),
    '-c', str(SOURCE), '-o', str(OUT / 'crc_production.o'),
], capture_output=True, text=True)
if production.returncode:
    raise RuntimeError(production.stdout + production.stderr)

results = []
for optimization in ('-O0', '-Os'):
    elf = OUT / f'crc_{optimization[1:]}.elf'
    build = subprocess.run([
        str(TC / 'arm-none-eabi-gcc.exe'), *common, optimization,
        '-DBSP_CRC_TEST_PORT', '-I', str(HERE), '-nostdlib',
        '-fno-unwind-tables', '-fno-asynchronous-unwind-tables',
        str(SOURCE), str(HERE / 'crc_test.c'),
        '-T', str(LINKER), '-Wl,-e,crc_test_main', '-lgcc', '-o', str(elf),
    ], capture_output=True, text=True)
    if build.returncode:
        raise RuntimeError(build.stdout + build.stderr)
    uc, symbols = load_elf(elf)
    failure = call(uc, symbols, 'crc_test_main')
    if failure:
        raise RuntimeError(f'{optimization}: crc_test.c CHECK 실패, line {failure}')
    row = dict(optimization=optimization,
               assertions=read32(uc, symbols['crc_test_assertions']), status='pass')
    print(json.dumps(row), flush=True)

    # 실제 순정 이미지 네 개를 -Os의 실제 C에 그대로 공급한다. 기존 분석 결과는
    # trailer 및 residue 기대값으로만 사용하고 대상 원본은 변경하지 않는다.
    if optimization == '-Os':
        evidence = REVERSING / 'analysis/2026-09-09-ak550-boot-update-audit/crc-independent-verification.json'
        originals = []
        for entry in json.loads(evidence.read_text(encoding='utf-8')):
            firmware = REVERSING / entry['file']
            payload = firmware.read_bytes()
            uc.mem_write(INPUT, payload)
            stored = struct.unpack_from('<I', payload, len(payload) - 4)[0]
            if call(uc, symbols, 'crc_test_external', INPUT, len(payload) - 4) != 0:
                raise RuntimeError('원본 이미지 CRC 계산 오류')
            calculated = read32(uc, symbols['crc_test_external_result'])
            if calculated != stored or stored != int(entry['stored'], 16):
                raise RuntimeError(f'{firmware.name}: trailer 불일치')
            # 기존 prefix 계산에서 full-image residue를 별도 context로 다시 계산한다.
            if call(uc, symbols, 'crc_test_external', INPUT, len(payload)) != 0:
                raise RuntimeError('원본 이미지 residue 계산 오류')
            residue = read32(uc, symbols['crc_test_external_result'])
            if residue != 0:
                raise RuntimeError(f'{firmware.name}: residue 불일치')
            original = dict(file=str(firmware.relative_to(REVERSING)), bytes=len(payload),
                            sha256=hashlib.sha256(payload).hexdigest(),
                            trailer=f'0x{stored:08X}', actual_c_crc=f'0x{calculated:08X}',
                            actual_c_residue=residue)
            originals.append(original)
            print(json.dumps(original), flush=True)
        row['original_images'] = originals
    results.append(row)

report = dict(results=results, production_cmsis_compile='pass',
              sources={str(path.relative_to(PROJECT)): hashlib.sha256(path.read_bytes()).hexdigest()
                       for path in (SOURCE, HEADER, HERE / 'crc_test.c', HERE / 'crc_test_port.h', Path(__file__))},
              limits='실제 ARM C를 실행하되 CRC/RCC/IRQ 경계는 모형이다. 실기 CRC, AHB 지연, 인터럽트 선점 타이밍, 처리 시간은 미검증이다.')
(OUT / 'crc_results.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps(report, ensure_ascii=False, indent=2))
