"""실제 arc/geometry C를 ARM에서 실행하고 LVGL 경계만 대체한다."""
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
OUT = HERE / 'output'
OUT.mkdir(parents=True, exist_ok=True)
(OUT / 'test.ld').write_text('''MEMORY {
 FLASH(rx): ORIGIN = 0x10000000, LENGTH = 64K
 RAM(rwx): ORIGIN = 0x20000000, LENGTH = 64K
}
SECTIONS {
 .text : { *(.text*) *(.rodata*) } > FLASH
 .data : { *(.data*) } > RAM
 .bss (NOLOAD) : { *(.bss*) *(COMMON) } > RAM
}
''')
sources = [PROJECT / 'Graphics/UI/src/Graphics_ArcSweep.c',
           PROJECT / 'Graphics/Port/src/graphics_geometry.c']
contracts = [PROJECT / 'Graphics/UI/inc/Graphics_ArcSweep.h',
             PROJECT / 'Graphics/Port/inc/Graphics_Viewport.h',
             PROJECT / 'Graphics/UI/inc/Graphics_BringupHUD.h']
# Match the actual linked font heights rather than assuming the font size is
# the glyph line height. The latter changes the rectangle containment test.
fonts = [PROJECT / f'Middlewares/Third_Party/lvgl/src/font/lv_font_montserrat_{n}.c'
         for n in (14, 40)]
heights = [int(re.search(r'\.line_height\s*=\s*(\d+)', p.read_text(encoding='utf-8')).group(1))
           for p in fonts]
(OUT / 'font_metrics.h').write_text(
    f'#define ARC_FONT14_HEIGHT {heights[0]}\n#define ARC_FONT40_HEIGHT {heights[1]}\n')


def build_and_load(opt, profile, mutation=False):
    name = f'{profile}-{opt[1:]}' + ('-old-title-width' if mutation else '')
    elf = OUT / (name + '.elf')
    build_sources = sources.copy()
    if mutation:
        original = sources[0].read_text(encoding='utf-8')
        marker = 'Label("DEMO SPEED", -68, 120,'
        if original.count(marker) != 1:
            raise RuntimeError('The integrated title changed; update the historical-failure mutation explicitly.')
        mutated = OUT / 'Graphics_ArcSweep_old_title_width.c'
        mutated.write_text(original.replace(marker, 'Label("DEMO SPEED", -68, 140,'), encoding='utf-8')
        build_sources[0] = mutated
    cmd = [str(TC / 'arm-none-eabi-gcc.exe'), '-mcpu=cortex-m4', '-mthumb',
           '-std=c11', opt, '-ffreestanding', '-fno-builtin', '-nostdlib',
           '-Wall', '-Wextra', '-Werror', '-I', str(HERE),
           '-I', str(OUT), f'-DNOODOE_INTEGRATED={int(profile == "integrated")}',
           '-I', str(PROJECT / 'Graphics/UI/inc'),
           '-I', str(PROJECT / 'Graphics/Port/inc'),
           *map(str, build_sources), str(HERE / 'test.c'),
           '-T', str(OUT / 'test.ld'), '-Wl,-e,test_main', '-lgcc', '-o', str(elf)]
    build = subprocess.run(cmd, capture_output=True, text=True)
    if build.returncode:
        raise RuntimeError(build.stdout + build.stderr)
    nm = subprocess.check_output([str(TC / 'arm-none-eabi-nm.exe'), str(elf)], text=True)
    symbols = {line.split()[2]: int(line.split()[0], 16)
               for line in nm.splitlines() if len(line.split()) == 3}
    # 실제 ELF load segment를 적재하고 test_main을 실행한다. C 모델을 재작성하지 않는다.
    data = elf.read_bytes()
    phoff = struct.unpack_from('<I', data, 28)[0]
    phsize, phnum = struct.unpack_from('<HH', data, 42)
    uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
    uc.mem_map(0x10000000, 0x10000)
    uc.mem_map(0x20000000, 0x10000)
    for i in range(phnum):
        kind, offset, va, pa, filesz, memsz, flags, align = struct.unpack_from('<8I', data, phoff + i * phsize)
        if kind == 1 and filesz:
            uc.mem_write(va, data[offset:offset + filesz])
    uc.reg_write(UC_ARM_REG_XPSR, 0x1000000)
    uc.reg_write(UC_ARM_REG_SP, 0x2000f000)
    def invoke(function):
        uc.reg_write(UC_ARM_REG_LR, 0x1000fff1)
        uc.emu_start(symbols[function] | 1, 0x1000fff0, count=100000000)
        if uc.reg_read(UC_ARM_REG_PC) != 0x1000fff0:
            raise RuntimeError(f'{name}: instruction limit reached in {function}')
        return uc.reg_read(UC_ARM_REG_R0)
    return invoke


results = []
for profile in ('graphics', 'integrated'):
    for opt in ('-O0', '-Os'):
        invoke = build_and_load(opt, profile)
        for entry in ('test_integrated_geometry', 'test_viewport_geometry', 'test_main'):
            failure = invoke(entry)
            if failure:
                raise RuntimeError(f'{profile}/{opt}/{entry}: production C assertion failed at test.c:{failure}')
        results.append(dict(profile=profile, optimization=opt,
                            assertions=invoke('get_assertions'), status='pass'))

mutations = []
for opt in ('-O0', '-Os'):
    invoke = build_and_load(opt, 'integrated', mutation=True)
    failure = invoke('test_integrated_geometry')
    error = invoke('get_last_error')
    if not failure or error != 2:
        raise RuntimeError(f'{opt}: historical width140 must fail geometry; line={failure}, error={error}')
    mutations.append(dict(optimization=opt, title_width_percent=140, y_percent=-68,
                          rejected_at_assertion_line=failure, production_geometry_error=error, status='rejected'))
report = dict(results=results,
              historical_regression_mutations=mutations,
              font_line_heights=dict(zip(('montserrat_14','montserrat_40'),heights)),
              sources={str(p.relative_to(PROJECT)): hashlib.sha256(p.read_bytes()).hexdigest()
                       for p in sources + contracts + fonts},
              limits='LVGL 객체/성능입력과 통합 HUD 경계는 대역이다. 실제 렌더/GPU/SPI/HUD 자체 기하/시각 품질은 이 시험이 검증하지 않는다.')
(OUT / 'results.json').write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding='utf-8')
print(json.dumps(report, indent=2, ensure_ascii=False))
