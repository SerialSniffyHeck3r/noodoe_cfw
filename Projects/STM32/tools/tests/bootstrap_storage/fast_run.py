"""Packet-level scoped installer regression, actual ARM storage state machine.

NOR is memory-backed. SHA primitives/memcpy are accelerated with host equivalents;
this tests guards and bytes, not MCU/RF timing. Never opens hardware.
"""
from pathlib import Path
HERE=Path(__file__).resolve().parent
source=(HERE/'run.py').read_text()
exec(compile(source[:source.index("for opt in ('-O0','-Os'):")],str(HERE/'run.py'),'exec'))
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R1,UC_ARM_REG_R2
from bootstrap_storage import SIZES
OUT=HERE/'fast-output';OUT.mkdir(exist_ok=True)
(OUT/'test.ld').write_text((HERE/'output/test.ld').read_text())
opt='-Os';elf=OUT/'test.elf'
# Reuse the same compiler/source contract as the legacy tests.
setup=source[source.index('    args=['):source.index('    nm=')]
exec(compile('\n'.join(line[4:] for line in setup.splitlines()).replace('-u,GateReader','-u,GateReader,-u,FastInit,-u,FastPacket,-u,FastPump,-u,FastReply,-u,FastTimeout'),'<compile>','exec'))
nm=subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),str(elf)],text=True)
syms={s[2]:int(s[0],16) for line in nm.splitlines() if len(s:=line.split())==3}
b=elf.read_bytes();phoff=struct.unpack_from('<I',b,28)[0];phsize,phnum=struct.unpack_from('<HH',b,42)
images=[rsc,record(1,[1,2,3])+b'\xff'*(131072-4096),record(2,[1,2,3])+b'\xff'*(262144-4096),
        record(3,[1,2,3],struct.pack('<2I',0x31465043,1))+b'\xff'*(1048576-4096),image,*gate,log_container([1,2,3])]
order=[4,0,1,2,3,8,5,6,7]
# Add an existing stock file; preservation is not tested on an empty FAT only.
from resource_install import Fat
fs=Fat(raw);fs.setfat(2,0xfff)
e=bytearray(32);e[:11]=b'STOCK   JPG';e[11]=32;struct.pack_into('<H',e,26,2);struct.pack_into('<I',e,28,32768)
fs.b[0x5000:0x5020]=e;fs.b[0x9000:0x11000]=bytes(range(256))*128
raw=bytes(swapped(fs.b));expected=raw;first=[0]*9
for k in order:
    plan,expected=create_plan(expected,k,images[k],uid=[1,2,3]);first[k]=plan['address']
begin=hashlib.sha256(swapped(raw[:0x9000])).digest()+struct.pack('<9I',*first)
def machine(disk):
    u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
    for a,n in ((0x10000000,0x40000),(0x20000000,0x30000),(0x90000000,0x8000000),(0xc0000000,0x100000),(0x11000000,0x100000)):u.mem_map(a,n)
    for i in range(phnum):
        t,o,va,_,size,_,_,_=struct.unpack_from('<8I',b,phoff+i*phsize)
        if t==1 and size:u.mem_write(va,b[o:o+size])
    u.mem_write(0x90000000,disk);hashes={}
    def hook(u,pc,size,data):
        a=u.reg_read(UC_ARM_REG_R0);v=u.reg_read(UC_ARM_REG_R1);n=u.reg_read(UC_ARM_REG_R2)
        if pc==syms['UpdateSha256_Init']:hashes[a]=hashlib.sha256()
        elif pc==syms['UpdateSha256_Feed']:hashes[a].update(u.mem_read(v,n))
        elif pc==syms['UpdateSha256_Final']:u.mem_write(v,hashes[a].digest())
        elif pc==syms['memcpy']:
            if n:u.mem_write(a,bytes(u.mem_read(v,n)))
        elif pc==syms['memset']:
            if n:u.mem_write(a,bytes([v&255])*n)
        u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
    for name in ('UpdateSha256_Init','UpdateSha256_Feed','UpdateSha256_Final','memcpy','memset'):
        u.hook_add(UC_HOOK_CODE,hook,begin=syms[name],end=syms[name])
    call(u,'FastInit');return u
def call(u,name,arg=0):
    u.reg_write(UC_ARM_REG_SP,0x2002fff0);u.reg_write(UC_ARM_REG_LR,STOP|1);u.reg_write(UC_ARM_REG_R0,arg);u.reg_write(UC_ARM_REG_XPSR,0x1000000)
    u.emu_start(syms[name]|1,STOP,count=2_000_000_000,timeout=60_000_000)
    assert u.reg_read(UC_ARM_REG_PC)==STOP,(name,'timeout')
    return u.reg_read(UC_ARM_REG_R0)
packets=0;payload_bytes=0
def req(u,op,p=b'',error=False):
    global packets,payload_bytes
    packets+=1;payload_bytes+=len(p)
    u.mem_write(0x11000000,struct.pack('<I',len(p))+p)
    assert call(u,'FastPacket',op)==0
    call(u,'FastPump',1);assert call(u,'FastReply')==0
    n=struct.unpack('<I',u.mem_read(0x11000000,4))[0];r=bytes(u.mem_read(0x11000004,n))
    if not error:assert struct.unpack_from('<I',r)[0]==0,(hex(op),r[:32].hex())
    return r
def pump(u,wanted):
    for i in range(100):
        state=call(u,'FastPump',512)
        if state==wanted:return
        assert state!=9,req(u,0x56,error=True).hex()
    raise AssertionError(('timeout',state,wanted))
def value(u,name):return struct.unpack('<I',u.mem_read(syms[name],4))[0]
u=machine(raw)
r=req(u,0x80,struct.pack('<II',0x9000,960));assert r[32:]==raw[0x9000:0x93c0]
assert struct.unpack_from('<I',req(u,0x80,struct.pack('<II',0x7ffffff,960),True))[0]!=0
wrong=bytearray(begin);wrong[32]^=0x10
req(u,0x81,bytes(wrong));call(u,'FastPump',2000)
assert value(u,'test_mutations')==0 and struct.unpack_from('<I',req(u,0x56,error=True),4)[0]==9
u=machine(raw);req(u,0x81,begin);pump(u,8);baseline=req(u,0x56)[32:64]
assert value(u,'test_mutations')==0
for k in order:
    print('scoped install',k,flush=True);image=images[k];digest=hashlib.sha256(image).digest()
    req(u,0x52,struct.pack('<II',k,len(image))+digest+baseline)
    assert struct.unpack_from('<I',req(u,0x83,struct.pack('<II',1,65536),True))[0]!=0
    pos=0
    while pos<len(image):
        span=0
        while span<65536 and pos+span<len(image) and image[pos+span]==255:span+=1
        if span>=64:req(u,0x83,struct.pack('<II',pos,span));pos+=span
        else:
            data=image[pos:pos+960];req(u,0x53,struct.pack('<I',pos)+data);pos+=len(data)
    req(u,0x54);pump(u,6);req(u,0x55,digest)
    # Fork the current writing state in memory, prove timeout cannot write,
    # then restore the harness state to exercise successful completion too.
    saved=bytes(u.mem_read(0x20000000,0x30000))
    assert call(u,'FastTimeout')==0
    u.mem_write(0x20000000,saved)
    pump(u,8)
assert bytes(u.mem_read(0x90000000,0x8000000))==expected,'Write differs from independent plan'
assert not value(u,'test_bad_write')
req(u,0x82);pump(u,8);assert req(u,0x56)[32:64]==baseline
# Resume must independently re-audit the new FAT and preserve the same baseline.
call(u,'FastInit');resume=hashlib.sha256(swapped(expected[:0x9000])).digest()+begin[32:]+baseline
req(u,0x81,resume);pump(u,8)
for k in order:req(u,0x59,struct.pack('<I',k));pump(u,8)
req(u,0x82);pump(u,8)
# A changed stock byte must fail preservation verification, never grant staging.
u.mem_write(0x90009000,b'X');req(u,0x82)
for i in range(100):
    if call(u,'FastPump',512)==9:break
else:raise AssertionError('Changed stock file accepted')
assert not value(u,'test_bad_write')
# New Bootstrap after a stock round trip: staging bytes can legitimately differ.
# Adopt all nine existing extents read-only against a NEW baseline; never rewrite
# files to make them match. Historical resume above remains strict.
reuse_disk=bytearray(expected);reuse_disk[0x7f80000]^=0x5a
u=machine(bytes(reuse_disk))
reuse=hashlib.sha256(swapped(expected[:0x9000])).digest()+begin[32:]+struct.pack('<I',0x45535552)
req(u,0x81,reuse);pump(u,8);fresh_baseline=req(u,0x56)[32:64]
assert fresh_baseline!=baseline
assert struct.unpack_from('<I',req(u,0x52,struct.pack('<II',0,len(images[0]))+hashlib.sha256(images[0]).digest()+fresh_baseline,True))[0]!=0
for k in order:req(u,0x59,struct.pack('<I',k));pump(u,8)
req(u,0x82);pump(u,8)
assert value(u,'test_mutations')==0 and bytes(u.mem_read(0x90000000,0x8000000))==reuse_disk
assert req(u,0x56)[32:64]==fresh_baseline
# Partial publication and a wrong declared extent cannot be adopted.
for bad_disk,bad_plan in ((raw,reuse),(bytes(reuse_disk),reuse[:32]+struct.pack('<I',first[0]+32768)+reuse[36:])):
    u=machine(bad_disk);req(u,0x81,bad_plan)
    for i in range(100):
        if call(u,'FastPump',512)==9:break
    else:raise AssertionError('Invalid existing scope accepted')
    assert value(u,'test_mutations')==0
result=dict(passed=True,packets=packets,request_payload_bytes=payload_bytes,files=9,blank_photo_slots=3,
    existing_stock_and_reserved_tail_preserved=True,resume_verified=True,read_only_adoption_verified=True,corruption_rejected=True,write_timeouts_revoked_without_extra_mutations=9,
    limits='Memory NOR and accelerated SHA/memcpy; not real wireless, power-cut or timing verification')
(OUT/'results.json').write_text(json.dumps(result,indent=2));print(result,flush=True)
