"""Run the actual TripLabel function with the shipped font metrics on Cortex-M4."""
from pathlib import Path
import json
HERE=Path(__file__).resolve().parent
P=HERE.parents[2];OUT=HERE/'output';OUT.mkdir(exist_ok=True)
source=P/'Graphics/UI/src/dashboard_pages_view.c'
c=source.read_text();start=c.index('static void TripLabel(');end=c.index('static void Plot(',start)
(OUT/'trip_label.h').write_text(c[start:end])
manifest=json.loads((P/'Graphics/Assets/Fonts/manifest.json').read_text())
fonts={k:v for fam in manifest['families'].values() for k,v in fam['fonts'].items()}
parts=[]
for name in ('number_36','number_40','text_20'):
    f=fonts['product_'+name];parts.append('static const lv_font_t '+name+'={'+str(f['line_height'])+','+str(f['base_line'])+', {')
    for g in f['glyphs']:
        w,h,x,y=g['box'];parts.append(f"[{g['codepoint']}]={{{h},{y}}},")
    parts.append('}};')
(OUT/'font_metrics.h').write_text('\n'.join(parts))
template=(HERE.parent/'dashboard_pages/run.py').read_text()
start=template.index('sources=');end=template.index('\nreport=[]',start)
template=template[:start]+'sources=[]'+template[end:]
template=template.replace("str(HERE/'test_pages.c')","str(HERE/'test_typography.c')")
template=template.replace("'-Wl,-e,test_trip'","'-Wl,-e,test_fixed_rows'")
template=template.replace("'sources':{x.name:hashlib.sha256(x.read_bytes()).hexdigest() for x in sources}","'sources':{source.name:hashlib.sha256(source.read_bytes()).hexdigest(),'fonts':hashlib.sha256((P/'Graphics/Assets/Fonts/manifest.json').read_bytes()).hexdigest()}")
template=template.replace('Actual ARM C model only; no hardware or LVGL rendering claimed.','Actual TripLabel C with shipped font metrics; LVGL Label positioning recorded.')
exec(compile(template,str(__file__),'exec'))
