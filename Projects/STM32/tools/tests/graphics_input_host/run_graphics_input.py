"""실제 graphics_input.c와 원본 Shutdown을 ARM/Unicorn으로 시험한다.

펌웨어 프로젝트 빌드/STM32 programmer/실물 보드 호출은 없다. 실제 header를
사용하고 LVGL/BSP/RTOS 경계만 fixture로 바꾼다. 결과에는 원본 해시를 남긴다.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import sys

sys.dont_write_bytecode = True
FIXTURE = Path(__file__).resolve().parent
PROJECT = FIXTURE.parents[2]
RESEARCH = PROJECT.parents[1]
TOOLCHAIN = Path('C:/ST/STM32CubeIDE_1.18.1/STM32CubeIDE/plugins/'
    'com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344/tools/bin')


def original_function(text: str, name: str) -> str:
    """함수 몸체를 다시 구현하지 않는다. 주석/문자열의 괄호는 제외하여 원본의
    정의 한 개를 그대로 추출한다. 원본 시그니처가 바뀌면 추측하지 않고 실패한다.
    """
    match = re.search(rf'^Graphics_Status {re.escape(name)}\(void\)\s*\{{', text, re.M)
    if not match:
        raise RuntimeError(f'Cannot find unchanged {name}(void) definition')
    depth = 0
    tokens = r'/\*[\s\S]*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\x27(?:\\.|[^\x27\\])*\x27|[{}]'
    for token in re.finditer(tokens, text[match.start():]):
        if token.group() == '{': depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0: return text[match.start():match.start() + token.end()]
    raise RuntimeError('Unbalanced C function; no extracted replacement produced')


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--toolchain', type=Path, default=TOOLCHAIN)
    parser.add_argument('--output', type=Path, default=FIXTURE / 'output')
    parser.add_argument("--product", action="store_true")
    args = parser.parse_args()
    sys.path.insert(0, str(RESEARCH / '.tools/analysis-python'))
    from unicorn import Uc, UC_ARCH_ARM, UC_MODE_MCLASS, UC_MODE_THUMB
    from unicorn.arm_const import UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_R0, UC_ARM_REG_SP, UC_ARM_REG_XPSR
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    source = PROJECT / 'Graphics/Port/src/graphics_input.c'
    product_sources=sorted((PROJECT/'App_Logic/UI/src').glob('ui_*.c'))+[PROJECT/'App_Logic/UI/src/product_input.c']
    lifecycle = PROJECT / 'Graphics/Port/src/graphics.c'
    extracted = original_function(lifecycle.read_text(encoding='utf-8'), 'Graphics_Shutdown')
    shutdown_file = output / 'shutdown_from_production.c'
    shutdown_file.write_text('#include "test_support.h"\n' + extracted + '\n', encoding='utf-8')
    linker = output / 'test.ld'
    linker.write_text('MEMORY { FLASH(rx): ORIGIN = 0x10000000, LENGTH = 64K\n'
        'RAM(rwx): ORIGIN = 0x20000000, LENGTH = 128K }\n'
        'SECTIONS { .text : { *(.text*) *(.rodata*) } > FLASH\n'
        '.data : { *(.data*) } > RAM AT>FLASH\n'
        '.bss (NOLOAD) : { *(.bss*) *(COMMON) } > RAM\n'
        '/DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment*) } }\n', encoding='utf-8')
    results = []
    for optimization in ('O0', 'O2'):
        elf = output / f'graphics-input-{optimization}.elf'
        command = [str(args.toolchain / 'arm-none-eabi-gcc.exe'), '-std=c11', '-mcpu=cortex-m4',
            '-mthumb', '-mfloat-abi=soft', f'-{optimization}', '-Wall', '-Wextra', '-Werror',
            '-ffreestanding', '-fno-builtin', '-fdata-sections', '-ffunction-sections',
            '-fno-unwind-tables', '-nostdlib', f'-I{FIXTURE}',
            '-DNOODOE_PRODUCT='+str(int(args.product)), f'-I{PROJECT / "Graphics/Port/inc"}', f'-I{PROJECT / "Drivers/BSP/inc"}',
            f'-I{PROJECT / "App_Logic/UI/inc"}', f'-I{PROJECT / "Middlewares/Noodoe/Input/inc"}',
            str(PROJECT / 'Middlewares/Noodoe/Input/src/ButtonEvents.c'),
            str(PROJECT / 'Middlewares/Noodoe/Input/src/ButtonFeedback.c'),
            str(FIXTURE / 'test_graphics_input.c'), str(source), str(shutdown_file),
            *map(str,product_sources),
            f'-Wl,-T,{linker}', '-Wl,--gc-sections', '-Wl,-e,GraphicsInput_TestMain', '-o', str(elf), '-lgcc']
        compiled = subprocess.run(command, text=True, capture_output=True)
        (output / f'compile-{optimization}.log').write_text(compiled.stdout + compiled.stderr, encoding='utf-8')
        if compiled.returncode:
            raise RuntimeError(f'{optimization} host fixture compilation failed: {compiled.stderr}')
        listing = subprocess.check_output([str(args.toolchain / 'arm-none-eabi-nm.exe'), str(elf)], text=True)
        symbols = {fields[2]: int(fields[0], 16) for line in listing.splitlines()
                   if len(fields := line.split()) == 3}
        emu = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
        emu.mem_map(0x10000000, 0x10000)
        emu.mem_map(0x20000000, 0x20000)
        data = elf.read_bytes()
        if data[:7] != b'\x7fELF\x01\x01\x01': raise RuntimeError('Not an ELF32 little-endian fixture')
        header = struct.unpack_from('<16sHHIIIIIHHHHHH', data)
        for index in range(header[10]):
            record = struct.unpack_from('<IIIIIIII', data, header[5] + index * header[9])
            kind, offset, address, _, size, _, _, _ = record
            if kind == 1 and size: emu.mem_write(address, data[offset:offset + size])
        stop = 0x1000FFF0
        emu.mem_write(stop, b'\x00\xbe')
        emu.reg_write(UC_ARM_REG_SP, 0x2001FFF0)
        emu.reg_write(UC_ARM_REG_LR, stop | 1)
        emu.reg_write(UC_ARM_REG_XPSR, 0x01000000)
        emu.emu_start(symbols['GraphicsInput_TestMain'] | 1, stop, timeout=20_000_000, count=5_000_000)
        def word(name): return struct.unpack('<I', emu.mem_read(symbols[name], 4))[0]
        result = dict(optimization=optimization, returned_to_sentinel=emu.reg_read(UC_ARM_REG_PC) == stop,
            return_code=emu.reg_read(UC_ARM_REG_R0), last_case=word('g_test_case'),
            failure_line=word('g_test_failure_line'), assertions=word('g_test_assertions'),
            source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
            shutdown_source_sha256=hashlib.sha256(lifecycle.read_bytes()).hexdigest(),
            extracted_shutdown_sha256=hashlib.sha256(extracted.encode()).hexdigest(),
            product_source_sha256={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in product_sources},
            hardware_access=False, elf=str(elf))
        result['passed'] = (result['returned_to_sentinel'] and result['return_code'] == 0 and
                            result['failure_line'] == 0 and result['last_case'] == (100 if args.product else 13))
        results.append(result)
        (output / 'results.json').write_text(json.dumps(results, indent=2), encoding='utf-8')
        print(json.dumps(result), flush=True)
        if not result['passed']: return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
