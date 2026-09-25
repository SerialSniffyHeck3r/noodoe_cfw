"""Compile the actual pure-C UI state machines for Cortex-M4 and execute them.
No Python reimplementation of state logic; no board or peripheral access.
"""
import hashlib,json,struct,subprocess,sys,re
from pathlib import Path
HERE=Path(__file__).resolve().parent
P=HERE.parents[2]
sys.path.insert(0,str(P.parents[1]/'.tools/analysis-python'))
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_R0,UC_ARM_REG_XPSR,UC_ARM_REG_PC,UC_ARM_REG_C1_C0_2,UC_ARM_REG_FPEXC
TC=Path('C:/ST/STM32CubeIDE_1.18.1/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344/tools/bin')
OUT=HERE/'output';OUT.mkdir(parents=True,exist_ok=True)
(OUT/'test.ld').write_text('MEMORY { FLASH(rx): ORIGIN = 0x10000000, LENGTH = 512K\n RAM(rwx): ORIGIN = 0x20000000, LENGTH = 256K }\n SECTIONS { .text : { *(.text*) *(.rodata*) } > FLASH\n .data : { *(.data*) } > RAM\n .bss (NOLOAD) : { *(.bss*) *(COMMON) } > RAM end = .; _end = .; }')
sources=sorted((P/'App_Logic/UI/src').glob('ui_*.c'))+[P/'App_Logic/UI/src'/x for x in ('trip_computer.c','phone_content.c','phone_trail.c','dashboard_pages.c','home_page.c','page_preview.c','development_data.c','phone_calls.c')]+[P/'Graphics/UI/src/page_transition.c',P/'Drivers/BSP/src/BSP_Calendar.c']
report=[]
for data_debug,opt in ((0,'-O0'),(0,'-Os'),(0,'-Oz'),(1,'-O0'),(1,'-Os'),(1,'-Oz')):
    elf=OUT/(opt[1:]+'-data'+str(data_debug)+'.elf')
    cmd=[str(TC/'arm-none-eabi-gcc.exe'),'-mcpu=cortex-m4','-mfpu=fpv4-sp-d16','-mfloat-abi=hard','-mthumb','-std=c11',opt,'-ffreestanding','-fno-builtin','-nostartfiles','--specs=nano.specs','--specs=nosys.specs','-Wall','-Wextra','-Werror','-I',str(P/'App_Logic/UI/inc'),*map(str,sources),str(HERE/'test_pages.c'),'-I',str(P/'Graphics/UI/inc'),'-I',str(P/'Drivers/BSP/inc'),'-I',str(P/'Graphics/Port/inc'),'-T',str(OUT/'test.ld'),'-Wl,-e,test_trip','-Wl,--start-group','-lc','-lm','-lgcc','-lnosys','-Wl,--end-group','-o',str(elf)]
    cmd+=['-DDATA_DEBUG='+str(data_debug),'-I',str(P/'App_Logic/Config/inc')]
    if opt=='-Oz':
        names=set(re.findall(r'uint32_t (test_\w+)\(void\)',(HERE/'test_pages.c').read_text()))
        cmd+=['-flto',*[f'-Wl,--undefined={name}' for name in sorted(names|{'get_assertions'})]]
    r=subprocess.run(cmd,capture_output=True,text=True);assert r.returncode==0,r.stdout+r.stderr
    nm=subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),str(elf)],text=True)
    syms={x.split()[2]:int(x.split()[0],16) for x in nm.splitlines() if len(x.split())==3}
    uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS);uc.mem_map(0x10000000,0x80000);uc.mem_map(0x20000000,0x40000)
    uc.reg_write(UC_ARM_REG_C1_C0_2,0xf00000);uc.reg_write(UC_ARM_REG_FPEXC,0x40000000)
    data=elf.read_bytes();off=struct.unpack_from('<I',data,28)[0];sz,num=struct.unpack_from('<HH',data,42)
    for i in range(num):
        kind,offset,va,pa,filesz,memsz,flags,align=struct.unpack_from('<8I',data,off+i*sz)
        if kind==1 and filesz:uc.mem_write(va,data[offset:offset+filesz])
    uc.reg_write(UC_ARM_REG_XPSR,0x1000000);uc.reg_write(UC_ARM_REG_SP,0x2003f000)
    def call(name):
        uc.reg_write(UC_ARM_REG_LR,0x1007fff1);uc.emu_start(syms[name]|1,0x1007fff0,count=100000000)
        assert uc.reg_read(UC_ARM_REG_PC)==0x1007fff0,'Instruction limit: '+name
        return uc.reg_read(UC_ARM_REG_R0)
    names=sorted(x for x in syms if x.startswith('test_'))
    for name in names:
        failure=call(name);assert not failure,f'{opt}/{name}: C assertion failed at test_state.c:{failure}'
    report.append({'data_debug':data_debug,'optimization':opt,'tests':names,'assertions':call('get_assertions'),'status':'PASS'})
result={'runs':report,'sources':{x.name:hashlib.sha256(x.read_bytes()).hexdigest() for x in sources},'scope':'Actual ARM C model only; no hardware or LVGL rendering claimed.'}
(OUT/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
