"""Build six music-control masks from pinned Material Icons Round.

24px alpha masks preserve optical margins. EVE scales/tints these same ROM
images, so animation does not allocate fonts or upload a bitmap per frame.
"""
from pathlib import Path
import hashlib,json
from PIL import Image,ImageDraw,ImageFont
P=Path(__file__).resolve().parents[1];A=P/'Graphics/Assets/Icons'
source=A/'source/MaterialIconsRound-Regular.otf'
upstream=json.loads((A/'source/upstream.json').read_text())
assert hashlib.sha256(source.read_bytes()).hexdigest()==upstream['files'][source.name]['sha256']
points=dict(line.split() for line in (A/'source/codepoints.txt').read_text().splitlines())
names=['speed','warning','check','arrow_back','add','remove']
font=ImageFont.truetype(str(source),24,layout_engine=ImageFont.Layout.BASIC)
data=[];records=[]
for name in names:
    cp=int(points[name],16);im=Image.new('L',(24,24))
    x0,y0,x1,y1=font.getbbox(chr(cp),anchor='ls')
    ImageDraw.Draw(im).text(((24-(x1-x0))//2-x0,(24-(y1-y0))//2-y0),chr(cp),font=font,fill=255,anchor='ls')
    # Center the visible ink, not OTF ascent/descent whitespace. The original
    # raster is not stretched; a taller phone still retains its native shape.
    ink=im.crop(im.getbbox());im=Image.new('L',(24,24));im.paste(ink,((24-ink.width)//2,(24-ink.height)//2))
    pixels=[(v*15+127)//255 for v in im.tobytes()]
    packed=bytes((pixels[i]<<4)|pixels[i+1] for i in range(0,len(pixels),2))
    records.append({'name':name,'codepoint':cp,'ink_box':im.getbbox(),'sha256':hashlib.sha256(packed).hexdigest()})
    data.append(packed)
text='''/* Generated Material Icons Round navigation subset. Apache-2.0.
 * Six24px alpha masks; see Graphics/Assets/Icons/ICON_NOTICES.txt. */
#include "Settings_Icons.h"
static const uint8_t masks[SETTINGS_ICON_COUNT][288]={
'''
for blob in data:
    text+=' {\n'+ '\n'.join(','.join(f'0x{x:02x}' for x in blob[i:i+24])+',' for i in range(0,len(blob),24))+'\n },\n'
text+='};\nstatic const lv_image_dsc_t icons[SETTINGS_ICON_COUNT]={\n'
for n in range(6):text+=f' {{.header={{.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_A4,.w=24,.h=24,.stride=12}},.data_size=288,.data=masks[{n}]}},\n'
text+='''};
/* ROM lookup. Invalid mode returnsNULL; no fallback or device I/O. */
const lv_image_dsc_t *Settings_Icon(uint32_t mode)
{return mode<SETTINGS_ICON_COUNT?&icons[mode]:NULL;}
'''
from product_aux_assets import transform_icons
target=P/'Graphics/UI/src/settings_icon_data.c';target.write_text(transform_icons(target.name,text),encoding='utf-8')
record={'family':'Google Material Icons Round','source_commit':upstream['commit'],'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'mask_size':24,'format':'LVGL A4 / EVE L4 alpha','bitmap_bytes':sum(map(len,data)),'icons':records,'generated_sha256':hashlib.sha256(target.read_bytes()).hexdigest()}
(A/'settings-manifest.json').write_text(json.dumps(record,indent=2)+'\n')
print(json.dumps(record,indent=2))
