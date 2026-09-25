"""Exercise the unmodified production texture retirement function on ARM."""
from pathlib import Path
H=Path(__file__).resolve().parent;P=H.parents[2]
source=P/'Graphics/UI/src/product_music_graphic.c'
s=source.read_text();start=s.index('static uint32_t BindText(');end=s.index('\nvoid MusicGraphic_Update',start)
(H/'actual_function.inc').write_text(s[start:end])
s=(H.parent/'companion_control/run.py').read_text()
s=s.replace("str(P/'App_Logic/Control/src/CompanionControl.c'),str(P/'App_Logic/UI/src/phone_content.c'),",'')
exec(compile(s,str(H/'run.py'),'exec'))
