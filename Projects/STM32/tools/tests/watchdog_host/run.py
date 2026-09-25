"""Execute actual watchdog/health ARM code against MMIO, not hardware.
Tests missed-owner progress, failure latch, absolute leases, wrap, STOP grants,
ISR refusal and pre-C flash-LMA startup. The flash busy timer is synthetic.
"""
import hashlib,json,struct,subprocess,sys,re
from pathlib import Path
HERE=Path(__file__).resolve().parent;P=HERE.parents[2]
TC=Path('C:/ST/STM32CubeIDE_1.18.1/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344/tools/bin')
sys.path.insert(0,str(P.parents[1]/'.tools/analysis-python'))
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS,UC_HOOK_MEM_WRITE
from unicorn.arm_const import UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_R0,UC_ARM_REG_PC,UC_ARM_REG_XPSR
out=HERE/'output';out.mkdir(exist_ok=True)
ld=out/'test.ld';ld.write_text('''MEMORY { FLASH(rx): ORIGIN = 0x08000000, LENGTH = 256K
 RAM(rwx): ORIGIN = 0x20000000, LENGTH = 128K }
SECTIONS { .text : { *(.text*) *(.rodata*) } > FLASH
 .data : { _sdata = .; *(.data*) *(.RamFunc*) } > RAM AT> FLASH
 _sidata = LOADADDR(.data);
 .bss(NOLOAD) : { *(.bss*) *(COMMON) } > RAM
 /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) } }''')
incs=[P/x for x in ['Drivers/BSP/inc','Middlewares/Noodoe/Health/inc','Drivers/CMSIS/Device/ST/STM32F4xx/Include','Drivers/CMSIS/Include']]
sources=[P/'Drivers/BSP/src/BSP_Watchdog.c',P/'Middlewares/Noodoe/Health/src/Health_Service.c',HERE/'test_watchdog.c'];reports=[]
for opt in ('O0','Os'):
    elf=out/(opt+'.elf')
    cmd=[str(TC/'arm-none-eabi-gcc.exe'),'-mcpu=cortex-m4','-mthumb','-std=c11','-'+opt,'-Wall','-Wextra','-Werror','-DSTM32F429xx','-nostdlib','-ffreestanding','-fno-builtin','-ffunction-sections','-fdata-sections',*['-I'+str(x) for x in incs],*map(str,sources),'-Wl,-T,'+str(ld),'-Wl,-e,Watchdog_Test','-lgcc','-o',str(elf)]
    r=subprocess.run(cmd,capture_output=True,text=True);(out/('compile-'+opt+'.log')).write_text(r.stdout+r.stderr)
    if r.returncode:raise RuntimeError(r.stderr)
    nm=subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),str(elf)],text=True)
    syms={x.split()[2]:int(x.split()[0],16) for x in nm.splitlines() if len(x.split())==3}
    uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
    for a,s in [(0x08000000,0x40000),(0x20000000,0x20000),(0x40000000,0x30000),(0xE0000000,0x100000)]:uc.mem_map(a,s)
    data=elf.read_bytes();off=struct.unpack_from('<I',data,28)[0];size,count=struct.unpack_from('<HH',data,42)
    segments=[]
    for n in range(count):
        kind,pos,va,pa,fs,ms,flags,align=struct.unpack_from('<8I',data,off+n*size)
        if kind==1 and fs:
            uc.mem_write(pa,data[pos:pos+fs]);segments.append((va,data[pos:pos+fs]))
    def get(a):return struct.unpack('<I',uc.mem_read(a,4))[0]
    def put(a,v):uc.mem_write(a,struct.pack('<I',v))
    def call(name,ipsr=0):
        end=0x0803fff0;uc.reg_write(UC_ARM_REG_SP,0x2001fff0);uc.reg_write(UC_ARM_REG_LR,end|1);uc.reg_write(UC_ARM_REG_XPSR,0x1000000|ipsr)
        uc.emu_start(syms[name]|1,end,timeout=30_000_000,count=30000000)
        if uc.reg_read(UC_ARM_REG_PC)!=end:raise RuntimeError('did not return '+name)
        return uc.reg_read(UC_ARM_REG_R0)
    # No initialized RAM: calling the RAM-VMA HardwareKey here would execute
    # garbage and fail. The production StartEarly must use its flash LMA.
    uc.mem_write(0x20000000,b'\xA5'*0x20000);put(0x40023874,2);put(0x4000300C,0)
    # An LSI-ready mock alone previously hid the real F429 failure: PR/RLR
    # updates cannot complete before the watchdog kernel has been started.
    startup_keys=[]
    def check_start_order(u,access,address,size,value,context):
        if address==0x40003000:startup_keys.append(value)
        if address in (0x40003004,0x40003008):
            assert startup_keys[:2]==[0xCCCC,0x5555], 'IWDG must start before PR/RLR update'
    startup_hook=uc.hook_add(UC_HOOK_MEM_WRITE,check_start_order,begin=0x40003000,end=0x4000300b)
    early=call('BSP_Watchdog_StartEarly');assert early==1
    assert startup_keys==[0xCCCC,0x5555,0xAAAA]
    uc.hook_del(startup_hook)
    uc.mem_write(0x20000000,b'\0'*0x20000)
    for va,content in segments:uc.mem_write(va,content)
    result=call('Watchdog_Test');irq=call('Watchdog_IRQ_Test',16)
    start_failure=call('Watchdog_StartFailure_Test')
    report={'optimization':opt,'result':result,'failure_line':get(syms['test_failure']),'assertions':get(syms['test_assertions']),'pre_c_without_initialized_ram':early==1,'irq_no_feed':irq==0,'lsi_start_failure_latched':start_failure==0,'hardware_access':False}
    reports.append(report);print(json.dumps(report),flush=True)
    if result or irq or start_failure:raise SystemExit(1)
# Project-owned feed audit. Vendor/generated init may configure IWDG, but no
# project IRQ/task/driver may retain an independent raw refresh instruction.
raw=[]
for directory in ['Drivers/BSP/src','App_Logic','Middlewares/Noodoe','Graphics']:
    for path in (P/directory).rglob('*.c'):
        if re.search(r'IWDG\s*->\s*KR\s*=',path.read_text(encoding='utf-8')):raw.append(str(path.relative_to(P)))
assert raw==['Drivers\\BSP\\src\\BSP_Watchdog.c'] or raw==['Drivers/BSP/src/BSP_Watchdog.c'],raw
(out/'results.json').write_text(json.dumps({'runs':reports,'sole_key_writer':raw,'sources':{str(x.relative_to(P)):hashlib.sha256(x.read_bytes()).hexdigest() for x in sources}},indent=2))
