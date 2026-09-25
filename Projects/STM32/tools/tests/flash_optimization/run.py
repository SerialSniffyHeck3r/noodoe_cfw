"""Execute before/after EVE operations and bounded formatting on ARM M4.

The upstream reference is compiled from the immutable vendor file. Generated
test-only formatter changes only selection/names so both adapters can coexist.
"""
from pathlib import Path
import json, struct, subprocess, sys
H=Path(__file__).resolve().parent;P=H.parents[2];O=H/'output';O.mkdir(exist_ok=True)
sys.path.insert(0,str(P.parents[1]/'.tools/analysis-python'))
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_R0, UC_ARM_REG_XPSR, UC_ARM_REG_PC
TC=Path('C:/ST/STM32CubeIDE_1.18.1/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344/tools/bin')
LV=P/'Middlewares/Third_Party/LVGL'
def run(cmd):
    r=subprocess.run(list(map(str,cmd)),capture_output=True,text=True)
    if r.returncode:raise RuntimeError(r.stdout+r.stderr)
    return r.stdout
reference=(LV/'src/stdlib/builtin/lv_sprintf_builtin.c').read_text()
reference=reference.replace('#if LV_USE_STDLIB_SPRINTF == LV_STDLIB_BUILTIN','#if 1',1)
(O/'reference_format.c').write_text(reference)
(O/'test.ld').write_text('MEMORY { FLASH(rx): ORIGIN = 0x10000000, LENGTH = 256K\n RAM(rwx): ORIGIN = 0x20000000, LENGTH = 128K }\n SECTIONS { .text : { *(.text*) *(.rodata*) } > FLASH\n .data : { *(.data*) } > RAM\n .bss(NOLOAD) : { *(.bss*) *(COMMON) } > RAM\n end = .; }')
reports=[]
for opt in ('O0','Os','Oz'):
    flags=['-mcpu=cortex-m4','-mthumb','-std=c11','-'+opt,'-ffunction-sections','-fdata-sections','-DLV_CONF_INCLUDE_SIMPLE','-DNOODOE_PRODUCT=1','-DNOODOE_INTEGRATED=1','-I'+str(P/'Graphics/Port/inc'),'-I'+str(LV),'-I'+str(LV/'src/stdlib/builtin')]
    inputs=[(LV/'src/draw/eve/lv_draw_eve_arc.c',['-Dlv_draw_eve_arc=ReferenceArc']),
            (P/'Graphics/Port/src/graphics_eve_arc.c',[]),
            (O/'reference_format.c',['-Dlv_snprintf=ReferenceSnprintf','-Dlv_vsnprintf=ReferenceVsnprintf']),
            (LV/'src/stdlib/clib/lv_sprintf_clib.c',[]),
            (LV/'src/stdlib/builtin/lv_string_builtin.c',[]),
            (LV/'src/misc/lv_math.c',[]),(H/'test_port.c',[])]
    objects=[]
    for i,(source,extra) in enumerate(inputs):
        obj=O/f'{opt}-{i}.o';run([TC/'arm-none-eabi-gcc.exe',*flags,*extra,'-c',source,'-o',obj]);objects.append(obj)
    elf=O/(opt+'.elf')
    run([TC/'arm-none-eabi-gcc.exe',*flags,'-nostartfiles','--specs=nano.specs','--specs=nosys.specs',*objects,'-T',O/'test.ld','-Wl,--gc-sections','-Wl,-u,test_arcs','-Wl,-u,test_format','-lc','-lgcc','-o',elf])
    syms={x.split()[2]:int(x.split()[0],16) for x in run([TC/'arm-none-eabi-nm.exe',elf]).splitlines() if len(x.split())==3}
    u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS);u.mem_map(0x10000000,0x40000);u.mem_map(0x20000000,0x20000)
    data=elf.read_bytes();off=struct.unpack_from('<I',data,28)[0];size,num=struct.unpack_from('<HH',data,42)
    for i in range(num):
        kind,offset,va,pa,n,memsz,_,_=struct.unpack_from('<8I',data,off+i*size)
        if kind==1 and n:u.mem_write(va,data[offset:offset+n])
    def value(name):return struct.unpack('<I',u.mem_read(syms[name],4))[0]
    for fn in ('test_arcs','test_format'):
        u.reg_write(UC_ARM_REG_XPSR,0x1000000);u.reg_write(UC_ARM_REG_SP,0x2001fff0);u.reg_write(UC_ARM_REG_LR,0x1003fff1)
        u.emu_start(syms[fn]|1,0x1003fff0,count=800000000)
        assert u.reg_read(UC_ARM_REG_PC)==0x1003fff0,'instruction limit '+fn
        assert u.reg_read(UC_ARM_REG_R0)==0,(opt,fn,'line',u.reg_read(UC_ARM_REG_R0),'case',value('failed_case'))
    report=dict(optimization=opt,arcs=value('test_cases'),formats=value('format_cases'),result='PASS')
    reports.append(report);print(json.dumps(report),flush=True)
(O/'results.json').write_text(json.dumps(reports,indent=2)+'\n')
