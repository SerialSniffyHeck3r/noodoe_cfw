"""Actual ARM transfer/CRC/decoder lifecycle; storage device is stubbed."""
from pathlib import Path
import sys,io
from PIL import Image
H=Path(__file__).resolve().parent;P=H.parents[2]
buf=io.BytesIO();Image.new('RGB',(16,16),(210,80,40)).save(buf,format='JPEG')
(H/'jpeg.inc').write_text('static const unsigned char jpeg[]={'+','.join(map(str,buf.getvalue()))+'};')
preview=P.parents[2]/'NoodoeInstaller/Android/app/build/phone-preview'
parts=[]
for name,extension in (('packed_fixture','rle'),('mask_fixture','a4')):
    raw=(preview/('notification-roomy'+('-gpu' if extension=='a4' else '')+'.'+extension)).read_bytes()
    parts.append('static const unsigned char '+name+'[]={'+','.join(map(str,raw))+'};')
(H/'output').mkdir(exist_ok=True)
for name in ('notification-large','header-large','replies-large','notification-compact-sample','header-compact'):
 for extension in ('rle','a4'):
  raw=(preview/(name+'.'+extension)).read_bytes()
  parts.append('static const unsigned char '+name.replace('-','_')+'_'+extension+'[]={'+','.join(map(str,raw))+'};')
(H/'output/panel_fixture.h').write_text('\n'.join(parts))
music=preview.parent/'music-preview'
(H/'output/music_fixture.h').write_text('\n'.join('static const unsigned char '+name+'[]={'+','.join(map(str,(music/file).read_bytes()))+'};' for name,file in [('music_raw','tile.raw'),('music_rle','tile.rle')]))
s=(H.parent/'companion_control/run.py').read_text()
s=s.replace('LENGTH = 128K','LENGTH = 256K').replace('0x20000)', '0x40000)').replace('0x1001fff0','0x1003fff0')
s=s.replace("LENGTH = 512K", "LENGTH = 2M").replace('0x80000)', '0x200000)').replace('0x2007fff0','0x201ffff0')
s=s.replace("incs=[", "incs=[H]+[")
s=s.replace("str(P/'App_Logic/Control/src/CompanionControl.c'),str(P/'App_Logic/UI/src/phone_content.c')", "str(P/'Middlewares/Noodoe/Photos/src/phone_visual.c'),str(P/'Middlewares/Noodoe/Photos/src/photo_jpeg.c'),str(P/'Middlewares/Noodoe/Photos/src/music_tiles.c'),str(P/'Drivers/BSP/src/noodoe_crc32.c')")
s=s.replace("cmd=[", "incs.append(P/'Middlewares/Third_Party/LVGL/src/libs/tjpgd')\n cmd=[")
s=s.replace('count=300000000','count=1500000000')
s=s.replace("if not row['passed']:print(row);raise SystemExit(1)","if not row['passed']:print(row,hex(u.reg_read(UC_ARM_REG_PC)),struct.unpack('<I',u.mem_read(syms['checks'],4)),bytes(u.mem_read(syms['g_phone_visual'],36)).hex());raise SystemExit(1)")
exec(compile(s,str(H/'run.py'),'exec'))
