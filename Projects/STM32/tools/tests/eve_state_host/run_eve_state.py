"""Compile unchanged upstream lv_eve.c plus extracted adapter code for ARM/Unicorn.

This is a bounded command-stream test. No Cube project build, programmer, GUI,
SPI, or physical target is involved. Assertions execute C, not a Python rewrite.
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


def block(text: str, pattern: str) -> str:
    """Extract a complete original C block, ignoring braces inside comments/text."""
    matches = list(re.finditer(pattern, text, re.M))
    if len(matches) != 1:
        raise RuntimeError(f'Expected one production block, found {len(matches)}: {pattern}')
    start = matches[0].start()
    depth = 0
    tokens = r'/\*[\s\S]*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\x27(?:\\.|[^\x27\\])*\x27|[{}]'
    for token in re.finditer(tokens, text[start:]):
        if token.group() == '{': depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0: return text[start:start + token.end()]
    raise RuntimeError('Unbalanced production C block')


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--toolchain', type=Path, default=TOOLCHAIN)
    parser.add_argument('--output', type=Path, default=FIXTURE / 'output')
    args = parser.parse_args()
    sys.path.insert(0, str(RESEARCH / '.tools/analysis-python'))
    from unicorn import Uc, UC_ARCH_ARM, UC_MODE_MCLASS, UC_MODE_THUMB
    from unicorn.arm_const import UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_R0, UC_ARM_REG_SP, UC_ARM_REG_XPSR
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    vendor = PROJECT / 'Middlewares/Third_Party/LVGL'
    source = vendor / 'src/draw/eve/lv_eve.c'
    fill_source = vendor / 'src/draw/eve/lv_draw_eve_fill.c'
    geometry = PROJECT / 'Graphics/Port/src/graphics_geometry.c'
    viewport = PROJECT / 'Graphics/Port/src/graphics_eve_viewport.c'
    viewport_header = PROJECT / 'Graphics/Port/inc/Graphics_Viewport.h'
    area_source = vendor / 'src/misc/lv_area.c'
    adapter = PROJECT / 'Graphics/Port/src/graphics_eve_port.c'
    config = PROJECT / 'Graphics/Port/inc/lv_conf.h'
    adapter_text = adapter.read_text(encoding='utf-8')
    dispatch = block(adapter_text, r'^static int32_t EveDispatch\(lv_draw_unit_t \*unit, lv_layer_t \*layer\)\s*\{')
    render = block(adapter_text, r'^static void EveRenderStart\(lv_event_t \*event\)\s*\{')
    bitmap = block(render, r'^[ \t]*if \(bitmap_baseline_pending\)\s*\{')
    (output / 'dispatch_from_production.inc').write_text(dispatch + '\n', encoding='utf-8')
    (output / 'bitmap_seed_from_production.inc').write_text(
        'static void SeedBitmapBaseline(void)\n{\n' + bitmap + '\n}\n', encoding='utf-8')
    linker = output / 'test.ld'
    linker.write_text('MEMORY { FLASH(rx): ORIGIN = 0x10000000, LENGTH = 64K\n'
        'RAM(rwx): ORIGIN = 0x20000000, LENGTH = 128K }\n'
        'SECTIONS { .text : { *(.text*) *(.rodata*) } > FLASH\n'
        '.data : { *(.data*) } > RAM AT>FLASH\n'
        '.bss (NOLOAD) : { *(.bss*) *(COMMON) } > RAM\n'
        '/DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment*) } }\n', encoding='utf-8')
    results = []
    for optimization in ('O0', 'Os'):
        elf = output / f'eve-state-{optimization}.elf'
        command = [str(args.toolchain / 'arm-none-eabi-gcc.exe'), '-std=c11', '-mcpu=cortex-m4',
            '-mthumb', '-mfloat-abi=soft', f'-{optimization}', '-Wall', '-Wextra', '-Werror',
            '-ffreestanding', '-fno-builtin', '-fdata-sections', '-ffunction-sections',
            '-fno-unwind-tables', '-nostdlib', '-DLV_CONF_INCLUDE_SIMPLE', f'-I{output}',
            f'-I{PROJECT / "Graphics/Port/inc"}', f'-I{vendor}',
            str(FIXTURE / 'test_eve_state.c'), str(source), str(PROJECT / "Graphics/Port/src/graphics_eve_clip.c"), str(fill_source), str(geometry), str(viewport), str(area_source),
            f'-Wl,-T,{linker}', '-Wl,--gc-sections', '-Wl,-e,EveState_TestMain', '-o', str(elf), '-lgcc']
        compiled = subprocess.run(command, text=True, capture_output=True)
        (output / f'compile-{optimization}.log').write_text(compiled.stdout + compiled.stderr, encoding='utf-8')
        (output / f'command-{optimization}.json').write_text(json.dumps(command, indent=2), encoding='utf-8')
        if compiled.returncode:
            raise RuntimeError(f'{optimization} fixture compilation failed: {compiled.stderr}')
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
        emu.emu_start(symbols['EveState_TestMain'] | 1, stop, timeout=20_000_000, count=5_000_000)
        def word(name): return struct.unpack('<I', emu.mem_read(symbols[name], 4))[0]
        result = dict(optimization=optimization, returned_to_sentinel=emu.reg_read(UC_ARM_REG_PC) == stop,
            return_code=emu.reg_read(UC_ARM_REG_R0), last_case=word('g_test_case'),
            failure_line=word('g_test_failure_line'), assertions=word('g_test_assertions'),
            mock_error=word('g_test_mock_error'), upstream_source_sha256=digest(source),
            adapter_source_sha256=digest(adapter), config_sha256=digest(config), fixture_sha256=digest(FIXTURE / 'test_eve_state.c'),
            dispatch_extracted_sha256=hashlib.sha256(dispatch.encode()).hexdigest(),
            bitmap_extracted_sha256=hashlib.sha256(bitmap.encode()).hexdigest(),
            geometry_source_sha256=digest(geometry), viewport_source_sha256=digest(viewport),
            viewport_header_sha256=digest(viewport_header), area_source_sha256=digest(area_source),
            fill_source_sha256=digest(fill_source), hardware_access=False, elf=str(elf), elf_sha256=digest(elf))
        result['passed'] = (result['returned_to_sentinel'] and result['return_code'] == 0 and
            result['failure_line'] == 0 and result['mock_error'] == 0 and result['last_case'] == 15)
        results.append(result)
        (output / 'results.json').write_text(json.dumps(results, indent=2), encoding='utf-8')
        print(json.dumps(result), flush=True)
        if not result['passed']: return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
