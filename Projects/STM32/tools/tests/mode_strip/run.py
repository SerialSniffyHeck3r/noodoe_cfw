"""Actual Cortex-M4 strip/timeline code; LVGL calls recorded, GPU not emulated."""
from pathlib import Path
HERE=Path(__file__).resolve().parent
template=(HERE.parent/'dashboard_pages/run.py').read_text()
start=template.index('sources=');end=template.index('\nreport=[]',start)
template=template[:start]+"sources=[P/'Graphics/UI/src/product_mode_strip.c',P/'Graphics/UI/src/page_transition.c',P/'Graphics/UI/src/scalar_transition.c']"+template[end:]
template=template.replace("'-I',str(P/'App_Logic/UI/inc'),","'-I',str(HERE),")
template=template.replace("str(HERE/'test_pages.c')","str(HERE/'test_strip.c')")
template=template.replace("'-Wl,-e,test_trip'","'-Wl,-e,test_navigation_motion'")
template=template.replace('Actual ARM C model only; no hardware or LVGL rendering claimed.','Actual strip/timeline C; LVGL calls stubbed. EVE verified separately on hardware.')
template=template.replace("HERE/'test_pages.c'","HERE/'test_strip.c'")
exec(compile(template,str(__file__),'exec'))
