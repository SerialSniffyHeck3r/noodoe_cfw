"""Execute actual descriptors/binders on ARM; compare every auxiliary byte."""
from pathlib import Path
import hashlib,json,re,struct,subprocess,sys,zipfile
H=Path(__file__).resolve().parent;P=H.parents[2];O=H/'output';O.mkdir(exist_ok=True)
sys.path[:0]=[str(P/'tools'),str(P.parents[1]/'.tools/analysis-python')]
from product_aux_assets import montserrat,ICONS,transform_icons
from resource_install import TC,check_slot
from validate_image import Elf32
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_R0,UC_ARM_REG_XPSR,UC_ARM_REG_PC
slot=(P/'Resources/slot.bin').read_bytes();check_slot(slot)
manifest=json.loads((P/'Resources/manifest.json').read_text())
assert (P/'Graphics/UI/src/product_montserrat_font.c').read_text()==montserrat()
before=P.parents[1]/'analysis/2026-09-20-independent-recovery/before-managed-source.zip'
identities=[]
with zipfile.ZipFile(before) as z:
    for entry in manifest['entries'][12:]:
        name=entry['name']
        if name=='icon_status':
            raw=(P/'Graphics/Assets/Icons/status-masks.bin').read_bytes()
            assert raw==slot[4096+entry['offset']:4096+entry['offset']+entry['bytes']]
            identities.append(dict(name=name,bytes=len(raw),sha256=hashlib.sha256(raw).hexdigest()));continue
        if name=='font_montserrat_14':
            text=(P/'Middlewares/Third_Party/LVGL/src/font/lv_font_montserrat_14.c').read_text(encoding='utf-8');symbol='glyph_bitmap'
        else:
            filename=next((n for n,(role,_,_) in ICONS.items() if name=='icon_'+role),'product_icon_fonts.c')
            text=z.read('Graphics/UI/src/'+filename).decode();symbol='bitmap' if name=='icon_footer' else 'masks'
            current=(P/'Graphics/UI/src'/filename).read_text()
            assert transform_icons(filename,current)==current
        body=re.search(r'const uint8_t '+symbol+r'\[.*?\n\};',text,re.S).group(0)
        body=re.sub(r'/\*.*?\*/|//[^\n]*','',body,flags=re.S)
        raw=bytes(int(x,16) for x in re.findall(r'0x([a-fA-F0-9]{1,2})\b',body))
        got=slot[4096+entry['offset']:4096+entry['offset']+entry['bytes']]
        assert raw==got,(name,len(raw),len(got))
        identities.append(dict(name=name,bytes=len(raw),sha256=hashlib.sha256(raw).hexdigest()))
sources=[P/'Graphics/UI/src'/n for n in [*ICONS,'product_icon_fonts.c','product_montserrat_font.c','product_aux_assets.c','product_status_icon_data.c']]+[H/'test.c']
(O/'test.ld').write_text('MEMORY { FLASH(rx): ORIGIN = 0x10000000, LENGTH = 512K\nRAM(rwx): ORIGIN = 0x20000000, LENGTH = 256K }\nSECTIONS { .text : { *(.text*) *(.rodata*) } > FLASH\n.data : { *(.data*) } > RAM AT>FLASH\n.bss(NOLOAD) : { *(.bss*) *(COMMON) } > RAM end = .; _end = .; }')
report=[]
for opt in ('O0','Os','Oz'):
    elf=O/(opt+'.elf')
    cmd=[str(TC/'arm-none-eabi-gcc.exe'),'-mcpu=cortex-m4','-mthumb','-'+opt,'-DNOODOE_PRODUCT=1','-DNOODOE_INTEGRATED=1','-DLV_CONF_INCLUDE_SIMPLE','-nostartfiles','--specs=nano.specs','--specs=nosys.specs','-Wall','-Wextra','-Werror',*['-I'+str(P/p) for p in ('Graphics/UI/inc','Graphics/Port/inc','Middlewares/Third_Party/LVGL','Middlewares/Noodoe/Resources/inc')],*map(str,sources),'-T'+str(O/'test.ld'),'-Wl,-e,test_bind','-Wl,--undefined=get_assertions','-o',str(elf)]
    if opt=='Oz':cmd+=['-flto']
    r=subprocess.run(cmd,capture_output=True,text=True);assert not r.returncode,r.stderr
    nm=subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),str(elf)],text=True);syms={w[2]:int(w[0],16) for line in nm.splitlines() if len(w:=line.split())==3}
    u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS);u.mem_map(0x10000000,0x80000);u.mem_map(0x20000000,0x40000)
    data=elf.read_bytes();parsed=Elf32(data)
    for p in parsed.programs:
        if p['type']==1 and p['filesz']:u.mem_write(p['vaddr'],data[p['offset']:p['offset']+p['filesz']])
    u.reg_write(UC_ARM_REG_SP,0x2003fff0);u.reg_write(UC_ARM_REG_XPSR,0x1000000)
    def call(fn):
        u.reg_write(UC_ARM_REG_LR,0x1007fff1);u.emu_start(syms[fn]|1,0x1007fff0,count=10000000)
        assert u.reg_read(UC_ARM_REG_PC)==0x1007fff0
        return u.reg_read(UC_ARM_REG_R0)
    assert call('test_bind')==0;report.append(dict(optimization=opt,assertions=call('get_assertions')))
result=dict(pixel_bytes=sum(e['bytes'] for e in identities),byte_identity=identities,runs=report,hardware_tested=False)
(O/'results.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
