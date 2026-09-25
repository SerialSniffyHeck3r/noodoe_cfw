"""Production GPS geometry and unchanged EVE backend executed on Cortex-M4.
Counts GPS display-list words, not a full EVE raster/screenshot or real FPS.
"""
from pathlib import Path
import json,sys,subprocess,struct,hashlib
H=Path(__file__).resolve().parent;P=H.parents[2];O=H/'output';O.mkdir(exist_ok=True)
E=P.parents[1]/'analysis/2026-09-24-gps-tearing'
sys.path[:0]=[str(P/'tools'),str(P.parents[1]/'.tools/analysis-python')]
from recovery_gate_build import TC
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS
from unicorn.arm_const import *
a=(P/'Graphics/UI/src/dashboard_pages_view.c').read_text(encoding='utf-8')
old=(E/'before/dashboard_pages_view.c').read_text(encoding='utf-8')
fade=(E/'before/Gps_GridFade.h').read_text()
(O/'new_grid.inc').write_text(a[a.index('static void PlotLine('):a.index('static void Plot(')])
(O/'old_grid.inc').write_text(fade[fade.index('static inline uint32_t'):fade.rindex('#endif')]+old[old.index('static void GridLine('):old.index('static void Plot(')].replace('GridLine(','OldGridLine('))
ld=O/'test.ld';ld.write_text('MEMORY { FLASH(rx): ORIGIN = 0x10000000, LENGTH = 128K\n RAM(rwx): ORIGIN = 0x20000000, LENGTH = 512K }\nSECTIONS { .text : { *(.text*) *(.rodata*) } > FLASH\n .data : { *(.data*) } > RAM\n .bss(NOLOAD) : { *(.bss*) *(COMMON) } > RAM\n /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) } }')
V=P/'Middlewares/Third_Party/LVGL';results=[]
for opt in ('O0','Os','Oz'):
 elf=O/(opt+'.elf')
 src=[H/'test.c',P/'App_Logic/UI/src/phone_trail.c',V/'src/draw/eve/lv_eve.c',V/'src/draw/eve/lv_draw_eve_line.c',P/'Graphics/Port/src/graphics_eve_clip.c',P/'Graphics/Port/src/graphics_subpixel_arc.c',P/'Graphics/Port/src/graphics_eve_viewport.c',V/'src/misc/lv_math.c',V/'src/misc/lv_color.c']
 cmd=[str(TC/'arm-none-eabi-gcc.exe'),'-mcpu=cortex-m4','-mthumb','-mfpu=fpv4-sp-d16','-mfloat-abi=hard','-'+opt,'-g','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-DLV_CONF_INCLUDE_SIMPLE',*['-I'+str(x)for x in [O,V,P/'App_Logic/UI/inc',P/'Graphics/UI/inc',P/'Graphics/Port/inc']],*map(str,src),'-nostartfiles','--specs=nosys.specs','--specs=nano.specs','-Wl,--wrap=lv_eve_scissor','-Wl,--gc-sections','-Wl,-e,Test','-Wl,-T,'+str(ld),'-o',str(elf)]
 r=subprocess.run(cmd,capture_output=True,text=True);(O/(opt+'.log')).write_text(r.stdout+r.stderr)
 if r.returncode:raise RuntimeError(r.stderr)
 syms={s[2]:int(s[0],16)for l in subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),str(elf)],text=True).splitlines()if len(s:=l.split())==3}
 u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS);u.mem_map(0x10000000,0x20000);u.mem_map(0x20000000,0x80000)
 u.reg_write(UC_ARM_REG_C1_C0_2,0xf00000);u.reg_write(UC_ARM_REG_FPEXC,0x40000000)
 data=elf.read_bytes();ph=struct.unpack_from('<I',data,28)[0];ps,pn=struct.unpack_from('<HH',data,42)
 for i in range(pn):
  k,o,a,_,n,_,_,_=struct.unpack_from('<8I',data,ph+i*ps)
  if k==1 and n:u.mem_write(a,data[o:o+n])
 stop=0x1001fff0;u.reg_write(UC_ARM_REG_SP,0x2007fff0);u.reg_write(UC_ARM_REG_LR,stop|1);u.reg_write(UC_ARM_REG_XPSR,0x1000000)
 u.emu_start(syms['Test']|1,stop,count=700000000)
 row={s:struct.unpack('<I',u.mem_read(syms[s],4))[0]for s in ['failure','old_max','new_max','grid_max','cases','bad_vertices','ring_mask_bytes']}
 row.update(opt=opt,returned=u.reg_read(UC_ARM_REG_PC)==stop);print(json.dumps(row),flush=True);results.append(row)
 (O/'results.json').write_text(json.dumps({'scope':__doc__,'hardware':False,'runs':results,'sources':{str(s):hashlib.sha256(s.read_bytes()).hexdigest()for s in src}},indent=2))
 assert not row['failure'] and row['returned'],row
