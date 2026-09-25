"""Generate pinned Material Icons Round fuel/Bluetooth/location A4 resources."""
from pathlib import Path
import hashlib,json
from PIL import Image,ImageDraw,ImageFont
P=Path(__file__).resolve().parents[1];A=P/'Graphics/Assets/Icons'
source=A/'source/MaterialIconsRound-Regular.otf'
upstream=json.loads((A/'source/upstream.json').read_text())
assert hashlib.sha256(source.read_bytes()).hexdigest()==upstream['files'][source.name]['sha256']
points=dict(line.split() for line in (A/'source/codepoints.txt').read_text().splitlines())
data=bytearray();records=[]
for name,size in [('local_gas_station',88),('bluetooth',24),('location_on',24)]:
 font=ImageFont.truetype(str(source),size,layout_engine=ImageFont.Layout.BASIC)
 cp=int(points[name],16);x0,y0,x1,y1=font.getbbox(chr(cp),anchor='ls')
 im=Image.new('L',(size,size));ImageDraw.Draw(im).text(((size-(x1-x0))//2-x0,(size-(y1-y0))//2-y0),chr(cp),font=font,fill=255,anchor='ls')
 q=[(v*15+127)//255 for v in im.tobytes()];packed=bytes(q[i]<<4|q[i+1] for i in range(0,len(q),2))
 records.append(dict(name=name,codepoint=cp,size=size,offset=len(data),bytes=len(packed),sha256=hashlib.sha256(packed).hexdigest()));data+=packed
target=A/'status-masks.bin'
if target.exists():
 # A changed rasterizer is an asset format change, never silently regenerated.
 assert target.read_bytes()==data,'Raster differs from checked-in resource; review before replacing'
else:target.write_bytes(data)
(A/'status-manifest.json').write_text(json.dumps(dict(family='Google Material Icons Round',license='Apache-2.0',source_commit=upstream['commit'],source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),icons=records,bitmap_bytes=len(data),sha256=hashlib.sha256(data).hexdigest()),indent=2)+'\n')
print('Verified status masks:',len(data),hashlib.sha256(data).hexdigest())
