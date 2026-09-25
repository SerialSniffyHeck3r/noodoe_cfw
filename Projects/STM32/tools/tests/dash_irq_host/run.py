"""Production Dash C with real HAL headers and forced IRQ interleavings."""
import hashlib,json,struct,subprocess,sys
from pathlib import Path
HERE=Path(__file__).resolve().parent;PROJECT=HERE.parents[2]
CC=Path('C:/ST/STM32CubeIDE_1.18.1/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344/tools/bin')
sys.path.insert(0,str(PROJECT.parents[1]/'.tools/analysis-python'))
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS,UC_HOOK_MEM_READ
from unicorn.arm_const import UC_ARM_REG_LR,UC_ARM_REG_PC,UC_ARM_REG_R0,UC_ARM_REG_SP,UC_ARM_REG_XPSR
out=HERE/'output';out.mkdir(exist_ok=True);linker=out/'test.ld'
linker.write_text('MEMORY { FLASH(rx): ORIGIN = 0x10000000, LENGTH = 64K\nRAM(rwx): ORIGIN = 0x20000000, LENGTH = 128K }\nSECTIONS { .text : { *(.text*) *(.rodata*) } > FLASH\n.data : { *(.data*) } > RAM AT>FLASH\n.bss(NOLOAD) : { *(.bss*) *(COMMON) } > RAM\n/DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) } }\n')
incs=['Core/Inc','Drivers/BSP/inc','Drivers/BSP/src','Drivers/STM32F4xx_HAL_Driver/Inc','Drivers/CMSIS/Device/ST/STM32F4xx/Include','Drivers/CMSIS/Include'];reports=[]
for optimization in ('O0','Os'):
    elf=out/f'dash-{optimization}.elf'
    cmd=[str(CC/'arm-none-eabi-gcc.exe'),'-mcpu=cortex-m4','-mthumb','-std=c11',f'-{optimization}','-Wall','-Wextra','-Werror','-DSTM32F429xx','-DUSE_HAL_DRIVER','-ffreestanding','-fno-builtin','-ffunction-sections','-fdata-sections','-nostdlib',*['-I'+str(PROJECT/p) for p in incs],str(HERE/'test_dash.c'),'-Wl,-T,'+str(linker),'-Wl,--gc-sections','-Wl,-e,DashIRQ_TestMain','-o',str(elf),'-lgcc']
    compiled=subprocess.run(cmd,capture_output=True,text=True);(out/f'compile-{optimization}.log').write_text(compiled.stdout+compiled.stderr)
    if compiled.returncode:raise RuntimeError(compiled.stderr)
    listing=subprocess.check_output([str(CC/'arm-none-eabi-nm.exe'),str(elf)],text=True);symbols={p[2]:int(p[0],16) for line in listing.splitlines() if len(p:=line.split())==3}
    emu=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
    for address,size in ((0x10000000,0x10000),(0x20000000,0x20000),(0x40005000,0x1000)):
        emu.mem_map(address,size)
    # STM32F4: reading SR, then DR clears PE/FE/NE/ORE/IDLE and RXNE.
    sr_read=[False]
    def uart_read(uc,access,address,size,value,user):
        if address==0x40005000:sr_read[0]=True
        elif address==0x40005004 and sr_read[0]:
            sr=struct.unpack('<I',uc.mem_read(0x40005000,4))[0]
            uc.mem_write(0x40005000,struct.pack('<I',sr&~0x3f));sr_read[0]=False
    emu.hook_add(UC_HOOK_MEM_READ,uart_read,begin=0x40005000,end=0x40005007)
    data=elf.read_bytes();header=struct.unpack_from('<16sHHIIIIIHHHHHH',data)
    for i in range(header[10]):
        kind,offset,address,_,size,_,_,_=struct.unpack_from('<IIIIIIII',data,header[5]+i*header[9])
        if kind==1 and size:emu.mem_write(address,data[offset:offset+size])
    stop=0x1000FFF0;emu.mem_write(stop,b'\x00\xbe');emu.reg_write(UC_ARM_REG_SP,0x2001FFF0);emu.reg_write(UC_ARM_REG_LR,stop|1);emu.reg_write(UC_ARM_REG_XPSR,0x01000000)
    emu.emu_start(symbols['DashIRQ_TestMain']|1,stop,timeout=10_000_000,count=10_000_000)
    word=lambda name:struct.unpack('<I',emu.mem_read(symbols[name],4))[0]
    report={'optimization':optimization,'returned':emu.reg_read(UC_ARM_REG_PC)==stop,'result':emu.reg_read(UC_ARM_REG_R0),'assertions':word('g_assertions'),'failure_line':word('g_failure_line'),'mock_error':word('g_mock_error'),'hardware_access':False,'source_sha256':hashlib.sha256((PROJECT/'Drivers/BSP/src/BSP_Dash.c').read_bytes()).hexdigest()}
    reports.append(report);print(json.dumps(report),flush=True);(out/'results.json').write_text(json.dumps(reports,indent=2)+'\n')
    if not report['returned'] or report['result'] or report['failure_line'] or report['mock_error']:raise SystemExit(1)
