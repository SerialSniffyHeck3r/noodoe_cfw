"""Actual ARM Bootstrap inspects used/reset photos, with no hardware access.
The optional input is produced by actual Product PhotoStore_ResetSlots().
Legacy mode demonstrates the old format error without changing the fixture.
"""
from pathlib import Path
import argparse,gc
parser=argparse.ArgumentParser();parser.add_argument('--legacy',action='store_true');parser.add_argument('--product-reset',type=Path);parser.add_argument('--output',type=Path);parser.add_argument('--opt',choices=['O0','Os'],default='Os')
parser.add_argument('--storage-source',type=Path,help='Compile preserved pre-fix source for the regression baseline')
options=parser.parse_args()
HERE=Path(__file__).resolve().parent
code=(HERE/'fast_run.py').read_text(encoding='utf-8').split('u=machine(raw)')[0]
destination=options.output or HERE/'photo-reset-output'
code=code.replace("OUT=HERE/'fast-output'",'OUT=Path('+repr(str(destination.resolve()))+')')
code=code.replace("opt='-Os'","opt='-"+options.opt+"'")
if options.storage_source:
    code=code.replace('# Reuse the same compiler/source contract',"sources=[options.storage_source if p.name=='bootstrap_storage.c' else p for p in sources]\n# Reuse the same compiler/source contract")
    code=code.replace("setup=source[source.index('    args=['):source.index('    nm=')]", "setup=source[source.index('    args=['):source.index('    nm=')]\nsetup=setup.replace(\"    args+=list(map(str,sources))\",\"    args+=['-I',str(P/'Middlewares/Noodoe/Storage/src')]\\n    args+=list(map(str,sources))\")")
exec(compile(code,str(HERE/'fast_run.py'),'exec'))

HEADER=4096;BANK=163840
def offset(slot,bank=0):return 65536+slot*2*BANK+bank*BANK
def reheader(p,at,field,value):
    struct.pack_into('<I',p,at+field,value);struct.pack_into('<I',p,at+4088,zlib.crc32(p[at:at+4088]))
def reset(p,count=6):
    p=bytearray(p)
    for bank in range(count):p[65536+bank*BANK:65536+bank*BANK+HEADER]=b'\xff'*HEADER
    return bytes(p)
cases=[('fresh empty',images[3],0,0),('used valid JPEG',pic,0,0)]
for count in range(1,7):cases.append(('reset after header '+str(count),reset(pic,count),0,6))
dirty=bytearray(reset(pic));dirty[offset(0)+HEADER:offset(0)+BANK]=b'X'*(BANK-HEADER)
cases.append(('empty header with arbitrary stale body',bytes(dirty),0,6))
if options.product_reset:
    result=options.product_reset.read_bytes();assert len(result)==1048576
    # Product fixture has a different mocked silicon UID. Adapt only the
    # container identity/CRC; bank headers and stale JPEG bodies stay exact.
    assert struct.unpack_from('<3I',result,20)==(0x12345678,0x90abcdef,0x1234)
    assert result[:4096]!=b'\xff'*4096
    assert any(result[offset(s)+HEADER:offset(s)+BANK]!=b'\xff'*(BANK-HEADER) for s in range(3))
    assert all(result[65536+n*BANK:65536+n*BANK+HEADER]==b'\xff'*HEADER for n in range(6))
    adapted=bytearray(result)
    for field,word in [(20,1),(24,2),(28,3)]:reheader(adapted,0,field,word)
    assert adapted[4096:]==result[4096:]
    cases.append(('actual Product reset result (fixture UID adapted)',bytes(adapted),0,6))
for label,field,changed_value in [('wrong photo UID',20,99),('unsupported photo schema',4,2),('zero JPEG length',68,0),('oversized JPEG',68,131073),('wrong JPEG magic',64,0)]:
    bad=bytearray(pic);reheader(bad,offset(0),field,changed_value);cases.append((label,bytes(bad),6,6))
bad=bytearray(pic);bad[offset(0)+4096]^=1;cases.append(('body CRC failure',bytes(bad),6,6))
bad=bytearray(pic);bad[offset(0)+4092]^=1;cases.append(('uncommitted only header',bytes(bad),6,6))
bad=bytearray(reset(pic));bad[offset(0)+4095]=0;cases.append(('partially erased header',bytes(bad),6,6))
bad=bytearray(pic);reheader(bad,0,20,99);cases.append(('wrong container UID',bytes(bad),6,6))
bad=bytearray(pic);bad[offset(0)+BANK:offset(0)+BANK+HEADER]=b'X'*HEADER
cases.append(('valid A with torn B',bytes(bad),0,0))
rows=[]
for name,container,fixed_error,legacy_error in cases:
    disk=bytearray(expected);disk[first[3]:first[3]+1048576]=swapped(container)
    m=machine(bytes(disk));req(m,0x59,struct.pack('<I',3))
    for _ in range(100):
        state=call(m,'FastPump',128)
        if state in (8,9):break
    status=req(m,0x56,error=True);error=struct.unpack_from('<I',status,8)[0]
    assert error==(legacy_error if options.legacy else fixed_error),(name,state,error)
    assert state==(9 if error else 8),(name,state,error)
    assert struct.unpack_from('<I',status,12)[0]==1048576
    assert not value(m,'test_mutations') and not value(m,'test_bad_write')
    for at in range(0,len(disk),1048576):assert m.mem_read(0x90000000+at,1048576)==disk[at:at+1048576],name
    rows.append(dict(case=name,state=state,error=error,bytes=1048576,mutations=0,whole_nor_unchanged=True))
    print(rows[-1],flush=True);del m,disk;gc.collect()
result=dict(passed=True,legacy=options.legacy,cases=rows,bootstrap_sha256=hashlib.sha256((options.storage_source or P/'Middlewares/Noodoe/Storage/src/bootstrap_storage.c').read_bytes()).hexdigest(),limits='Actual ARM with memory NOR and accelerated SHA/memcpy. Product fixture UID adapted; photo banks exact. Not RF or power-loss timing.')
(OUT/'results.json').write_text(json.dumps(result,indent=2));print('PASS',len(rows),flush=True)
