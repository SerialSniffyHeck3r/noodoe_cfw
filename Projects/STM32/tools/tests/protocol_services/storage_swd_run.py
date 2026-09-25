"""실제 StorageSWD C를 ARM으로 실행한다. NOR/SDRAM은 메모리 경계 모형이다."""
import hashlib
import json
import struct
import subprocess
import sys
import zlib
from pathlib import Path

HERE=Path(__file__).resolve().parent
PROJECT=HERE.parents[2]
sys.path.insert(0,str(PROJECT.parents[1]/'.tools/analysis-python'))
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_R0,UC_ARM_REG_XPSR,UC_ARM_REG_PC

TC=Path('C:/ST/STM32CubeIDE_1.18.1/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344/tools/bin')
OUT=HERE/'storage_swd_output'
OUT.mkdir(parents=True,exist_ok=True)
SOURCE=PROJECT/'Middlewares/Noodoe/StorageSWD/src/StorageSWD.c'
HEADER=PROJECT/'Middlewares/Noodoe/StorageSWD/inc/StorageSWD.h'
STOP=0x1001FFF0


def fixture(offset,length):
    """모의 NOR의 byte 함수와 독립 zlib로 기대 CRC를 만든다."""
    return bytes((((i*17)+(i>>8))^0xA5)&255 for i in range(offset,offset+length))


vectors={
    '123_4097':fixture(123,4097),
    'LAST_1':fixture(0x7FFFFFF,1),
    '0_8192':fixture(0,8192),
    'FULL':fixture(0x7800000,0x800000),
}
(OUT/'storage_swd_fixture.h').write_text(''.join(
    f'#define SWD_CRC_{name} 0x{zlib.crc32(data):08X}UL\n' for name,data in vectors.items()))
(OUT/'storage_swd_test.ld').write_text('''MEMORY {
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
common=['-I',str(PROJECT/'Drivers/BSP/inc'),'-std=c11','-mcpu=cortex-m4','-mthumb','-mfloat-abi=soft','-Wall','-Wextra','-Werror',
        '-ffreestanding','-fno-builtin','-I',str(PROJECT/'Middlewares/Noodoe/StorageSWD/inc')]
production=subprocess.run([str(TC/'arm-none-eabi-gcc.exe'),*common,'-Os','-DSTM32F429xx',
    '-I',str(PROJECT/'Drivers/BSP/inc'),'-I',str(PROJECT/'Drivers/CMSIS/Include'),
    '-I',str(PROJECT/'Drivers/CMSIS/Device/ST/STM32F4xx/Include'),
    '-I',str(PROJECT/'Drivers/STM32F4xx_HAL_Driver/Inc'),'-I',str(PROJECT/'Core/Inc'),
    '-c',str(SOURCE),'-o',str(OUT/'storage_swd_production.o')],capture_output=True,text=True)
if production.returncode:raise RuntimeError(production.stdout+production.stderr)


def call(uc,symbols,name):
    """실제 ELF Thumb 진입점을 실행하고 정상 반환 및 최대 실행량을 검사한다."""
    uc.reg_write(UC_ARM_REG_XPSR,0x1000000)
    uc.reg_write(UC_ARM_REG_SP,0x2001FFF0)
    uc.reg_write(UC_ARM_REG_LR,STOP|1)
    uc.emu_start(symbols[name]|1,STOP,count=2_000_000_000,timeout=50_000_000)
    if uc.reg_read(UC_ARM_REG_PC)!=STOP:raise RuntimeError(f'{name}: 실행 제한')
    result=uc.reg_read(UC_ARM_REG_R0)
    if result:raise RuntimeError(f'{name}: storage_swd_test.c CHECK line {result}')


results=[]
for opt in ('-O0','-Os'):
    elf=OUT/f'storage_swd_{opt[1:]}.elf'
    build=subprocess.run([str(TC/'arm-none-eabi-gcc.exe'),*common,opt,'-DSTORAGE_SWD_TEST_PORT',
        '-I',str(HERE),'-I',str(OUT),'-nostdlib',str(PROJECT/'Drivers/BSP/src/noodoe_crc32.c'),'-fno-unwind-tables',
        str(SOURCE),str(HERE/'storage_swd_test.c'),'-T',str(OUT/'storage_swd_test.ld'),
        '-Wl,-e,storage_swd_test_main','-lgcc','-o',str(elf)],capture_output=True,text=True)
    if build.returncode:raise RuntimeError(build.stdout+build.stderr)
    nm=subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),str(elf)],text=True)
    symbols={line.split()[2]:int(line.split()[0],16) for line in nm.splitlines() if len(line.split())==3}
    data=elf.read_bytes();phoff=struct.unpack_from('<I',data,28)[0]
    phsize,phnum=struct.unpack_from('<HH',data,42)
    uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
    uc.mem_map(0x10000000,0x20000);uc.mem_map(0x20000000,0x20000)
    uc.mem_map(0xC0000000,0x820000)
    for index in range(phnum):
        kind,offset,va,_,filesz,_,_,_=struct.unpack_from('<8I',data,phoff+index*phsize)
        if kind==1 and filesz:uc.mem_write(va,data[offset:offset+filesz])
    call(uc,symbols,'storage_swd_test_main')
    row=dict(optimization=opt,status='pass')
    if opt=='-Os':
        call(uc,symbols,'storage_swd_test_full')
        actual=bytes(uc.mem_read(0xC0010000,0x800000))
        if actual!=vectors['FULL']:raise RuntimeError('전체8MiB SDRAM byte mismatch')
        row['full_buffer']=dict(bytes=len(actual),crc32_iso=f'0x{zlib.crc32(actual):08X}',
                                sha256=hashlib.sha256(actual).hexdigest(),process_polls=2048)
    row['assertions']=struct.unpack('<I',uc.mem_read(symbols['storage_swd_test_assertions'],4))[0]
    results.append(row);print(json.dumps(row),flush=True)
report=dict(results=results,production_cmsis_compile='pass',
    sources={str(path.relative_to(PROJECT)):hashlib.sha256(path.read_bytes()).hexdigest()
             for path in (SOURCE,HEADER,HERE/'storage_swd_test.c',HERE/'storage_swd_test_port.h',Path(__file__))},
    limits='실제 ARM C를 실행한다. NOR/RAM/UID/메모리 barrier 경계는 모형이며 실제 SPI5/SDRAM/SWD/UID/RTOS 선점/백업 속도/장치 동작은 미검증이다.')
(OUT/'storage_swd_results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(report,ensure_ascii=False,indent=2))
