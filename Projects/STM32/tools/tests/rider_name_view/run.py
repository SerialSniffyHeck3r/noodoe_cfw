"""Production welcome formatting/drawing on ARM with the shipped font metrics.
LVGL draw submissions are recorded; this is not a hardware screenshot.
"""
from pathlib import Path
import json
HERE=Path(__file__).resolve().parent
P=HERE.parents[2];OUT=HERE/'output';OUT.mkdir(exist_ok=True)
source=P/'Graphics/UI/src/power_view.c';code=source.read_text()
a=code.index('static void PrepareWelcome(');b=code.index('/* Immutable text slots',a)
draw_a=code.index('static void Draw(');draw_b=code.index('/* Allocate one transparent',draw_a)
(OUT/'welcome_code.h').write_text(code[a:b]+code[draw_a:draw_b])
m=json.loads((P/'Graphics/Assets/Fonts/manifest.json').read_text())
font=m['families']['text']['fonts']['product_text_32']
(OUT/'font_metrics.h').write_text('static const uint16_t advances[128]={'+','.join(f'[{g["codepoint"]}]={g["advance"]}' for g in font['glyphs'])+'};\n')
template=(HERE.parent/'dashboard_pages/run.py').read_text()
a=template.index('sources=');b=template.index('\nreport=[]',a)
template=template[:a]+'sources=[]'+template[b:]
template=template.replace("((0,'-O0'),(0,'-Os'),(0,'-Oz'),(1,'-O0'),(1,'-Os'),(1,'-Oz'))","((0,'-O0'),(0,'-Os'))")
template=template.replace("str(HERE/'test_pages.c')","str(HERE/'test_welcome.c')")
template=template.replace("'-Wl,-e,test_trip'","'-Wl,-e,test_welcome'")
template=template.replace("'sources':{x.name:hashlib.sha256(x.read_bytes()).hexdigest() for x in sources}","'sources':{source.name:hashlib.sha256(source.read_bytes()).hexdigest(),'font_metrics':hashlib.sha256((P/'Graphics/Assets/Fonts/manifest.json').read_bytes()).hexdigest()}")
template=template.replace('Actual ARM C model only; no hardware or LVGL rendering claimed.','Production welcome formatting and draw submissions; real font advances, mocked LVGL.')
exec(compile(template,str(__file__),'exec'))
