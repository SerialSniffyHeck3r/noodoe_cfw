"""Actual ARM resource migration through scoped NDCP packets, never hardware.
SHA/memcpy are accelerated exactly as in fast_run; timings are not MCU timings.
"""
from pathlib import Path
HERE=Path(__file__).resolve().parent
code=(HERE/'fast_run.py').read_text(encoding='utf-8').split('u=machine(raw)')[0]
code=code.replace("OUT=HERE/'fast-output'","OUT=HERE/'resource-reseed-output'")
code=code.replace('-u,FastTimeout','-u,FastTimeout,-u,FastPhase,-u,FastCancel')
exec(compile(code,str(HERE/'fast_run.py'),'exec'))
from resource_install import check_slot
import gc

# Valid historical asset ID: change one byte, then repair its entry CRC, body
# digest and header CRC. It is different, not structurally corrupt.
slot=524288;old=bytearray(rsc[:slot]);old[4096]^=1
entry_bytes=struct.unpack_from('<I',old,56)[0]
struct.pack_into('<I',old,60,zlib.crc32(old[4096:4096+entry_bytes]))
total,count=struct.unpack_from('<II',old,8)
old[16:48]=hashlib.sha256(old[48:48+count*16]+old[4096:4096+total]).digest()
struct.pack_into('<I',old,4088,zlib.crc32(old[:4088]));old=bytes(old)
check_slot(old);check_slot(rsc[:slot]);assert old[16:48]!=rsc[16:48]
new=rsc[:slot];newsha=hashlib.sha256(new).digest();base=first[0]
rows=[]

def initial(container):
    disk=bytearray(expected);disk[base:base+2*slot]=swapped(container);return disk

def reopen(disk):
    m=machine(bytes(disk))
    plan=hashlib.sha256(swapped(disk[:0x9000])).digest()+begin[32:]+struct.pack('<II',0x32504353,1)
    req(m,0x81,plan);pump(m,8)
    req(m,0x59,struct.pack('<I',4));pump(m,8)
    return m,req(m,0x56)[32:64]

def start(m,proof,prehash=None):
    prior=bytes(swapped(m.mem_read(0x90000000+base,2*slot)))
    req(m,0x88,struct.pack('<II',0,slot)+newsha+proof+(prehash or hashlib.sha256(prior).digest()))

def prepare(m):
    pump(m,3)
    for at in range(0,slot,960):req(m,0x53,struct.pack('<I',at)+new[at:at+960])
    req(m,0x54);pump(m,6)
    status=req(m,0x56);assert struct.unpack_from('<I',status,20)[0]==slot
    return struct.unpack_from('<I',status,16)[0]

def check_unchanged_outside(m,disk,target):
    for at in range(0,len(disk),65536):
        end=at+65536
        if end<=target or at>=target+slot:
            assert m.mem_read(0x90000000+at,65536)==disk[at:end],('outside write',hex(at))
        else:
            for lo,hi in ((at,min(end,target)),(max(at,target+slot),end)):
                if hi>lo:assert m.mem_read(0x90000000+lo,hi-lo)==disk[lo:hi]
    assert not value(m,'test_bad_write')

for name,container,target_offset in [('old A / erased B',old+b'\xff'*slot,slot),
        ('old A / interrupted B',old+b'X'*slot,slot),
        ('invalid A / old B',b'\xff'*slot+old,0),
        ('two old valid slots',old+old,slot)]:
    disk=initial(container);m,proof=reopen(disk);start(m,proof);target=prepare(m)
    assert target==base+target_offset and not value(m,'test_mutations')
    req(m,0x55,newsha);pump(m,8)
    assert bytes(swapped(m.mem_read(0x90000000+target,slot)))==new
    check_unchanged_outside(m,disk,target)
    req(m,0x59,struct.pack('<I',0));pump(m,8)
    rows.append({'case':name,'result':'saved, exact readback, old slot/FAT/other files preserved'})
    print(rows[-1],flush=True);del m,disk;gc.collect()

for name,container,wrong_hash in [('wrong preimage',old+b'\xff'*slot,True),
        ('no valid old header',b'\xff'*(2*slot),False),
        ('old body corrupt',old[:4096]+bytes([old[4096]^1])+old[4097:]+b'\xff'*slot,False)]:
    disk=initial(container);m,proof=reopen(disk);start(m,proof,b'x'*32 if wrong_hash else None)
    state=call(m,'FastPump',2000)
    if state==3:
        for at in range(0,slot,960):req(m,0x53,struct.pack('<I',at)+new[at:at+960])
        req(m,0x54);state=call(m,'FastPump',10000)
    assert state==9 and not value(m,'test_mutations')
    rows.append({'case':name,'result':'rejected before mutation'});print(rows[-1],flush=True)
    del m,disk;gc.collect()

# Power loss at each distinct writing phase preserves the old valid slot and
# can safely resume with a fresh audited preimage, including a partial B header.
for stop_phase in (1,2,3,4):
    disk=initial(old+b'\xff'*slot);m,proof=reopen(disk);start(m,proof);target=prepare(m);req(m,0x55,newsha)
    for _ in range(30000):
        call(m,'FastPump',1)
        if call(m,'FastPhase')==stop_phase and value(m,'test_mutations')>0:break
    else:raise AssertionError(('phase never reached',stop_phase))
    assert bytes(swapped(m.mem_read(0x90000000+base,slot)))==old
    partial=bytearray(disk);partial[target:target+slot]=m.mem_read(0x90000000+target,slot)
    check_unchanged_outside(m,disk,target);del m;gc.collect()
    m,proof=reopen(partial);start(m,proof);prepare(m);req(m,0x55,newsha);pump(m,8)
    assert bytes(swapped(m.mem_read(0x90000000+target,slot)))==new
    check_unchanged_outside(m,disk,target)
    rows.append({'case':'power interruption phase '+str(stop_phase),'result':'old slot preserved, fresh audited retry succeeds'})
    print(rows[-1],flush=True);del m,disk,partial;gc.collect()

(OUT/'results.json').write_text(json.dumps({'cases':rows,'hardware_access':False,'timing_verified':False},indent=2))
