"""Execute production PowerService and STOP port on ARM with emulated MMIO.
WFI/RTC progression is synthetic. This is not board wake/current verification.
"""
import hashlib,json,struct,subprocess,sys
from pathlib import Path
HERE=Path(__file__).resolve().parent;P=HERE.parents[2]
TC=Path('C:/ST/STM32CubeIDE_1.18.1/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344/tools/bin')
sys.path.insert(0,str(P.parents[1]/'.tools/analysis-python'))
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS,UC_HOOK_CODE,UC_HOOK_MEM_READ
from unicorn.arm_const import UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_R0,UC_ARM_REG_PC,UC_ARM_REG_XPSR
out=HERE/'output';out.mkdir(exist_ok=True)
ld=out/'test.ld'
ld.write_text('MEMORY { FLASH(rx): ORIGIN=0x10000000, LENGTH=64K\n RAM(rwx): ORIGIN=0x20000000, LENGTH=128K }\n SECTIONS { .text : { *(.text*) *(.rodata*) } > FLASH\n .data : { *(.data*) } > RAM\n .bss(NOLOAD) : { *(.bss*) *(COMMON) } > RAM }'.replace('ORIGIN=','ORIGIN = ').replace('LENGTH=','LENGTH = '))
includes=[HERE/'stubs',*[P/x for x in ['Core/Inc','Drivers/BSP/inc','Drivers/STM32F4xx_HAL_Driver/Inc','Drivers/CMSIS/Device/ST/STM32F4xx/Include','Drivers/CMSIS/Include','Middlewares/Noodoe/Power/inc','Middlewares/Noodoe/Input/inc']]]
sources=[P/'Drivers/BSP/src/BSP_LowPower.c',P/'Drivers/BSP/src/BSP_Watchdog.c',P/'Drivers/BSP/src/BSP_DisplayPower.c',P/'Middlewares/Noodoe/Power/src/PowerService.c',P/'Middlewares/Noodoe/Input/src/ButtonFeedback.c',HERE/'test_power.c'];reports=[]
regression='--shadow-regression' in sys.argv
if regression:
    original=sources[0].read_text();assert original.count('        (void)RTC->DR;')==1
    mutant=out/'prior-shadow-bug.c';mutant.write_text(original.replace('        (void)RTC->DR;',''))
    sources[0]=mutant
for opt in ('O0','Os'):
    elf=out/(opt+'.elf')
    cmd=[str(TC/'arm-none-eabi-gcc.exe'),'-mcpu=cortex-m4','-mthumb','-std=c11','-'+opt,'-Wall','-Wextra','-Werror','-DSTM32F429xx','-DUSE_HAL_DRIVER','-nostdlib','-ffreestanding','-ffunction-sections','-fdata-sections',*['-I'+str(x) for x in includes],*map(str,sources),'-Wl,-T,'+str(ld),'-Wl,--gc-sections','-Wl,-e,Power_Test','-lgcc','-o',str(elf)]
    r=subprocess.run(cmd,capture_output=True,text=True)
    (out/('compile-'+opt+'.log')).write_text(r.stdout+r.stderr)
    if r.returncode:raise RuntimeError(r.stderr)
    nm=subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),str(elf)],text=True)
    syms={x.split()[2]:int(x.split()[0],16) for x in nm.splitlines() if len(x.split())==3}
    uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
    for a,s in [(0x10000000,0x10000),(0x20000000,0x20000),(0x40000000,0x30000),(0xA0000000,0x2000),(0xE000E000,0x2000)]:uc.mem_map(a,s)
    data=elf.read_bytes();off=struct.unpack_from('<I',data,28)[0];size,count=struct.unpack_from('<HH',data,42)
    for n in range(count):
        kind,pos,va,pa,fs,ms,flags,align=struct.unpack_from('<8I',data,off+n*size)
        if kind==1 and fs:uc.mem_write(va,data[pos:pos+fs])
    def get(a):return struct.unpack('<I',uc.mem_read(a,4))[0]
    def put(a,v):uc.mem_write(a,struct.pack('<I',v))
    # SSR/TR freeze RTC shadow registers until DR is read. The old second-SSR
    # rollover check accidentally left that lock held across WFI.
    shadow={'locked':False,'locked_at_wfi':0}
    def rtc_read(emu,access,address,size,value,context):
        if address in (0x40002828,0x40002800):shadow['locked']=True
        elif address==0x40002804:shadow['locked']=False
    uc.hook_add(UC_HOOK_MEM_READ,rtc_read,begin=0x40002800,end=0x4000282b)
    def instruction(emu,address,size,context):
        if bytes(emu.mem_read(address,2))!=b'\x30\xbf':return
        put(syms['test_wfi_count'],get(syms['test_wfi_count'])+1)
        if get(syms['test_emulate_time']):
            if shadow['locked']:shadow['locked_at_wfi']+=1
            put(0x40002828,get(0x40002828)-32) # 125ms, RTC SSR
            put(0x4000280C,get(0x4000280C)|(1<<10)) # WUTF
        emu.reg_write(UC_ARM_REG_PC,(address+2)|1)
    uc.hook_add(UC_HOOK_CODE,instruction,begin=0x10000000,end=0x1000ffff)
    end=0x1000fff0;uc.reg_write(UC_ARM_REG_SP,0x2001fff0);uc.reg_write(UC_ARM_REG_LR,end|1);uc.reg_write(UC_ARM_REG_XPSR,0x1000000)
    uc.emu_start(syms['Power_Test']|1,end,timeout=30_000_000,count=10000000)
    report={'optimization':opt,'returned':uc.reg_read(UC_ARM_REG_PC)==end,'result':uc.reg_read(UC_ARM_REG_R0),'failure_line':get(syms['test_failure']),'assertions':get(syms['test_assertions']),'rtc_shadow_locked_at_wfi':shadow['locked_at_wfi'],'hardware_access':False}
    reports.append(report);print(json.dumps(report),flush=True)
    (out/('shadow-regression.json' if regression else 'results.json')).write_text(json.dumps({'runs':reports,'expected_prior_bug':regression,'sources':{x.name:hashlib.sha256(x.read_bytes()).hexdigest() for x in sources}},indent=2))
    if not report['returned'] or report['result'] or (bool(shadow['locked_at_wfi'])!=regression):raise SystemExit(1)
