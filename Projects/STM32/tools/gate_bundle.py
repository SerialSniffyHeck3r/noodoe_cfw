"""Build a UID-bound independent-gate package; no device access.

The former APP-only writer must not be used for a layout2 ELF. Initial install
is gate+Product at08010000; subsequent updates are Product only at08020000.
Budget failure prevents publication of any deployable bundle.
"""
from pathlib import Path
import argparse,hashlib,json,struct,zlib
from validate_image import Elf32,validate,require,SHF_ALLOC,SHT_NOBITS,contained
from memory_report import report

PRODUCT_BASE=0x08020000
PRODUCT_BYTES=0x60000
COMMIT=0x31544d43

def sha(data):return hashlib.sha256(data).digest()
def put(buf,offset,*words):struct.pack_into('<'+'I'*len(words),buf,offset,*words)
def seal(buf):
    put(buf,4088,zlib.crc32(buf[:4088]),COMMIT)
    return bytes(buf)

def image_container(image,uid,slot,version=1,generation=1):
    require(len(image)==PRODUCT_BYTES,'Gate source must be a complete padded384KiB image')
    require(slot in (0,1),'Invalid source slot')
    h=bytearray(b'\xff'*4096)
    put(h,0,0x314d4947,1,4096,0x80000,*uid,PRODUCT_BASE,PRODUCT_BYTES,generation)
    h[40:72]=sha(image);h[72:116]=image[0x200:0x22c]
    put(h,116,1,version,0)
    data=bytearray(b'\xff'*0x80000);data[:4096]=seal(h);data[4096:4096+len(image)]=image
    # Identity is immutable across every ordinary upload, including a power
    # loss after the inactive mutable image header has been erased.
    h=bytearray(b'\xff'*4096);put(h,0,0x31444947,1,slot,0x80000,*uid,1)
    data[0x7f000:]=seal(h)
    return bytes(data)

def initial_journal(image,uid,generation=1):
    h=bytearray(b'\xff'*4096)
    put(h,0,0x314a4247,2,1,1,0,0xffffffff,0,0)
    h[32:64]=sha(image);h[64:96]=b'\0'*32
    put(h,96,*uid,generation,0,1)
    put(h,120,1,0xffffffff,0)
    h[132:164]=bytes(32)
    put(h,164,0,0,0,0,0)
    return seal(h)+b'\xff'*(0x10000-4096)

def log_container(uid):
    h=bytearray(b'\xff'*4096)
    put(h,0,0x31494c4e,1,0x40000,64,*uid)
    return b'\xff'*0x3f000+seal(h)

def gate_image(path):
    e=Elf32(path.read_bytes());base=0x08010000;limit=PRODUCT_BASE;pieces=[]
    vector=e.section('.isr_vector');require(vector['addr']==base,'Gate vector must occupy S4 start')
    sp,rv=struct.unpack_from('<II',e.section_data(vector))
    require(sp==0x2002ff00 and rv&1 and base<=rv<limit,'Gate vector/SP invalid')
    require((e.entry&~1)==(rv&~1),'Gate entry differs from vector')
    require(e.code_bytes(rv&~1,2)==b'\x72\xb6','Gate must mask IRQ before inherited runtime access')
    for s in e.sections:
        if not s['flags']&SHF_ALLOC or not s['size'] or s['type']==SHT_NOBITS:continue
        a=e.section_lma(s);require(contained(a,s['size'],base,limit),'Gate load bytes escape isolated S4')
        pieces.append((a,e.section_data(s)))
    data=bytearray(b'\xff'*0x10000);end=base
    for a,b in sorted(pieces):
        require(a>=end,'Overlapping gate sections');data[a-base:a-base+len(b)]=b;end=a+len(b)
    require(end>base,'Empty gate');return bytes(data)

def build(gate,product,debug,uid,output):
    release_report=report(product,'Product','Release')
    require(release_report['budget_passed'],
            'Memory gates failed; no installable package: '+repr(release_report['failures']))
    debug_report=report(debug,'Product','Debug')
    require(debug_report['budget_passed'],
            'Memory gates failed; no installable package: '+repr(debug_report['failures']))
    app,manifest=validate(Elf32(product.read_bytes()))
    require(manifest['layout_version']==2,'A legacy APP cannot be placed after the gate')
    require(len(uid)==3 and any(uid),'A specific observed device UID is required')
    app=app.ljust(PRODUCT_BYTES,b'\xff');gate=gate_image(gate)
    require(struct.unpack_from('<III',app,0x200)==(0x51534352,1,1),'Product resources requirement missing')
    require(struct.unpack_from('<4I',app,0x230)==(0x3250554e,2,2,0x60000),'Product trial/rollback contract missing')
    artifacts={'first-install.bin':gate+app,'product-update.bin':app,
               'CFWA.DAT':image_container(app,uid,0),'CFWB.DAT':image_container(app,uid,1),
               'CFWBOOT.DAT':initial_journal(app,uid),'CFWLOG.DAT':log_container(uid)}
    output.mkdir(parents=True,exist_ok=False)
    for name,data in artifacts.items():(output/name).write_bytes(data)
    record=dict(format='NOODOE_GATE_BUNDLE_1',uid=uid,hardware_access=False,wireless_verified=False,
                initial_base='0x08010000',update_base='0x08020000',resource_sha256=app[0x20c:0x22c].hex(),
                release_memory=release_report,debug_memory=debug_report,
                files={n:dict(bytes=len(b),sha256=sha(b).hex()) for n,b in artifacts.items()},
                prerequisites=['fresh full NOR A/B backups','audited provisioned CFWA/B/BOOT/REC/RSC',
                               'exact internal flash preimage','approved resident BL and UID'])
    (output/'manifest.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf-8')
    return record

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--gate',type=Path,required=True);p.add_argument('--product',type=Path,required=True)
    p.add_argument('--debug',type=Path,required=True);p.add_argument('--uid',nargs=3,type=lambda v:int(v,0),required=True)
    p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    print(json.dumps(build(a.gate,a.product,a.debug,a.uid,a.output),indent=2))
