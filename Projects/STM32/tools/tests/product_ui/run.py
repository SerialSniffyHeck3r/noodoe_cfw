"""Compile the actual pure-C UI state machines for Cortex-M4 and execute them.
No Python reimplementation of state logic; no board or peripheral access.
"""
import hashlib,json,struct,subprocess,sys,re
from pathlib import Path
HERE=Path(__file__).resolve().parent
P=HERE.parents[2]
sys.path.insert(0,str(P.parents[1]/'.tools/analysis-python'))
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_R0,UC_ARM_REG_XPSR,UC_ARM_REG_PC
TC=Path('C:/ST/STM32CubeIDE_1.18.1/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344/tools/bin')
OUT=HERE/'output';OUT.mkdir(parents=True,exist_ok=True)
(OUT/'test.ld').write_text('MEMORY { FLASH(rx): ORIGIN = 0x10000000, LENGTH = 128K\n RAM(rwx): ORIGIN = 0x20000000, LENGTH = 64K }\n SECTIONS { .text : { *(.text*) *(.rodata*) } > FLASH\n .data : { *(.data*) } > RAM\n .bss (NOLOAD) : { *(.bss*) *(COMMON) } > RAM }')
sources=sorted((P/'App_Logic/UI/src').glob('ui_*.c'))+[P/'App_Logic/UI/src/ignition_session.c',P/'App_Logic/UI/src/speed_home_model.c',P/'App_Logic/UI/src/speed_home_startup.c',P/'App_Logic/UI/src/product_preview.c',P/'Graphics/UI/src/page_transition.c']
report=[]
sources += [P/'App_Logic/UI/src/phone_calls.c',P/'App_Logic/UI/src/notification_preview.c',HERE/'test_calls.c',HERE/'test_notification_preview.c']
for opt in ('-O0','-Os','-Oz'):
    elf=OUT/(opt[1:]+'.elf')
    cmd=[str(TC/'arm-none-eabi-gcc.exe'),'-mcpu=cortex-m4','-mthumb','-std=c11',opt,*(['-flto'] if opt=='-Oz' else []),'\x2dffreestanding','-fno-builtin','-nostdlib','-Wall','-Wextra','-Werror','-I',str(P/'App_Logic/UI/inc'),'-I',str(P/'App_Logic/Config/inc'),*map(str,sources),str(HERE/'test_state.c'),str(HERE/'test_speed.c'),str(HERE/'test_preview.c'),'-I',str(P/'Graphics/Port/inc'),'-I',str(P/'Graphics/UI/inc'),'-T',str(OUT/'test.ld'),'-Wl,-e,test_power','-lgcc','-o',str(elf)]
    if opt=='-Oz':
        exports=['memset','memcpy','get_assertions','get_speed_assertions']+re.findall(r'unsigned (test_\w+)\(void\)', ''.join((HERE/f).read_text() for f in ['test_state.c','test_speed.c','test_preview.c','test_calls.c','test_notification_preview.c']))
        cmd += ['-Wl,--undefined='+n for n in exports]
    r=subprocess.run(cmd,capture_output=True,text=True);assert r.returncode==0,r.stdout+r.stderr
    nm=subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),str(elf)],text=True)
    syms={x.split()[2]:int(x.split()[0],16) for x in nm.splitlines() if len(x.split())==3}
    uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS);uc.mem_map(0x10000000,0x20000);uc.mem_map(0x20000000,0x10000)
    data=elf.read_bytes();off=struct.unpack_from('<I',data,28)[0];sz,num=struct.unpack_from('<HH',data,42)
    for i in range(num):
        kind,offset,va,pa,filesz,memsz,flags,align=struct.unpack_from('<8I',data,off+i*sz)
        if kind==1 and filesz:uc.mem_write(va,data[offset:offset+filesz])
    uc.reg_write(UC_ARM_REG_XPSR,0x1000000);uc.reg_write(UC_ARM_REG_SP,0x2000f000)
    def call(name):
        uc.reg_write(UC_ARM_REG_LR,0x1001fff1);uc.emu_start(syms[name]|1,0x1001fff0,count=100000000)
        assert uc.reg_read(UC_ARM_REG_PC)==0x1001fff0,'Instruction limit: '+name
        return uc.reg_read(UC_ARM_REG_R0)
    names=sorted(x for x in syms if x.startswith('test_'))
    for name in names:
        failure=call(name);assert not failure,f'{opt}/{name}: C assertion failed at test_state.c:{failure}'
    report.append({'optimization':opt,'tests':names,'assertions':call('get_assertions'),'speed_assertions':call('get_speed_assertions'),'status':'PASS'})
result={'runs':report,'sources':{x.name:hashlib.sha256(x.read_bytes()).hexdigest() for x in sources},'scope':'Actual ARM C model only; no hardware or LVGL rendering claimed.'}
(OUT/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
