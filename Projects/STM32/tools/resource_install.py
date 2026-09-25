"""Audited create-only FAT12 resource provisioning / inactive-slot updates.

Plan is offline. Execute requires two byte-identical current full NOR backups,
verified APP identity, live readback of every precondition region, and a device
mailbox which independently checks geometry, chain, slot SHA and commit-last.
No format, unlink, file resize, bootloader, NVM or update staging writes.
"""
from pathlib import Path
import argparse,hashlib,json,struct,subprocess,time,zlib
from resource_pack import SLOT,HEADER,MAGIC,COMMIT
P=Path(__file__).resolve().parents[1]
TC=Path('C:/ST/STM32CubeIDE_1.18.1/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344/tools/bin')
def require(ok,message):
    if not ok:raise ValueError(message)
def sha(data):return hashlib.sha256(data).hexdigest()
def swapped(data):
    require(len(data)%2==0,'Pair alignment');out=bytearray(data);out[0::2]=data[1::2];out[1::2]=data[0::2];return out
def check_slot(data,required=None):
    require(len(data)==SLOT,'Slot size')
    magic,version,total,count=struct.unpack_from('<4I',data)
    require(magic==MAGIC and version==1 and 1<=count<=(4088-48)//16 and 0<total<=SLOT-HEADER,'Slot header')
    require(struct.unpack_from('<I',data,4092)[0]==COMMIT and struct.unpack_from('<I',data,4088)[0]==zlib.crc32(data[:4088]),'Incomplete/corrupt header')
    digest=hashlib.sha256(data[48:48+count*16]+data[HEADER:HEADER+total]).digest()
    require(digest==data[16:48] and (required is None or digest==required),'Resource ID/SHA')
    end=0
    for i in range(count):
        ident,off,n,crc=struct.unpack_from('<4I',data,48+i*16)
        require(ident==i+1 and off>=end and off%4==0 and n>0 and off+n<=total,'Entry bounds')
        require(zlib.crc32(data[HEADER+off:HEADER+off+n])==crc,'Entry CRC');end=off+n
    require(end==total,'Trailing data');return digest
class Fat:
    def __init__(self,raw):
        require(len(raw)==0x8000000,'Full NOR required');self.b=swapped(raw)
        b=self.b
        require(struct.unpack_from('<H',b,11)[0]==4096 and b[13]==8 and struct.unpack_from('<H',b,14)[0]==1 and b[16]==2 and struct.unpack_from('<H',b,17)[0]==512 and struct.unpack_from('<H',b,22)[0]==2 and struct.unpack_from('<I',b,32)[0]==0x7f80,'Unsupported FAT geometry')
        require(b[0x1000:0x3000]==b[0x3000:0x5000],'FAT copies disagree')
        require(b[510:512]==b'\x55\xaa' and self.fat(0)==(0xF00|b[21]) and self.fat(1)>=0xFF8,'Invalid FAT reserved entries')
        self.refs={};self.files={};self.walk(b[0x5000:0x9000],'')
    def fat(self,c):
        value=struct.unpack_from('<H',self.b,0x1000+c+c//2)[0];return value>>4 if c&1 else value&0xfff
    def address(self,c):return 0x9000+(c-2)*32768
    def chain(self,c,name):
        result=[]
        while c<0xff8:
            require(2<=c<4080 and c not in self.refs,'Invalid/free/cross-linked FAT chain: '+name)
            self.refs[c]=name;result.append(c);c=self.fat(c)
        return result
    def walk(self,data,path):
        for off in range(0,len(data),32):
            e=data[off:off+32]
            if e[0]==0:break
            if e[0]==0xe5 or e[11]==0xf or e[11]&8 or e[0]==46:continue
            name=e[:8].decode('ascii',errors='replace').rstrip();ext=e[8:11].decode('ascii',errors='replace').rstrip()
            name=path+name+('.'+ext if ext else '')
            require(name not in self.files,'Duplicate directory name')
            cluster=struct.unpack_from('<H',e,26)[0];n=struct.unpack_from('<I',e,28)[0]
            require(struct.unpack_from('<H',e,20)[0]==0,'FAT12 high cluster')
            chain=self.chain(cluster,name) if cluster else []
            require((e[11]&16 and chain) or n<=len(chain)*32768,'Truncated file chain')
            self.files[name]=(chain,n)
            if e[11]&16:self.walk(b''.join(self.b[self.address(c):self.address(c)+32768] for c in chain),name+'/')
    def setfat(self,c,n):
        for base in (0x1000,0x3000):
            off=base+c+c//2;old=struct.unpack_from('<H',self.b,off)[0]
            struct.pack_into('<H',self.b,off,(old&15)|(n<<4) if c&1 else (old&0xf000)|n)
def make_plan(raw,prepared,command=1,slot=0):
    digest=check_slot(prepared);fs=Fat(raw);entry=None
    require(command in (1,2) and slot in (0,1),'Command/slot')
    if command==1:
        require('NOODOE.RSC' not in fs.files and slot==0,'Container exists or initial slot is not A')
        first=next((c for c in range(2,4047) if fs.address(c)+2*SLOT<=0x7f70000 and all(fs.fat(x)==0 and x not in fs.refs for x in range(c,c+32))),None)
        require(first is not None,'No safe contiguous free cluster range')
        entry=next((i for i in range(512) if fs.b[0x5000+i*32] in (0,0xe5)),None);require(entry is not None,'Root full')
        for i in range(32):fs.setfat(first+i,0xfff if i==31 else first+i+1)
        e=bytearray(32);e[:11]=b'NOODOE  RSC';e[11]=0x20;struct.pack_into('<H',e,26,first);struct.pack_into('<I',e,28,2*SLOT)
        fs.b[0x5000+entry*32:0x5000+(entry+1)*32]=e
        fs.b[fs.address(first):fs.address(first)+2*SLOT]=prepared+b'\xff'*SLOT
    else:
        require('NOODOE.RSC' in fs.files,'Container absent');chain,n=fs.files['NOODOE.RSC'];first=chain[0]
        require(n==2*SLOT and chain==list(range(first,first+32)) and fs.address(first)+2*SLOT<=0x7f70000,'Invalid/reserved container')
        start=fs.address(first);check_slot(fs.b[start+(1-slot)*SLOT:start+(2-slot)*SLOT])
        fs.b[start+slot*SLOT:start+(slot+1)*SLOT]=prepared;entry=0
    after=bytes(swapped(fs.b));Fat(after) # Verify complete graph after proposed creation.
    # Entire metadata and container are preconditions, even unchanged sectors.
    regions=[dict(offset=0,length=0x9000),dict(offset=fs.address(first),length=2*SLOT)]
    for r in regions:
        a,n=r['offset'],r['length'];r.update(before_sha256=sha(raw[a:a+n]),after_sha256=sha(after[a:a+n]))
    return dict(schema=1,command=command,slot=slot,first_cluster=first,root_index=entry,backup_sha256=sha(raw),slot_sha256=sha(prepared),asset_id=digest.hex(),regions=regions),after
def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('action',choices=['plan','execute'])
    ap.add_argument('--backup',type=Path,required=True);ap.add_argument('--slot-file',type=Path,default=P/'Resources/slot.bin');ap.add_argument('--output',type=Path,required=True)
    ap.add_argument('--command',type=int,choices=[1,2],default=1);ap.add_argument('--slot',type=int,choices=[0,1],default=0);ap.add_argument('--serial');ap.add_argument('--elf',type=Path);ap.add_argument('--read-khz',type=int,default=950);ap.add_argument('--repair-journal',type=Path)
    a=ap.parse_args();manifest=json.loads((a.backup/'manifest.json').read_text());require(manifest['verified'] and manifest['state']=='verified','Unverified backup')
    raw=(a.backup/'A.bin').read_bytes();require(raw==(a.backup/'B.bin').read_bytes(),'Independent backups differ')
    provenance={'independent_backup_sha256':sha(raw)}
    if a.repair_journal:
        journal=json.loads((a.repair_journal/'result.json').read_text())
        require(journal['state']=='verified' and journal['before_sha256']==sha(raw),'Unverified/mismatched repair journal')
        repaired=(Path(journal['plan'])/'after.bin').read_bytes()
        require(len(repaired)==len(raw) and sha(repaired)==journal['after_sha256'],'Repair image changed')
        ranges=[]
        for item in journal['readback']:
            data=Path(item['path']).read_bytes();off=item['offset'];n=item['length']
            require(len(data)==n and sha(data)==item['sha256'] and data==repaired[off:off+n],'Repair readback changed')
            ranges.append((off,off+n))
        changed=[off for off in range(0,len(raw),4096) if raw[off:off+4096]!=repaired[off:off+4096]]
        require(all(4096<=off<0x7f70000 and any(lo<=off and off+4096<=hi for lo,hi in ranges) for off in changed),'Unverified repair sector')
        raw=repaired;provenance['verified_repair_journal']=str(a.repair_journal.resolve())
    prepared=a.slot_file.read_bytes();plan,after=make_plan(raw,prepared,a.command,a.slot)
    plan['source_provenance']=provenance
    a.output.mkdir(parents=True,exist_ok=False);(a.output/'plan.json').write_text(json.dumps(plan,indent=2))
    for i,r in enumerate(plan['regions']):
        off,n=r['offset'],r['length'];(a.output/f'{i}-before.bin').write_bytes(raw[off:off+n]);(a.output/f'{i}-after.bin').write_bytes(after[off:off+n])
    if a.action=='plan':print(json.dumps(plan,indent=2));return
    require(a.serial and a.elf,'Explicit target and ELF required')
    from bringup import Board
    from storage_swd_backup import LiveMemory,mailbox_symbol,read_segment
    b=Board(a.serial,a.output,100);b.read_frequency_khz=a.read_khz
    mem=LiveMemory(a.serial,a.output,mailbox_symbol(a.elf),a.read_khz,240);mem.descriptor();identity=mem.verify_app(a.elf)
    box=mem.descriptor();require([box[f'uid{i}'] for i in range(3)]==manifest['identity']['uid_words'],'Wrong donor')
    seq=box['last_request_seq'];evidence=dict(identity=identity,state='preflight',before=[],after=[])
    def save(): (a.output/'result.json').write_text(json.dumps(evidence,indent=2))
    save()
    for r in plan['regions']:
        seq+=1;path,detail=read_segment(mem,r['offset'],r['length'],seq,120,3)
        require(sha(path.read_bytes())==r['before_sha256'],'Live FAT/container differs from backup; no write');evidence['before'].append(detail);save()
    nm=subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),'-S',str(a.elf)],text=True)
    symbols={w[3]:(int(w[0],16),int(w[1],16)) for l in nm.splitlines() if len(w:=l.split())==4}
    address,size=symbols['g_resource_install'];require(size==60 and 0x20000000<=address<=0x20030000-size,'Mailbox ABI')
    counter=0
    def readbox():
        nonlocal counter
        counter+=1;path=a.output/f'install-status-{counter:04d}.bin';b.command(path.stem,'-u',hex(address),hex(size),str(path))
        w=struct.unpack('<15I',path.read_bytes());require(w[:2]==(0x52534931,1),'Mailbox identity');return w
    w=readbox();require(w[9]==w[10] and w[11]!=4 and w[3]==SLOT and 0xc0010000<=w[2]<=0xc4000000-SLOT,'Busy/invalid buffer')
    source=a.output/'prepared.bin';source.write_bytes(prepared);b.command('resource-payload','-w',str(source),hex(w[2]))
    require(mem.upload('resource-input-readback',w[2],SLOT).read_bytes()==prepared,'Input readback mismatch')
    request=(w[9]+1)&0xffffffff or 1
    fields=[a.command,plan['first_cluster'],plan['root_index'],a.slot,0x42414b32]
    args=['-w32',hex(address+36),'0']
    for i,v in enumerate(fields):args+=['-w32',hex(address+16+4*i),hex(v)]
    args+=['-w32',hex(address+36),hex(request)];b.command('resource-install-request',*args)
    evidence['state']='installing';save();deadline=time.monotonic()+240
    while True:
        w=readbox()
        if w[10]==request:break
        require(time.monotonic()<deadline,'Install deadline; inspect before retry');time.sleep(.1)
    require(w[12]==0,f'Device rejected/failed: {w[12]}');evidence['state']='readback';save()
    for r in plan['regions']:
        seq+=1;path,detail=read_segment(mem,r['offset'],r['length'],seq,120,3)
        require(sha(path.read_bytes())==r['after_sha256'],'Unexpected NOR difference');evidence['after'].append(detail);save()
    evidence['state']='verified';save();print('PASS: exact FAT/container diff and slot commit verified; explicit reboot required')
if __name__=='__main__':main()
