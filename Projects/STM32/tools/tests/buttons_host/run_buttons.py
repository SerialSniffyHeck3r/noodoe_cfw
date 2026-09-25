"""실제 BSP_Buttons.c를 ARM GCC로 컴파일하여 호스트 Unicorn에서 실행한다.

Python은 빌드/ELF 로드/결과 수집만 한다. GPIO와 tick mock 및 기대값 검증은
test_buttons.c이며 production 버튼 상태 기계를 다른 언어로 복제하지 않는다.
Cube 생성/프로젝트 빌드/디버거/실물 보드에 접근하지 않는다.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys

PROJECT = Path(__file__).resolve().parents[3]
RESEARCH = PROJECT.parents[1]
FIXTURE = Path(__file__).resolve().parent
DEFAULT_TOOLCHAIN = Path(
    'C:/ST/STM32CubeIDE_1.18.1/STM32CubeIDE/plugins/'
    'com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344/tools/bin'
)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--toolchain', type=Path, default=DEFAULT_TOOLCHAIN)
    parser.add_argument('--output', type=Path,
                        default=RESEARCH / 'analysis/2026-09-12-buttons/host-tests')
    args = parser.parse_args()
    # 기존 리버싱용 임시 Python 패키지 경로다. 의존성을 몰래 다운로드하거나 전역
    # Python을 수정하지 않으며 Unicorn이 없으면 명확히 실패한다.
    sys.path.insert(0, str(RESEARCH / '.tools/analysis-python'))
    try:
        from unicorn import Uc, UC_ARCH_ARM, UC_MODE_MCLASS, UC_MODE_THUMB
        from unicorn.arm_const import (
            UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_R0, UC_ARM_REG_SP, UC_ARM_REG_XPSR,
        )
    except ImportError as error:
        raise RuntimeError('Unicorn 2.1.4 is required in Reversing/.tools/analysis-python') from error

    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    linker = output / 'test.ld'
    linker.write_text(
        'MEMORY { FLASH(rx): ORIGIN = 0x10000000, LENGTH = 64K\n'
        'RAM(rwx): ORIGIN = 0x20000000, LENGTH = 128K }\n'
        'SECTIONS { .text : { *(.text*) *(.rodata*) } > FLASH\n'
        '.data : { *(.data*) } > RAM AT>FLASH\n'
        '.bss (NOLOAD) : { *(.bss*) *(COMMON) } > RAM\n'
        '/DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment*) } }\n',
        encoding='utf-8',
    )
    source = PROJECT / 'Drivers/BSP/src/BSP_Buttons.c'
    header = PROJECT / 'Drivers/BSP/inc/BSP_Buttons.h'
    results = []
    for optimization in ('O0', 'O2'):
        elf = output / f'buttons-{optimization}.elf'
        command = [
            str(args.toolchain / 'arm-none-eabi-gcc.exe'), '-std=c11', '-mcpu=cortex-m4',
            '-mthumb', '-mfloat-abi=soft', f'-{optimization}', '-Wall', '-Wextra', '-Werror',
            '-ffreestanding', '-fno-builtin', '-fdata-sections', '-ffunction-sections',
            '-fno-unwind-tables', '-nostdlib', f'-I{FIXTURE}',
            f'-I{PROJECT / "Drivers/BSP/inc"}', str(FIXTURE / 'test_buttons.c'), str(source),
            f'-Wl,-T,{linker}', '-Wl,--gc-sections', '-Wl,-e,Buttons_TestMain',
            '-o', str(elf), '-lgcc',
        ]
        compilation = subprocess.run(command, text=True, capture_output=True)
        (output / f'compile-{optimization}.log').write_text(
            compilation.stdout + compilation.stderr, encoding='utf-8')
        if compilation.returncode:
            raise RuntimeError(f'{optimization} compilation failed:\n{compilation.stderr}')

        symbols = {}
        listing = subprocess.check_output(
            [str(args.toolchain / 'arm-none-eabi-nm.exe'), str(elf)], text=True)
        for line in listing.splitlines():
            fields = line.split()
            if len(fields) == 3:
                symbols[fields[2]] = int(fields[0], 16)

        emulator = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
        emulator.mem_map(0x10000000, 0x10000)
        emulator.mem_map(0x20000000, 0x20000)
        data = elf.read_bytes()
        elf_header = struct.unpack_from('<16sHHIIIIIHHHHHH', data)
        # 이 실행기가 방금 만든 32bit little-endian ELF만 로드한다.
        if data[:7] != b'\x7fELF\x01\x01\x01':
            raise RuntimeError('Expected an ELF32 little-endian ARM test executable')
        for index in range(elf_header[10]):
            record = struct.unpack_from('<IIIIIIII', data,
                                        elf_header[5] + index * elf_header[9])
            kind, offset, address, _physical, size, _memsize, _flags, _align = record
            if kind == 1 and size:
                emulator.mem_write(address, data[offset:offset + size])

        stop = 0x1000FFF0
        emulator.mem_write(stop, b'\x00\xbe')
        emulator.reg_write(UC_ARM_REG_SP, 0x2001FFF0)
        emulator.reg_write(UC_ARM_REG_LR, stop | 1)
        emulator.reg_write(UC_ARM_REG_XPSR, 0x01000000)
        emulator.emu_start(symbols['Buttons_TestMain'] | 1, stop,
                           timeout=20_000_000, count=5_000_000)

        def word(symbol: str) -> int:
            return struct.unpack('<I', emulator.mem_read(symbols[symbol], 4))[0]

        result = {
            'optimization': optimization,
            'returned_to_sentinel': emulator.reg_read(UC_ARM_REG_PC) == stop,
            'return_code': emulator.reg_read(UC_ARM_REG_R0),
            'last_case': word('g_test_case'),
            'failure_line': word('g_test_failure_line'),
            'assertions': word('g_test_assertions'),
            'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
            'header_sha256': hashlib.sha256(header.read_bytes()).hexdigest(),
            'elf': str(elf),
        }
        result['passed'] = (result['returned_to_sentinel'] and result['return_code'] == 0
                            and result['failure_line'] == 0 and result['last_case'] == 10)
        results.append(result)
        print(json.dumps(result))
        (output / 'results.json').write_text(json.dumps(results, indent=2) + '\n', encoding='utf-8')
        if not result['passed']:
            return 1
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        print(f'FAIL: {error}', file=sys.stderr)
        raise SystemExit(1)
