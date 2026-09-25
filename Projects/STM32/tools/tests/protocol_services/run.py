"""서비스 원본 C를 Cortex-M4 ELF로 컴파일하여 Unicorn에서 실행한다.

Python은 ELF 적재/진단만 담당한다. protocol 상태/단위 변환의 Python 복제품은 없다.
장치·Cube 생성·프로젝트 빌드는 수행하지 않으며 output 아래에만 결과를 쓴다.
"""
import hashlib
import json
import struct
import subprocess
import sys
import zlib
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
 FLASH(rx): ORIGIN = 0x10000000, LENGTH = 128K
 RAM(rwx): ORIGIN = 0x20000000, LENGTH = 1024K
}
SECTIONS {
 .text : { *(.text*) *(.rodata*) } > FLASH
 .data : { *(.data*) } > RAM
 .bss (NOLOAD) : { *(.bss*) *(COMMON) } > RAM
 /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) }
}
''')
names = ('Vehicle', 'GNSS', 'OBD')
sources = [PROJECT / f'Middlewares/Noodoe/{n}/src/{n}_Service.c' for n in names]
sources += [PROJECT / f'Middlewares/Noodoe/Update/src/{n}.c' for n in ('NDCP', 'Update_Service', 'Update_SHA256', 'Recovery_Core')]
headers = [PROJECT / f'Middlewares/Noodoe/{n}/inc/{n}_Service.h' for n in names]
headers += [PROJECT / f'Middlewares/Noodoe/Update/inc/{n}.h' for n in ('NDCP', 'Update_Service')]
tests = [HERE / f'test_{n}.c' for n in ('runtime', 'vehicle', 'gnss', 'obd', 'ndcp', 'update')]
includes = [v for n in names for v in ('-I', str(PROJECT / f'Middlewares/Noodoe/{n}/inc'))]
includes += ['-I', str(PROJECT / 'Middlewares/Noodoe/Update/inc'), '-I', str(OUT)]
# C fixture 생성 규칙과 독립적인 hashlib/zlib로 전체448KiB expected digest를 고정한다.
fixture = bytearray((i * 13 + 7) & 255 for i in range(0x70000))
fixture[:8] = struct.pack('<II', 0x20020000, 0x08010101)
digest = hashlib.sha256(fixture).digest()
(OUT / 'update_fixture.h').write_text('#define FIXTURE_CRC 0x%08XU\nstatic const unsigned char fixture_sha[32]={%s};\n' %
                                    (zlib.crc32(fixture), ','.join('0x%02X' % n for n in digest)))
results = []
for opt in ('-O0', '-Os'):
    elf = OUT / (opt[1:] + '.elf')
    cmd = [str(TC / 'arm-none-eabi-gcc.exe'),str(next(q for q in Path(__file__).resolve().parents if (q/'tools/build.ps1').is_file())/'Drivers/BSP/src/noodoe_crc32.c'),'-I'+str(next(q for q in Path(__file__).resolve().parents if (q/'tools/build.ps1').is_file())/'Drivers/BSP/inc'), '-mcpu=cortex-m4', '-mthumb',
           '-mfloat-abi=soft', '-std=c11', opt, '-ffreestanding', '-fno-builtin',
           '-fdata-sections', '-ffunction-sections', '-fno-unwind-tables', '-nostdlib',
           '-Wall', '-Wextra', '-Werror', '-I', str(HERE), *includes,
           *map(str, sources + tests), '-T', str(OUT / 'test.ld'),
           '-Wl,--gc-sections', '-Wl,-e,test_main', '-lgcc', '-o', str(elf)]
    build = subprocess.run(cmd, capture_output=True, text=True)
    (OUT / f'{opt[1:]}-build.log').write_text(build.stdout + build.stderr, encoding='utf-8')
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
    uc.mem_map(0x20000000, 0x100000)
    for i in range(phnum):
        kind, offset, va, pa, filesz, memsz, flags, align = struct.unpack_from('<8I', data, phoff + i * phsize)
        if kind == 1 and filesz:
            uc.mem_write(va, data[offset:offset + filesz])
    uc.reg_write(UC_ARM_REG_XPSR, 0x1000000)
    uc.reg_write(UC_ARM_REG_SP, 0x200ffff0)
    stop = 0x1001fff0
    uc.reg_write(UC_ARM_REG_LR, stop | 1)
    uc.reg_write(UC_ARM_REG_PC, symbols['test_main'] | 1)
    for _ in range(12):
        uc.emu_start(uc.reg_read(UC_ARM_REG_PC) | 1, stop, timeout=10000000, count=800000000)
        if uc.reg_read(UC_ARM_REG_PC) == stop:
            break
    diag = {n: struct.unpack('<I', uc.mem_read(symbols['g_test_' + n], 4))[0]
            for n in ('suite', 'failure_line', 'assertions')}
    if uc.reg_read(UC_ARM_REG_PC) != stop:
        raise RuntimeError(f'{opt}: execution limit reached: {diag}')
    if uc.reg_read(UC_ARM_REG_R0):
        raise RuntimeError(f'{opt}: actual C assertion failed: {diag}')
    results.append(dict(optimization=opt, assertions=diag['assertions'], status='pass'))
report = dict(results=results,
              fixture=dict(bytes=len(fixture), sha256=digest.hex(), crc32_iso=zlib.crc32(fixture)),
              sources={str(p.relative_to(PROJECT)): hashlib.sha256(p.read_bytes()).hexdigest()
                       for p in sources + headers + tests + [HERE / 'test_common.h', Path(__file__)]},
              limits='실제 ARM C의 protocol/OTA 상태 시험이다. NOR/metadata/reset은 경계 모형이며 UART/SPP/GPS/ELM/ECU/FLASH 실장 연결과 실제 task 동시성은 검증하지 않는다. 모형 NOR 배열 때문에 시험용 RAM map은1MiB다.')
(OUT / 'results.json').write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding='utf-8')
print(json.dumps(report, indent=2, ensure_ascii=False))
