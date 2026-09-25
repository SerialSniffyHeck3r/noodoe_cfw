"""Execute production page positioning and panel draw clipping on ARM.
LVGL drawing is captured at its API boundary; this is not an EVE screenshot.
"""
from pathlib import Path
import json,hashlib
H=Path(__file__).resolve().parent;P=H.parents[2];O=H/'output';O.mkdir(exist_ok=True)
a=(P/'Graphics/UI/src/dashboard_pages_view.c').read_text()
b=(P/'Graphics/UI/src/product_music_graphic.c').read_text()
# Alpha's position prefix is exercised with unchanged opacity; the suffix only
# sets independent style alpha values and is not substituted or reimplemented.
prefix=a[a.index('static void Alpha('):a.index('    if(s->alpha==alpha)return;')]
(O/'position.inc').write_text(prefix+'    (void)alpha;\n}\n')
(O/'progress.inc').write_text(b[b.index('static void Progress('):b.index('uint32_t MusicGraphic_Create(')])
runner=(H.parent/'companion_control/run.py').read_text()
start=runner.index(' cmd=');end=runner.index(' r=subprocess.run',start)
runner=runner[:start]+" cmd=[str(TC/'arm-none-eabi-gcc.exe'),'-mcpu=cortex-m4','-mthumb','-'+opt,'-g','-Wall','-Wextra','-Werror','-I'+str(O),str(H/'test.c'),'-nostartfiles','--specs=nosys.specs','--specs=nano.specs','-Wl,-T,'+str(ld),'-o',str(elf)]\n"+runner[end:]
exec(compile(runner,str(__file__),'exec'))
result=json.loads((O/'results.json').read_text())
result['scope']='Production position and draw commands with mock LVGL API; no hardware/EVE frame validation'
result['sources']={name:hashlib.sha256((P/'Graphics/UI/src'/name).read_bytes()).hexdigest() for name in ['dashboard_pages_view.c','product_music_graphic.c']}
(O/'results.json').write_text(json.dumps(result,indent=2))
