"""Execute the production toast model as Cortex-M4 code; no LVGL mock timer."""
from pathlib import Path
HERE=Path(__file__).resolve().parent
template=(HERE.parent/'dashboard_pages/run.py').read_text()
start=template.index('sources=');end=template.index('\nreport=[]',start)
template=template[:start]+"sources=[P/'App_Logic/UI/src/popup_notifications.c']"+template[end:]
template=template.replace("str(HERE/'test_pages.c')","str(HERE/'test_popup.c')")
template=template.replace("'-Wl,-e,test_trip'","'-Wl,-e,test_hint'")
exec(compile(template,str(__file__),'exec'))
