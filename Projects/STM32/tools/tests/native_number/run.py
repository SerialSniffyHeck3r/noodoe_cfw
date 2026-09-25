"""Compare actual ARM A4 composition to independent native Pillow glyph placement."""
import hashlib,json,struct,subprocess,sys
from pathlib import Path
from PIL import Image,ImageFont,ImageDraw
H=Path(__file__).resolve().parent;P=H.parents[2];O=H/'output';O.mkdir(exist_ok=True)
sys.path[:0]=[str(P/'tools'),str(P.parents[1]/'.tools/analysis-python')]
from validate_image import Elf32
from resource_install import TC
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_R0,UC_ARM_REG_XPSR,UC_ARM_REG_PC
font=json.loads((P/'Graphics/Assets/Fonts/manifest.json').read_text())['families']['number']['fonts']['product_number_160']
manifest=json.loads((P/'Resources/manifest.json').read_text());entry=next(e for e in manifest['entries'] if e['name']=='product_number_160')
slot=(P/'Resources/slot.bin').read_bytes();bitmap=slot[4096+entry['offset']:4096+entry['offset']+entry['bytes']]
assert hashlib.sha256(bitmap).hexdigest()==entry['sha256'] and entry['id']==20
parts=['static const uint8_t bitmap[]={'+','.join(map(str,bitmap))+'};',
       'static const lv_font_fmt_txt_glyph_dsc_t glyphs[]={ {0},']
for g in font['glyphs']:
 w,h,x,y=g['box'];parts.append('{.bitmap_index=%d,.box_w=%d,.box_h=%d,.ofs_x=%d,.ofs_y=%d},'%(g['bitmap_index'],w,h,x,y))
parts+=['};','static const lv_font_fmt_txt_dsc_t dsc={.glyph_bitmap=bitmap,.glyph_dsc=glyphs,.bpp=4,.stride=1};',
 'static const lv_font_t font={.dsc=&dsc};']
(O/'fixture.h').write_text('\n'.join(parts))
(O/'test.c').write_text('''#include "Number_Raster.h"
#include <string.h>
#include <stdio.h>
#include "fixture.h"
uint8_t pixels[18434];
bool lv_font_get_glyph_dsc(const lv_font_t *f,lv_font_glyph_dsc_t *g,uint32_t c,uint32_t next){
 (void)f;(void)next;if(c<48||c>57)return false;const lv_font_fmt_txt_glyph_dsc_t *s=&glyphs[c-47];
 *g=(lv_font_glyph_dsc_t){.box_w=s->box_w,.box_h=s->box_h,.ofs_x=s->ofs_x,.ofs_y=s->ofs_y};g->gid.index=c-47;return true;
}
uint32_t test_value(uint32_t value){char text[12];snprintf(text,sizeof(text),"%lu",(unsigned long)value);
 pixels[0]=0xa5;pixels[18433]=0x5a;
 uint32_t ok=NumberRaster_Compose(pixels+1,18432,&font,text);
 return ok&&pixels[0]==0xa5&&pixels[18433]==0x5a;
}''')
(O/'test.ld').write_text('MEMORY { FLASH(rx): ORIGIN = 0x10000000, LENGTH = 256K\nRAM(rwx): ORIGIN = 0x20000000, LENGTH = 128K }\nSECTIONS { .text : { *(.text*) *(.rodata*) } > FLASH\n.data : { *(.data*) } > RAM\n.bss(NOLOAD) : { *(.bss*) *(COMMON) } > RAM end = .; _end = .; }')
face=ImageFont.truetype(str(P/'Graphics/Assets/Fonts/source/ddin/D-DIN.otf'),160,layout_engine=ImageFont.Layout.BASIC)
below=max(face.getbbox(c,anchor='ls')[3] for c in '0123456789')
cases=list(range(20))+[97,98,99,100,101,111,160,188,199,200,298,299,399,400,888,999]
reports=[]
for opt in ('O0','Os','Oz'):
 elf=O/(opt+'.elf')
 cmd=[str(TC/'arm-none-eabi-gcc.exe'),'-mcpu=cortex-m4','-mthumb','-'+opt,'-DLV_CONF_INCLUDE_SIMPLE','-ffunction-sections','-fdata-sections','-nostartfiles','--specs=nano.specs','--specs=nosys.specs','-Wall','-Wextra','-Werror',*['-I'+str(P/p) for p in ('Graphics/UI/inc','Graphics/Port/inc','Middlewares/Third_Party/LVGL')],str(O/'test.c'),str(P/'Graphics/UI/src/product_number_raster.c'),'-T'+str(O/'test.ld'),'-Wl,-e,test_value','-Wl,--gc-sections','-lc','-lgcc','-o',str(elf)]
 r=subprocess.run(cmd,capture_output=True,text=True);assert not r.returncode,r.stdout+r.stderr
 nm=subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),str(elf)],text=True);syms={w[2]:int(w[0],16) for line in nm.splitlines() if len(w:=line.split())==3}
 u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS);u.mem_map(0x10000000,0x40000);u.mem_map(0x20000000,0x20000)
 data=elf.read_bytes()
 for p in Elf32(data).programs:
  if p['type']==1 and p['filesz']:u.mem_write(p['vaddr'],data[p['offset']:p['offset']+p['filesz']])
 u.reg_write(UC_ARM_REG_SP,0x2001fff0);u.reg_write(UC_ARM_REG_XPSR,0x1000000)
 for value in cases:
  u.reg_write(UC_ARM_REG_R0,value);u.reg_write(UC_ARM_REG_LR,0x1003fff1);u.emu_start(syms['test_value']|1,0x1003fff0,count=30000000)
  assert u.reg_read(UC_ARM_REG_PC)==0x1003fff0 and u.reg_read(UC_ARM_REG_R0)==1,(opt,value)
  raw=bytes(u.mem_read(syms['pixels']+1,18432));got=Image.frombytes('L',(288,128),bytes(v*17 for b in raw for v in (b>>4,b&15)))
  expected=Image.new('L',(288,128));draw=ImageDraw.Draw(expected);text=str(value)
  for i,c in enumerate(text):draw.text(((3-len(text)+i)*165//2+(82-round(face.getlength(c)))//2,128-below),c,font=face,fill=255,anchor='ls')
  expected=expected.point(lambda v:((v*15+127)//255)*17)
  if expected.tobytes()!=got.tobytes():
   expected.save(O/'expected.png');got.save(O/'actual.png');print(expected.getbbox(),got.getbbox());print([(i,a,b) for i,(a,b) in enumerate(zip(expected.tobytes(),got.tobytes())) if a!=b][:16]);raise AssertionError((opt,value,'native raster pixel mismatch'))
  if value==160:got.save(O/'native-160.png')
 reports.append(dict(optimization=opt,cases=len(cases),pixel_exact=True,buffer_guards=True))
(O/'results.json').write_text(json.dumps(reports,indent=2));print(json.dumps(reports))
