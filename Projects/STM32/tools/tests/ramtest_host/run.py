"""Compile and exercise actual sliced RAMTest C in ARM Unicorn; no hardware.
RAM bus/DMA are modeled so alias, corruption, delayed cancellation and timeout
failures can be reproduced. This does not prove physical SDRAM timing/retention.
"""
import hashlib,json,struct,subprocess,sys
from pathlib import Path
HERE=Path(__file__).resolve().parent
PROJECT=HERE.parents[2]
sys.path.insert(0,str(PROJECT.parents[1]/'.tools/analysis-python'))
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_R0,UC_ARM_REG_XPSR,UC_ARM_REG_PC
TC=Path('C:/ST/STM32CubeIDE_1.18.1/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344/tools/bin')
OUT=HERE/'output';OUT.mkdir(parents=True,exist_ok=True)
(OUT/'test.ld').write_text('MEMORY { FLASH(rx): ORIGIN = 0x10000000, LENGTH = 128K\nRAM(rwx): ORIGIN = 0x20000000, LENGTH = 128K }\nSECTIONS { .text : { *(.text*) *(.rodata*) } > FLASH\n.data : { *(.data*) } > RAM\n.bss (NOLOAD) : { *(.bss*) *(COMMON) } > RAM\n}\n')
tests=['test_prepare','test_success','test_walk_failure','test_alias_failure','test_mailbox_validation',
       'test_dma_corruption','test_dma_timeout','test_dma_cancel','test_stuck_dma_quarantined']
results=[]
for opt in ['-O0','-Os']:
    elf=OUT/(opt[1:]+'.elf')
    command=[str(TC/'arm-none-eabi-gcc.exe'),'-mcpu=cortex-m4','-mthumb','-std=gnu11',opt,
             '-ffreestanding','-fno-builtin','-nostdlib','-Wall','-Wextra','-Werror','-DBSP_RAMTEST_HOST',
             '-I',str(PROJECT/'Drivers/BSP/inc'),'-I',str(PROJECT/'Drivers/BSP/src'),str(HERE/'test.c'),
             '-T',str(OUT/'test.ld'),'-Wl,-e,test_prepare','-lgcc','-o',str(elf)]
    compiled=subprocess.run(command,capture_output=True,text=True)
    if compiled.returncode:raise RuntimeError(compiled.stdout+compiled.stderr)
    nm=subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),str(elf)],text=True)
    symbols={p[2]:int(p[0],16) for line in nm.splitlines() if len(p:=line.split())==3}
    data=elf.read_bytes();phoff=struct.unpack_from('<I',data,28)[0];phsize,phnum=struct.unpack_from('<HH',data,42)
    for name in tests:
        uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
        uc.mem_map(0x10000000,0x20000);uc.mem_map(0x20000000,0x20000);uc.mem_map(0xC0000000,0x40000)
        for i in range(phnum):
            kind,offset,va,pa,filesz,memsz,flags,align=struct.unpack_from('<8I',data,phoff+i*phsize)
            if kind==1 and filesz:uc.mem_write(va,data[offset:offset+filesz])
        uc.reg_write(UC_ARM_REG_XPSR,0x1000000);uc.reg_write(UC_ARM_REG_SP,0x2001f000);uc.reg_write(UC_ARM_REG_LR,0x1001fff1)
        uc.emu_start(symbols[name]|1,0x1001fff0,count=70000000)
        if uc.reg_read(UC_ARM_REG_PC)!=0x1001fff0:raise RuntimeError(f'{opt} {name}: instruction bound exhausted')
        if uc.reg_read(UC_ARM_REG_R0):raise RuntimeError(f'{opt} {name}: harness line {uc.reg_read(UC_ARM_REG_R0)} failed')
        uc.reg_write(UC_ARM_REG_LR,0x1001fff1);uc.emu_start(symbols['get_assertions']|1,0x1001fff0,count=1000)
        results.append({'optimization':opt,'test':name,'assertions':uc.reg_read(UC_ARM_REG_R0),'status':'pass'})
sources=[PROJECT/'Drivers/BSP/src/BSP_RAMTest.c',PROJECT/'Drivers/BSP/src/BSP_RAMTest_Port.c',PROJECT/'Drivers/BSP/inc/BSP_RAMTest.h']
report={'hardware_access':False,'results':results,'sources':{str(p.relative_to(PROJECT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}}
(OUT/'results.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
