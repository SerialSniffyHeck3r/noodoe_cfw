"""Bootstrap-only NDCP backup/create client; no device access unless --port.

Keeps the same authenticated RFCOMM session from independent128MiB A/B backup
through bounded creates and final A/B readback. No erase, arbitrary raw write,
format, or replacement command exists. A failed initial FAT publication is not
automatically retried. All before/after sectors are retained before COMMIT.
"""
from pathlib import Path
import argparse, hashlib, json, struct, time, zlib
from resource_install import Fat, require, swapped, sha, check_slot
from cfw_storage_install import make_plan as config_plan, file_bytes
from noodoe_control import Client

NAMES=('NOODOE.RSC','CFWCFG.DAT','CFWRIDE.DAT','CFWPIC.DAT','CFWREC.DAT','CFWA.DAT','CFWB.DAT','CFWBOOT.DAT','CFWLOG.DAT')
SHORT=(b'NOODOE  RSC',b'CFWCFG  DAT',b'CFWRIDE DAT',b'CFWPIC  DAT',b'CFWREC  DAT',b'CFWA    DAT',b'CFWB    DAT',b'CFWBOOT DAT',b'CFWLOG  DAT')
SIZES=(1048576,131072,262144,1048576,524288,524288,524288,65536,262144)
STOCK_SHA='162faeecc64bf56d828570168785e66d1a7da0152ef912bf83f9a19094d91edf'
BOOT_SHA='f8b379c3fac078a8e01d8b6c36fccc5bb5ea5852a0db008822ef6871e0f38df5'
SAFE_END=0x7f70000

def recovery_image(app,uid):
    require(len(app)==0x70000 and sha(app)==STOCK_SHA,'Exact approved V5.16 APP required')
    require(len(uid)==3 and all(0<=i<=0xffffffff for i in uid),'UID words')
    b=bytearray(b'\xff'*524288)
    struct.pack_into('<10I',b,0,0x3152434e,1,4096,524288,*uid,0x00100005,0x70000,0x000e0000)
    b[40:72]=bytes.fromhex(STOCK_SHA)
    struct.pack_into('<2I',b,4088,zlib.crc32(b[:4088]),0x31544d43)
    b[4096:4096+len(app)]=app
    return bytes(b)

def create_plan(raw,kind,image,uid=None):
    require(0<=kind<len(NAMES) and len(image)==SIZES[kind],'Whitelisted file/size')
    if kind>=5:
        from gate_containers import check_container
        require(uid is not None,'Gate plan requires independently observed target UID')
        check_container(image,kind,uid)
    fs=Fat(raw);require(NAMES[kind] not in fs.files,'Existing filename; create-only')
    require(all(fs.fat(c) in (0,0xff7) or c in fs.refs for c in range(2,4080)),'Orphan allocation')
    old={name:sha(file_bytes(fs,name)) for name,(_,n) in fs.files.items() if n}
    count=len(image)//32768
    first=next((c for c in range(2,4080-count+1) if fs.address(c)+len(image)<=SAFE_END and all(fs.fat(k)==0 and k not in fs.refs for k in range(c,c+count))),None)
    require(first is not None,'No safe contiguous free extent')
    root=next((i for i in range(512) if fs.b[0x5000+i*32] in (0,0xe5)),None)
    require(root is not None,'Root full')
    for i in range(count):fs.setfat(first+i,0xfff if i+1==count else first+i+1)
    e=bytearray(32);e[:11]=SHORT[kind];e[11]=0x20;struct.pack_into('<H',e,26,first);struct.pack_into('<I',e,28,len(image))
    fs.b[0x5000+root*32:0x5000+(root+1)*32]=e
    address=fs.address(first);fs.b[address:address+len(image)]=image
    after=bytes(swapped(fs.b));checked=Fat(after)
    require(all(sha(file_bytes(checked,n))==h for n,h in old.items()),'Original file changed')
    require(after[SAFE_END:]==raw[SAFE_END:],'Reserved tail changed')
    return dict(kind=kind,name=NAMES[kind],first_cluster=first,root_index=root,address=address,bytes=len(image),
                source_sha256=sha(image),before_sha256=sha(raw),after_sha256=sha(after),original_files=old),after

class BootstrapClient:
    def __init__(self,client):self.client=client
    def request(self,op,payload=b''):
        p=self.client.request(op,payload) # Client strips the result word.
        require(len(p)>=28,'Truncated bootstrap reply')
        state,error,pos,address,n,backup_pos,valid=struct.unpack_from('<7I',p)
        return dict(state=state,error=error,position=pos,address=address,bytes=n,backup_position=backup_pos,backup_valid=valid),p[28:]
    def backup(self,out,identity):
        out.mkdir(parents=True,exist_ok=False)
        for index,name in enumerate(('A.bin','B.bin')):
            self.request(0x50,struct.pack('<I',index));digest=hashlib.sha256();offset=0
            with (out/name).open('xb') as f:
                while offset<0x8000000:
                    n=min(480,0x8000000-offset);status,data=self.request(0x51,struct.pack('<2I',offset,n))
                    require(len(data)==n and status['backup_position']==offset+n,'Backup progression')
                    f.write(data);digest.update(data);offset+=n
                    if offset%(480*4096)==0:print(name,offset,'/134217728',flush=True)
                f.flush()
            (out/(name+'.sha256')).write_text(digest.hexdigest())
        a=(out/'A.bin').read_bytes();b=(out/'B.bin').read_bytes();status,device_hash=self.request(0x56)
        require(a==b and len(a)==0x8000000 and status['backup_valid'] and bytes.fromhex(sha(a))==device_hash,'Independent backup disagreement')
        manifest=dict(schema=1,state='verified',verified=True,identity=identity,transport='Bootstrap NDCP sequential two-pass SHA',
            acquisition='after initial Bootstrap OTA; not pristine pre-install MCU/NOR clone',
            captures=[dict(path=str((out/name).resolve()),address=0,length=len(a),sha256=sha(a),terminal_done_validated=True) for name in ('A.bin','B.bin')],
            comparison=dict(byte_identical=True,sha256_a=sha(a),sha256_b=sha(a)))
        (out/'manifest.json').write_text(json.dumps(manifest,indent=2))
        return a,device_hash
    def install(self,raw,backup_hash,kind,image,out):
        plan,after=create_plan(raw,kind,image);out.mkdir(parents=True,exist_ok=False)
        (out/'plan.json').write_text(json.dumps(plan,indent=2));(out/'payload.bin').write_bytes(image)
        for off in list(range(0x1000,0x9000,4096))+list(range(plan['address'],plan['address']+len(image),4096)):
            if raw[off:off+4096]!=after[off:off+4096]:
                (out/f'{off:08x}-before.bin').write_bytes(raw[off:off+4096]);(out/f'{off:08x}-after.bin').write_bytes(after[off:off+4096])
        digest=bytes.fromhex(plan['source_sha256']);self.request(0x52,struct.pack('<2I',kind,len(image))+digest+backup_hash)
        for off in range(0,len(image),480):self.request(0x53,struct.pack('<I',off)+image[off:off+480])
        self.request(0x54);deadline=time.monotonic()+180
        while True:
            status,_=self.request(0x56)
            require(status['state']!=9 and not status['error'],'Preparation failed')
            if status['state']==6:break
            require(time.monotonic()<deadline,'Preparation timed out');time.sleep(.02)
        require(status['address']==plan['address'] and status['bytes']==len(image),'Device allocation differs from audited plan')
        metadata=bytearray()
        for off in range(0,0x9000,480):
            _,data=self.request(0x56,struct.pack('<I',off));metadata.extend(data)
        require(bytes(metadata)==bytes(swapped(after[:0x9000])),'Device metadata differs from plan')
        (out/'prepared-metadata.bin').write_bytes(metadata)
        self.request(0x55,digest);deadline=time.monotonic()+600
        while True:
            status,_=self.request(0x56);(out/'status.json').write_text(json.dumps(status,indent=2))
            require(status['state']!=9 and not status['error'],'Installation failed; inspect sector journal, do not retry')
            if status['state']==8:break
            require(time.monotonic()<deadline,'Installation timed out; inspect sector journal');time.sleep(.02)
        return after

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--port',required=True);p.add_argument('--stock-app',type=Path,required=True)
    p.add_argument('--resources',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--bootstrap-sha256',required=True,help='SHA256 of the approved complete448KiB installed Bootstrap APP')
    args=p.parse_args();app=args.stock_app.read_bytes();require(len(app)==0x70000 and sha(app)==STOCK_SHA,'Unapproved stock image')
    rsc=args.resources.read_bytes();require(len(rsc)==1048576,'Resource size');check_slot(rsc[:524288])
    import serial
    args.output.mkdir(parents=True,exist_ok=False)
    with serial.Serial(args.port,115200,timeout=.2,write_timeout=15) as port:
        basic=Client(port);identity=basic.info();client=BootstrapClient(basic)
        require('bootstrap' in identity['build'].lower(),'Refusing non-Bootstrap APP')
        require(identity['boot_metadata_words'][0]==0x000e0000 and identity['boot_metadata_words'][4]==0,'BL/pending mismatch')
        attestation=basic.request(0x58);require(len(attestation)==84,'Bootstrap identity layout')
        version,role,*uid=struct.unpack_from('<5I',attestation)
        require(version==1 and role==1 and uid==identity['uid_words'],'Bootstrap identity/UID mismatch')
        require(attestation[20:52].hex()==args.bootstrap_sha256.lower() and attestation[52:84].hex()==BOOT_SHA,'Installed Bootstrap/immutableBL hash mismatch')
        identity['app_sha256']=attestation[20:52].hex();identity['immutable_boot_sha256']=attestation[52:84].hex()
        (args.output/'identity.json').write_text(json.dumps(identity,indent=2))
        raw,backup_hash=client.backup(args.output/'before',identity)
        initial=raw
        # Produce all target-bound initial records from untouched original files.
        _,_,all_cfg=config_plan(raw,identity['uid_words'])
        images=[recovery_image(app,identity['uid_words']),rsc,all_cfg[:131072],all_cfg[131072:393216],all_cfg[393216:]]
        for kind,image in zip((4,0,1,2,3),images):raw=client.install(raw,backup_hash,kind,image,args.output/NAMES[kind])
        observed,_=client.backup(args.output/'after',identity);require(observed==raw,'Whole NOR differs from expected complete image')
        (args.output/'result.json').write_text(json.dumps(dict(state='verified',identity=identity,before_sha256=sha(initial),after_sha256=sha(raw)),indent=2))
        client.request(0x57)

if __name__=='__main__':main()
