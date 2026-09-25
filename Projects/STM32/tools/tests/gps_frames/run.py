from pathlib import Path
import subprocess,sys,struct,re,json
H=Path(__file__).resolve().parent;P=H.parents[2];R=P.parents[2]
E=R/'Reversing/analysis/2026-09-24-session-media-brightness';baseline='--baseline' in sys.argv
sys.path[:0]=[str(P/'tools'),str(R/'Reversing/.tools/analysis-python')]
from recovery_gate_build import TC
from unicorn import *
from unicorn.arm_const import *
o=H/('baseline' if baseline else 'output');o.mkdir(exist_ok=True)
inc=[P/'Middlewares/Third_Party/LVGL',P/'Graphics/UI/inc',P/'Graphics/Port/inc',P/'Drivers/BSP/inc',P/'App_Logic/UI/inc',P/'App_Logic/Config/inc',P/'Middlewares/Noodoe/Resources/inc',P/'Middlewares/Noodoe/Photos/inc']
inc.append(P/'App_Logic/Vehicle/inc')
args=[str(TC/'arm-none-eabi-gcc.exe'),'-mcpu=cortex-m4','-mthumb','-mfpu=fpv4-sp-d16','-mfloat-abi=hard','-Os','-g','-DLV_CONF_INCLUDE_SIMPLE','-DNOODOE_PRODUCT=1','-DNOODOE_INTEGRATED=1','-DDEBUG',*['-I'+str(i)for i in inc]]
def run(a):
 r=subprocess.run(a,capture_output=True,text=True);(o/'build.log').write_text(r.stdout+r.stderr)
 if r.returncode:raise RuntimeError(r.stderr[-7000:])
run(args+['-c',str(H/'test.c'),'-o',str(o/'probe.o')])
objects=[P/'Debug'/x.strip('"') for x in (P/'Debug/objects.list').read_text().splitlines()]
run(args+['-c',str(E/'before/Reversing/STM32CubeProjects/FuckNudo_Noodoe_CFW_Project/App_Logic/UI/src/phone_trail.c' if baseline else P/'App_Logic/UI/src/phone_trail.c'),'-o',str(o/'trail.o')])
objects=[x for x in objects if x.name!='phone_trail.o']+[o/'trail.o']
run(args+['-c',str(P/'Graphics/UI/src/dashboard_pages_view.c'),'-o',str(o/'pages.o')])
objects=[x for x in objects if x.name!='dashboard_pages_view.o']+[o/'pages.o']
# Retain original wrappers and project code. Test wrappers affect hardware only.
wrap=re.findall(r'__wrap_(\w+)\(', (H/'test.c').read_text())
wrap+=['lv_draw_eve_ramg_get_addr','lv_eve_scissor','lv_draw_eve_label','lv_eve_color','lv_draw_eve_image']
(o/'test.ld').write_text('MEMORY { FLASH(rx): ORIGIN = 0x08020000, LENGTH = 2M\n RAM(rwx): ORIGIN = 0x20000000, LENGTH = 4M\n CCM(rwx): ORIGIN = 0x10000000, LENGTH = 64K }\nSECTIONS { .text : { *(.text*) *(.rodata*) } > FLASH\n .data : { *(.data*) } > RAM\n .bss(NOLOAD) : { *(.bss*) *(COMMON) } > RAM\n .ccm_bss(NOLOAD) : { *(.ccm_bss*) } > CCM\n /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) } }')
(o/'objects.rsp').write_text('\n'.join('"'+str(x).replace('\\','/')+'"'for x in objects))
elf=o/'probe.elf'
run(args+['@'+str(o/'objects.rsp'),str(o/'probe.o'),'-nostartfiles','--specs=nano.specs','--specs=nosys.specs','-Wl,--gc-sections','-Wl,-e,Setup','-Wl,--undefined=Frame','-Wl,--undefined=MarkerFrames','-Wl,--undefined=OdoModal',*['-Wl,--wrap='+s for s in set(wrap)],'-Wl,-T,'+str(o/'test.ld'),'-o',str(elf)])
syms={s[2]:int(s[0],16)for l in subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),str(elf)],text=True).splitlines()if len(s:=l.split())==3}
u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
for a,n in [(0x08000000,0x400000),(0x10000000,0x10000),(0x20000000,0x400000),(0xd0000000,0x200000),(0xe0000000,0x100000)]:u.mem_map(a,n)
data=elf.read_bytes();ph=struct.unpack_from('<I',data,28)[0];ps,pn=struct.unpack_from('<HH',data,42)
for i in range(pn):
 k,pos,a,_,n,_,_,_=struct.unpack_from('<8I',data,ph+i*ps)
 if k==1 and n:u.mem_write(a,data[pos:pos+n])
u.mem_write(0xd0000000,(P/'Resources/NOODOE.RSC').read_bytes())
u.reg_write(UC_ARM_REG_C1_C0_2,0xf00000);u.reg_write(UC_ARM_REG_FPEXC,0x40000000)
stop=0x083ffff0
def read(name):return struct.unpack('<I',u.mem_read(syms[name],4))[0]
def call(name,*a):
 u.reg_write(UC_ARM_REG_SP,0x203ffff0);u.reg_write(UC_ARM_REG_LR,stop|1);u.reg_write(UC_ARM_REG_XPSR,0x1000000)
 for reg,v in zip([UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3],a):u.reg_write(reg,v)
 try:u.emu_start(syms[name]|1,stop,count=30000000)
 except Exception as e:raise RuntimeError((name,hex(u.reg_read(UC_ARM_REG_PC)),e))
 assert u.reg_read(UC_ARM_REG_PC)==stop,(name,hex(u.reg_read(UC_ARM_REG_PC)),read('failure'),read('words')*4,a)
 assert not read('failure'),read('failure')
 return u.reg_read(UC_ARM_REG_R0)
assert call('Setup')==0
call('MarkerFrames')
print('Actual LVGL draw-time peak lifecycle passed',flush=True)
print('Actual LVGL shell initialized',flush=True)
t=2000;rows=[]
for heading in range(0,360,30):
 for kind in [6,1,6,2,6,3,6,5,6,7,6,0]:
  for dt in [0,20,40,60,80,100,120,140,160,180,200,220,240,280]:
   try:call('Frame',kind,t+dt,heading,heading>=180)
   except AssertionError:
    if not baseline:raise
    result=dict(reproduced=True,failure_line=read('failure'),bytes=read('words')*4,heading=heading,kind=kind,frames=read('frames'));assert result['bytes']>8192
    (o/'results.json').write_text(json.dumps(result,indent=2));print(result);sys.exit(0)
   rows.append(dict(kind=kind,heading=heading,time=t+dt,bytes=read('words')*4))
  t+=500
 assert read('background_draws')>0
 result=dict(hardware=False,scope='Full LVGL shell + GPS/other page transitions, two480px background cross-fade, dark/light; mocked physical I/O',max_bytes=read('max_words')*4,frames=read('frames'),background_draws=read('background_draws'),blend_frames=read('blend_frames'),worst=sorted(rows,key=lambda r:r['bytes'],reverse=True)[:10])
 (o/'results.json').write_text(json.dumps(result,indent=2));print({k:v for k,v in result.items() if k!='worst'},flush=True)

assert not baseline, 'Baseline no longer reproduces overflow'
assert read('blend_frames')>0
assert read('max_words')*4<8192
call('OdoModal');result['odo_modal_bytes']=read('words')*4;assert result['odo_modal_bytes']<8192
(o/'results.json').write_text(json.dumps(result,indent=2));print('ODO modal bytes:',result['odo_modal_bytes'])
