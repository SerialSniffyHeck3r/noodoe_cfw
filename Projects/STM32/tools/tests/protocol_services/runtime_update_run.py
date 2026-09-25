"""실제 RuntimeUpdate+UpdateService/NDCP/SHA를 ARM에서 실행하는 경계 검사."""
import os
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
bootstrap=os.environ.get('RUNTIME_UPDATE_BOOTSTRAP_TEST')=='1'
OUT=HERE/('bootstrap_update_output' if bootstrap else 'runtime_update_output')
OUT.mkdir(parents=True,exist_ok=True)
SOURCE=PROJECT/'App_Logic/Runtime/src/RuntimeUpdate.c'
SOURCES=[SOURCE]+[PROJECT/f'Middlewares/Noodoe/Update/src/{name}.c' for name in ('Update_Service','NDCP','Update_SHA256','Recovery_Core')]
SOURCES += [PROJECT/'App_Logic/Bootstrap/src/bootstrap_recovery.c']
HEADERS=[PROJECT/'App_Logic/Runtime/inc/RuntimeUpdate.h']+[PROJECT/f'Middlewares/Noodoe/Update/inc/{name}.h' for name in ('Update_Service','NDCP','Update_Metadata')]
HEADERS += [PROJECT/'Middlewares/Noodoe/Bluetooth/inc/NoodoeBluetooth.h']
STOP=0x1001FFF0
fixture=bytearray((i*13+7)&255 for i in range(0x70000))
fixture[:8]=struct.pack('<II',0x20020000,0x08010101)
digest=hashlib.sha256(fixture).digest()
raw_fixture=bytearray(len(fixture))
raw_fixture[0::2],raw_fixture[1::2]=fixture[1::2],fixture[0::2]
(OUT/'runtime_update_fixture.h').write_text('#define RUNTIME_FIXTURE_CRC 0x%08XUL\nstatic const unsigned char runtime_fixture_sha[32]={%s};\n' %
    (zlib.crc32(fixture),','.join('0x%02X'%b for b in digest)))
(OUT/'runtime_update_test.ld').write_text('''MEMORY {
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
includes=['App_Logic/Runtime/inc','App_Logic/Bootstrap/inc','Middlewares/Noodoe/Update/inc','Middlewares/Noodoe/Bluetooth/inc','Drivers/BSP/inc',
          'Drivers/CMSIS/Include','Drivers/CMSIS/Device/ST/STM32F4xx/Include',
          'Drivers/STM32F4xx_HAL_Driver/Inc','Core/Inc']
common=['-std=c11','-mcpu=cortex-m4','-mthumb','-mfloat-abi=soft','-Wall','-Wextra','-Werror',
        '-ffreestanding','-fno-builtin','-DSTM32F429xx']
if bootstrap:common += ['-DNOODOE_BOOTSTRAP=1']
common += [item for path in includes for item in ('-I',str(PROJECT/path))]
production=subprocess.run([str(TC/'arm-none-eabi-gcc.exe'),'-I'+str(next(q for q in Path(__file__).resolve().parents if (q/'tools/build.ps1').is_file())/'Drivers/BSP/inc'),*common,'-Os','-c',str(SOURCE),
                          '-o',str(OUT/'runtime_update_production.o')],capture_output=True,text=True)
if production.returncode:raise RuntimeError(production.stdout+production.stderr)
print('production CMSIS compile: pass',flush=True)

results=[]
for opt in ('-O0','-Os'):
    elf=OUT/f'runtime_update_{opt[1:]}.elf'
    build=subprocess.run([str(TC/'arm-none-eabi-gcc.exe'),str(next(q for q in Path(__file__).resolve().parents if (q/'tools/build.ps1').is_file())/'Drivers/BSP/src/noodoe_crc32.c'),'-I'+str(next(q for q in Path(__file__).resolve().parents if (q/'tools/build.ps1').is_file())/'Drivers/BSP/inc'),*common,opt,'-DRUNTIME_UPDATE_TEST_PORT',
        '-I',str(HERE),'-I',str(OUT),'-nostdlib','-fno-unwind-tables',
        *map(str,SOURCES),str(HERE/'runtime_update_test.c'),'-T',str(OUT/'runtime_update_test.ld'),
        '-Wl,-e,runtime_update_test_main','-lgcc','-o',str(elf)],capture_output=True,text=True)
    if build.returncode:raise RuntimeError(build.stdout+build.stderr)
    nm=subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),str(elf)],text=True)
    symbols={line.split()[2]:int(line.split()[0],16) for line in nm.splitlines() if len(line.split())==3}
    data=elf.read_bytes();phoff=struct.unpack_from('<I',data,28)[0]
    phsize,phnum=struct.unpack_from('<HH',data,42)
    uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
    uc.mem_map(0x10000000,0x20000);uc.mem_map(0x20000000,0x20000)
    uc.mem_map(0x11000000,0x80000);uc.mem_map(0xC0000000,0x40000)
    uc.mem_map(0x08000000,0x10000)
    resident=(PROJECT.parents[1]/'VERY_IMPORTANT_ORIGINAL_NOODOE_BOOTLOARDER/noodoe_full_flash_dump_A.bin').read_bytes()[:0x10000]
    uc.mem_write(0x08000000,resident)
    for index in range(phnum):
        kind,offset,va,_,filesz,_,_,_=struct.unpack_from('<8I',data,phoff+index*phsize)
        if kind==1 and filesz:uc.mem_write(va,data[offset:offset+filesz])
    uc.reg_write(UC_ARM_REG_XPSR,0x1000000);uc.reg_write(UC_ARM_REG_SP,0x2001FFF0)
    uc.reg_write(UC_ARM_REG_LR,STOP|1)
    # Extra complete448KiB readbacks exercise offline approval and corrupt NOR.
    # This host execution budget is not a firmware deadline/watchdog setting.
    uc.emu_start(symbols['runtime_update_test_main']|1,STOP,count=2_000_000_000,timeout=150_000_000)
    if uc.reg_read(UC_ARM_REG_PC)!=STOP:raise RuntimeError(f'{opt}: 실행 제한')
    failure=uc.reg_read(UC_ARM_REG_R0)
    if failure:
        diagnostics={n:struct.unpack('<I',uc.mem_read(symbols[n],4))[0] for n in ('test_command_opcode','test_command_expected','test_command_actual')}
        raise RuntimeError(f'{opt}: runtime_update_test.c CHECK line {failure}: {diagnostics}')
    # BSP 대역은 물리 byte 배열 그대로다. 실제 ARM adapter가 남긴 전체 staging을
    # Python의 독립 pair 주소 교환으로 대조하며 두 경로의 숨은 정규화를 허용하지 않는다.
    raw=bytes(uc.mem_read(0x11000000,len(fixture)))
    if raw!=raw_fixture:raise RuntimeError(f'{opt}: physical NOR != pair_swap(canonical APP)')
    stock_decode=bytearray(len(raw))
    stock_decode[0::2],stock_decode[1::2]=raw[1::2],raw[0::2]
    if stock_decode!=fixture:raise RuntimeError(f'{opt}: stock decode != full canonical APP')
    assertions=struct.unpack('<I',uc.mem_read(symbols['runtime_update_assertions'],4))[0]
    row=dict(optimization=opt,status='pass',assertions=assertions,
             full_raw_pair_swap_match=True,full_stock_decode_match=True,
             raw_sha256=hashlib.sha256(raw).hexdigest())
    results.append(row);print(json.dumps(row),flush=True)
report=dict(results=results,production_cmsis_compile='pass',
    image_fixture=dict(bytes=len(fixture),sha256=digest.hex(),crc32_iso=f'0x{zlib.crc32(fixture):08X}'),
    physical_fixture=dict(bytes=len(raw_fixture),sha256=hashlib.sha256(raw_fixture).hexdigest(),
                          crc32_iso=f'0x{zlib.crc32(raw_fixture):08X}',
                          layout='physical byte p equals canonical APP byte p xor 1; BSP stub performs no codec'),
    sources={str(path.relative_to(PROJECT)):hashlib.sha256(path.read_bytes()).hexdigest()
             for path in SOURCES+HEADERS+[HERE/'runtime_update_test.c',HERE/'runtime_update_test_port.h',Path(__file__)]},
    limits='실제 ARM adapter/UpdateService/NDCP/SHA 코드의 경계 시험이다. NOR/RAM/metadata/BT pause/reset/context는 대역이며 실제 장치와 RTOS 선점,UART pause,FLASH,전원차단 및 설치 결과를 검증하지 않는다.')
(OUT/'runtime_update_results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(report,ensure_ascii=False,indent=2))
