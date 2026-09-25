"""Execute real ARM tinfl+recovery source; compare all stock bytes and seeks."""
from pathlib import Path
import hashlib,json,struct,subprocess,sys
H=Path(__file__).resolve().parent;P=H.parents[2];O=H/'output';O.mkdir(exist_ok=True)
sys.path[:0]=[str(P/'tools'),str(P.parents[1]/'.tools/analysis-python')]
from bootstrap_pack import pack
from bootstrap_build import TC
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC,UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_XPSR
stock=P.parents[1]/'VERY_IMPORTANT_ORIGINAL_NOODOE_BOOTLOARDER/noodoe_full_flash_dump_A.bin'
pack(stock,O);raw=stock.read_bytes()[65536:]
(O/'test.ld').write_text('MEMORY {FLASH(rx): ORIGIN=0x08010000, LENGTH=1M\n RAM(rwx): ORIGIN=0x20000000, LENGTH=192K\n CCM(rwx): ORIGIN=0x10000000, LENGTH=64K}\nSECTIONS{.text:{*(.text*) *(.rodata*)}>FLASH\n .data:{*(.data*)}>RAM\n.bss(NOLOAD):{*(.bss*) *(COMMON)}>RAM\n.ccm_bss(NOLOAD):{*(.ccm_bss*)}>CCM\n/DISCARD/:{*(.ARM.exidx*) *(.ARM.extab*)}}'.replace('ORIGIN=','ORIGIN = ').replace('LENGTH=','LENGTH = ').replace('.text:', '.text : ').replace('.data:', '.data : ').replace('(NOLOAD):','(NOLOAD) : ').replace('/DISCARD/:','/DISCARD/ : '))
defs=['MINIZ_NO_ARCHIVE_APIS','MINIZ_NO_DEFLATE_APIS','MINIZ_NO_STDIO','MINIZ_NO_TIME','MINIZ_NO_MALLOC','MINIZ_NO_ZLIB_APIS','NDEBUG']
elf=O/'image.elf'
cmd=[str(TC/'arm-none-eabi-gcc.exe'),'-mcpu=cortex-m4','-mthumb','-Os','-ffunction-sections','-fdata-sections','-nostartfiles','--specs=nano.specs','--specs=nosys.specs',*['-D'+d for d in defs],'-I'+str(P/'App_Logic/Bootstrap/inc'),'-I'+str(P/'Middlewares/Third_Party/miniz'),str(P/'App_Logic/Bootstrap/src/bootstrap_image.c'),str(P/'Middlewares/Third_Party/miniz/miniz_tinfl.c'),str(O/'stock_asset.s'),'-Wl,-T,'+str(O/'test.ld'),'-Wl,--gc-sections','-Wl,-e,BootstrapImage_Read','-o',str(elf)]
r=subprocess.run(cmd,capture_output=True,text=True);assert r.returncode==0,r.stderr
syms={w[2]:int(w[0],16) for l in subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),str(elf)],text=True).splitlines() if len(w:=l.split())==3}
u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
for a,n in [(0x08010000,0x100000),(0x20000000,0x30000),(0x10000000,0x10000)]:u.mem_map(a,n)
data=elf.read_bytes();h=struct.unpack_from('<16sHHIIIIIHHHHHH',data)
for i in range(h[10]):
    kind,off,a,_,n,_,_,_=struct.unpack_from('<8I',data,h[5]+i*h[9])
    if kind==1 and n:u.mem_write(a,data[off:off+n])
def read(at,n):
    u.reg_write(UC_ARM_REG_SP,0x2002fff0);u.reg_write(UC_ARM_REG_XPSR,0x1000000)
    for reg,v in [(UC_ARM_REG_R0,0),(UC_ARM_REG_R1,at),(UC_ARM_REG_R2,0x20028000),(UC_ARM_REG_R3,n),(UC_ARM_REG_LR,0x0810fff1)]:u.reg_write(reg,v)
    u.emu_start(syms['BootstrapImage_Read']|1,0x0810fff0,count=200000000)
    assert u.reg_read(UC_ARM_REG_PC)==0x0810fff0,'instruction budget'
    return u.reg_read(UC_ARM_REG_R0),bytes(u.mem_read(0x20028000,min(n,4096)))
actual=bytearray()
for off in range(0,len(raw),4096):
    err,b=read(off,4096);assert err==0,(off,err);actual+=b
assert actual==raw
for off,n in [(0,256),(32760,32),(65530,70),(458496,256),(19,511),(0,4096)]:
    err,b=read(off,n);assert err==0 and b==raw[off:off+n],(off,n)
for off,n in [(0,0),(0,4097),(0x70000,1),(0xffffffff,4),(0x6ffff,2)]:assert read(off,n)[0]!=0
result=dict(passed=True,whole_image_bytes=len(actual),sha256=hashlib.sha256(actual).hexdigest(),seeks=6,bounds_rejections=5)
(O/'result.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
