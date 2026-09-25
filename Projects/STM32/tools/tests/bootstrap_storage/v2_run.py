"""Execute scoped-v2 against actual ARM NOR state machine, independent host plan.
Emulated physical program permits only 1->0. No device commands are issued.
"""
from pathlib import Path
HERE=Path(__file__).resolve().parent
text=(HERE/'fast_run.py').read_text(encoding='utf-8')
text=text[:text.index('u=machine(raw)')]
text=text.replace("OUT=HERE/'fast-output'","OUT=HERE/'v2-output'")
text=text.replace('-u,FastTimeout','-u,FastTimeout,-u,FastDisconnect,-u,FastCancel,-u,FastPhase')
exec(compile(text,str(HERE/'fast_run.py'),'exec'))
import gc,faulthandler
faulthandler.dump_traceback_later(180,repeat=True)
for unused in ('empty','valid','fs','pic'):globals().pop(unused,None)
gc.collect()
def equal_disk(m,disk):
    return all(m.mem_read(0x90000000+at,min(1048576,len(disk)-at))==disk[at:at+1048576] for at in range(0,len(disk),1048576))
def disk_snapshot(m):
    disk=bytearray(0x8000000)
    for at in range(0,len(disk),1048576):disk[at:at+1048576]=m.mem_read(0x90000000+at,1048576)
    return disk

u=machine(raw);u.mem_map(0x12000000,0x70000);u.mem_write(0x12000000,stock)
v2=begin+struct.pack('<II',0x32504353,0)
req(u,0x81,v2);pump(u,8);proof=req(u,0x56)[32:64]
assert proof==begin[:32] and value(u,'test_read_bytes')<0x100000
for k in order:
    print('scoped-v2 file',k,flush=True)
    im=images[k];sha=hashlib.sha256(im).digest()
    req(u,0x52,struct.pack('<II',k,len(im))+sha+proof)
    if k in (1,2,3,7,8):req(u,0x8a);pump(u,3)
    else:
        at=0
        while at<len(im):
            if at==4096 and k in (4,6):
                n=0x70000 if k==4 else 0x60000
                req(u,0x87,struct.pack('<IIII',at,0 if k==4 else 1,0,n));pump(u,3);at+=n;continue
            n=min(960,4096-(at&4095),len(im)-at)
            if im[at:at+n]==b'\xff'*n:req(u,0x83,struct.pack('<II',at,n))
            else:req(u,0x53,struct.pack('<I',at)+im[at:at+n])
            at+=n
    assert bytes(u.mem_read(0xc0000000,len(im)))==im,('local generation',k)
    req(u,0x54);pump(u,6);req(u,0x55,sha);pump(u,8)
assert equal_disk(u,expected)
before=value(u,'test_read_bytes');req(u,0x82);pump(u,8)
assert value(u,'test_read_bytes')-before<0x200000,'whole NOR scan came back'
assert not value(u,'test_bad_write')
used=value(u,'test_read_bytes');transferred=payload_bytes
# Reinstall from stock after an older CFW: only A/B/BOOT may be reseeded.
# Includes a used journal, wrong preimages and restart after destructive erase.
old_disk=bytearray(expected)
print('reseed: new version over existing CFW',flush=True)
old_boot=bytearray(images[7]);struct.pack_into('<I',old_boot,12,4)
struct.pack_into('<I',old_boot,4088,zlib.crc32(old_boot[:4088]))
old_disk[first[7]:first[7]+len(old_boot)]=swapped(old_boot)
new_app=bytearray(app);new_app[0x400:0x404]=b'NEW!';new_app=bytes(new_app)
new_images={5:image_container(new_app,[1,2,3],0),6:image_container(new_app,[1,2,3],1),7:initial_journal(new_app,[1,2,3])}
def reopen(disk):
    m=machine(bytes(disk));scope=hashlib.sha256(swapped(disk[:0x9000])).digest()+begin[32:]+struct.pack('<II',0x32504353,1)
    req(m,0x81,scope);pump(m,8);req(m,0x59,struct.pack('<I',4));pump(m,8)
    return m,req(m,0x56)[32:64]
def reseed(m,k,im,proof):
    before=bytes(swapped(m.mem_read(0x90000000+first[k],len(im))))
    sha=hashlib.sha256(im).digest()
    req(m,0x88,struct.pack('<II',k,len(im))+sha+proof+hashlib.sha256(before).digest());pump(m,3)
    for at in range(0,len(im),960):req(m,0x53,struct.pack('<I',at)+im[at:at+960])
    req(m,0x54);pump(m,6);req(m,0x55,sha)
u,proof=reopen(old_disk)
assert struct.unpack_from('<I',req(u,0x88,struct.pack('<II',4,len(images[4]))+hashlib.sha256(images[4]).digest()+proof+b'x'*32,True))[0]!=0
for k in (5,6,7):reseed(u,k,new_images[k],proof);pump(u,8)
target=old_disk[:]
for k in (5,6,7):target[first[k]:first[k]+len(new_images[k])]=swapped(new_images[k])
assert equal_disk(u,target) and not value(u,'test_bad_write')
reseed_boundaries=[]
for k,phase in ((5,1),(7,7),(7,1)):
    print('reseed power loss',k,phase,flush=True)
    del u;gc.collect()
    u,proof=reopen(old_disk);reseed(u,k,new_images[k],proof)
    # Stop after first erase in the selected phase. Last journal record was
    # verified before phase1 can erase the old first record.
    for _ in range(30000):
        ph=call(u,'FastPhase');old=value(u,'test_mutations');call(u,'FastPump',1)
        if ph==phase and value(u,'test_mutations')>old:break
    else:raise AssertionError(('reseed interruption not reached',k,phase))
    interrupted=disk_snapshot(u)
    del u;gc.collect()
    u,proof=reopen(interrupted);reseed(u,k,new_images[k],proof);pump(u,8)
    target=old_disk[:];target[first[k]:first[k]+len(new_images[k])]=swapped(new_images[k])
    assert equal_disk(u,target)
    assert not value(u,'test_bad_write');reseed_boundaries.append([k,phase])
# A safe link loss during an accepted write completes publication. It never
# lends the old grant to a request with a new link epoch.
for phase in (0,1,2,4,5):
    print('disconnect publication phase',phase,flush=True)
    del u;gc.collect()
    u=machine(raw);req(u,0x81,v2);pump(u,8);proof=req(u,0x56)[32:64]
    im=images[1];sha=hashlib.sha256(im).digest()
    req(u,0x52,struct.pack('<II',1,len(im))+sha+proof);req(u,0x8a);pump(u,3);req(u,0x54);pump(u,6)
    # Kind1 is not the declared next first-fit allocation; it MUST fail commit.
    rejected=req(u,0x55,sha,True);assert struct.unpack_from('<I',rejected)[0]!=0
    assert value(u,'test_mutations')==0
    # Start with recovery, the declared first file, then interrupt this phase.
    call(u,'FastInit');req(u,0x81,v2);pump(u,8);proof=req(u,0x56)[32:64]
    im=images[4];sha=hashlib.sha256(im).digest();req(u,0x52,struct.pack('<II',4,len(im))+sha+proof)
    for at in range(0,len(im),960):req(u,0x53,struct.pack('<I',at)+im[at:at+960])
    req(u,0x54);pump(u,6);req(u,0x55,sha)
    for _ in range(10000):
        if call(u,'FastPhase')==phase:break
        assert call(u,'FastPump',1)==7
    else:raise AssertionError(('write phase not reached',phase))
    call(u,'FastDisconnect')
    for _ in range(100):
        state=call(u,'FastPump',512)
        if state in (0,8):break
        assert state!=9,('disconnect failed publication',phase)
    else:raise AssertionError('drain timeout')
    _,only_rec=create_plan(raw,4,images[4],uid=[1,2,3])
    assert equal_disk(u,only_rec),('drained write mismatch',phase)
    assert not value(u,'test_bad_write')
# Final result reports only exercised paths. Phase fault injection is extended
# by the separate physical-NOR journal tests; this is not a timing benchmark.
result=dict(passed=True,all_nine_files_equal_independent_plan=True,product_sent_once=True,
            stock_wire_bytes=0,empty_containers_generated=True,no_full_nor_hash=True,
            device_read_bytes=used,request_payload_bytes=transferred,
            disconnected_write_phases=[0,1,2,4,5],
            reseed_preserves_fat_stock_recovery=True,reseed_power_loss=reseed_boundaries,
            limits='ARM emulation; accelerated SHA/memory, not real RF timing')
(OUT/'results.json').write_text(json.dumps(result,indent=2));print(result,flush=True)
faulthandler.cancel_dump_traceback_later()
