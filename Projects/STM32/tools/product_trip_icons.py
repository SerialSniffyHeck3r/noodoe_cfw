"""Build the trip glyph masks from pinned Material Icons Round.

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
names=['arrow_upward','average_slash','two_wheeler','signpost']
font=ImageFont.truetype(str(source),24,layout_engine=ImageFont.Layout.BASIC)
data=[];records=[]
for name in names:
    cp=int(points[name],16) if name in points else 0;im=Image.new('L',(24,24))
    if cp:
        x0,y0,x1,y1=font.getbbox(chr(cp),anchor='ls')
        ImageDraw.Draw(im).text(((24-(x1-x0))//2-x0,(24-(y1-y0))//2-y0),chr(cp),font=font,fill=255,anchor='ls')
        ink=im.crop(im.getbbox());im=Image.new('L',(24,24));im.paste(ink,((24-ink.width)//2,(24-ink.height)//2))
    else:
        # Mathematical average mark: project-owned circle/slash, not a new
        # Material icon or a modified Google glyph. Supersample only this mark.
        large=Image.new('L',(192,192));d=ImageDraw.Draw(large)
        d.ellipse((32,32,152,152),outline=255,width=16)
        d.line((36,168,148,16),fill=255,width=16)
        im=large.resize((24,24),Image.Resampling.LANCZOS)
    pixels=[(v*15+127)//255 for v in im.tobytes()]
    packed=bytes((pixels[i]<<4)|pixels[i+1] for i in range(0,len(pixels),2))
    records.append({'name':name,'codepoint':cp,'ink_box':im.getbbox(),'sha256':hashlib.sha256(packed).hexdigest()})
    data.append(packed)
text='''/* Generated Material Icons Round trip subset and project-owned mathematical average mark.
 * Four24px alpha masks; provenance/licenses: Graphics/Assets/Icons/TRIP_ICON_NOTICES.txt. */
#include "Product_TripIcons.h"
static const uint8_t masks[PRODUCT_TRIP_ICON_COUNT][288]={
'''
for blob in data:
    text+=' {\n'+ '\n'.join(','.join(f'0x{x:02x}' for x in blob[i:i+24])+',' for i in range(0,len(blob),24))+'\n },\n'
text+='};\nstatic const lv_image_dsc_t icons[PRODUCT_TRIP_ICON_COUNT]={\n'
for n in range(len(names)):text+=f' {{.header={{.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_A4,.w=24,.h=24,.stride=12}},.data_size=288,.data=masks[{n}]}},\n'
text+='''};
/* ROM lookup. Invalid mode returnsNULL; no fallback or device I/O. */
const lv_image_dsc_t *Product_TripIcon(uint32_t mode)
{return mode<PRODUCT_TRIP_ICON_COUNT?&icons[mode]:NULL;}
'''
from product_aux_assets import transform_icons
target=P/'Graphics/UI/src/product_trip_icon_data.c';target.write_text(transform_icons(target.name,text),encoding='utf-8')
record={'family':'Google Material Icons Round','source_commit':upstream['commit'],'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'mask_size':24,'format':'LVGL A4 / EVE L4 alpha','bitmap_bytes':sum(map(len,data)),'icons':records,'generated_sha256':hashlib.sha256(target.read_bytes()).hexdigest()}
(A/'trip-manifest.json').write_text(json.dumps(record,indent=2)+'\n')
(A/'TRIP_ICON_NOTICES.txt').write_text('arrow_upward, two_wheeler, signpost: Google Material Icons Round, pinned commit '+upstream['commit']+'; subset/raster conversion. Apache-2.0; see MODE_ICON_LICENSE.txt.\naverage_slash: project-owned geometric circle/slash; not a Google icon.\n')
print(json.dumps(record,indent=2))
