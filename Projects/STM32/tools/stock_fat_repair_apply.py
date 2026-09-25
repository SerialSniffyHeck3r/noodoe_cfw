"""Apply an explicitly reviewed lossless FAT repair; one SWD owner, no format.
Requires independent full backups and a matching ARM-tested after-image. Keep
the before-image, every changed sector, live readback and interrupted journal.
"""
from pathlib import Path
import argparse,json,struct,subprocess,time
from resource_install import sha,require,Fat,TC
from stock_fat_repair import batch_bytes,SAFE_END
from bringup import Board
from storage_swd_backup import LiveMemory,mailbox_symbol,read_segment
def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for name in ['backup','plan','elf','output']:ap.add_argument('--'+name,type=Path,required=True)
    ap.add_argument('--serial',required=True);ap.add_argument('--execute',action='store_true');a=ap.parse_args();require(a.execute,'Explicit execution required')
    backup=json.loads((a.backup/'manifest.json').read_text());require(backup['verified'] and backup['state']=='verified','Unverified backup')
    before=(a.backup/'A.bin').read_bytes();require(before==(a.backup/'B.bin').read_bytes(),'Independent backups differ')
    plan=json.loads((a.plan/'plan.json').read_text());after=(a.plan/'after.bin').read_bytes()
    require(len(before)==len(after)==0x8000000 and sha(before)==plan['before_sha256'] and sha(after)==plan['after_sha256'],'Plan image mismatch');Fat(after)
    addresses=plan['changed_sectors'];actual=[x for x in range(0,len(before),4096) if before[x:x+4096]!=after[x:x+4096]]
    require(sorted(addresses)==actual and len(set(addresses))==len(addresses),'Unrecorded/duplicate sector change')
    require(all(4096<=x<SAFE_END and (x<0x9000 or before[x:x+4096] in (bytes(4096),b'\xff'*4096)) for x in actual),'Nonblank data/protected extent overwrite')
    tests=json.loads((a.plan.parent/'repair-tests.json').read_text());require({x['optimization'] for x in tests if x['status']=='PASS' and x['repaired_sha256']==sha(after)}=={'Os','O0'},'Exact repair lacks both ARM tests')
    a.output.mkdir(parents=True,exist_ok=False);b=Board(a.serial,a.output,100);b.read_frequency_khz=950
    mem=LiveMemory(a.serial,a.output,mailbox_symbol(a.elf),950,240);mem.descriptor();identity=mem.verify_app(a.elf);box=mem.descriptor();mem.running()
    require([box[f'uid{i}'] for i in range(3)]==backup['identity']['uid_words'],'Wrong donor')
    nm=subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),'-S',str(a.elf)],text=True);symbols={w[3]:(int(w[0],16),int(w[1],16)) for l in nm.splitlines() if len(w:=l.split())==4}
    address,size=symbols['g_resource_install'];require(size==60 and 0x20000000<=address<=0x20030000-size,'Recovery mailbox outside SRAM')
    result={'schema':1,'state':'preflight','identity':identity,'backup':str(a.backup.resolve()),'plan':str(a.plan.resolve()),'before_sha256':sha(before),'after_sha256':sha(after),'batches':[],'readback':[],'nor_only':True};counter=0
    def save():(a.output/'result.json').write_text(json.dumps(result,indent=2))
    def readbox():
        nonlocal counter
        counter+=1;p=a.output/f'repair-status-{counter:04d}.bin';b.command(p.stem,'-u',hex(address),hex(size),str(p));w=struct.unpack('<15I',p.read_bytes());require(w[:2]==(0x52534931,1),'Mailbox ABI');return w
    save()
    for i in range(0,len(addresses),120):
        w=readbox();require(w[9]==w[10] and w[11]!=4 and w[3]==524288 and 0xc0010000<=w[2]<=0xc4000000-w[3],'Busy/invalid recovery buffer')
        payload=batch_bytes(before,after,addresses[i:i+120]);p=a.output/f'batch-{i//120}.bin';p.write_bytes(payload)
        b.command(f'payload-{i//120}','-w',str(p),hex(w[2]));require(mem.upload('repair-input',w[2],len(payload)).read_bytes()==payload,'Input readback mismatch')
        seq=(w[9]+1)&0xffffffff or 1;result['state']='writing';record={'index':i//120,'sequence':seq,'addresses':addresses[i:i+120],'payload_sha256':sha(payload),'state':'submitted'};result['batches'].append(record);save()
        b.command(f'publish-{i//120}','-w32',hex(address+16),'3','-w32',hex(address+32),'0x42414B32','-w32',hex(address+36),hex(seq))
        deadline=time.monotonic()+240
        while True:
            w=readbox()
            if w[10]==seq:break
            require(time.monotonic()<deadline,'Recovery deadline; preserve journal and inspect, never retry blindly');time.sleep(.2)
        record.update(state='complete' if w[12]==0 else 'failed',error=w[12],progress=w[13]);save();require(w[12]==0,'Device repair error '+str(w[12]))
    result['state']='readback';save()
    # Include unchanged gaps in the readback, not just the bytes we intended to
    # change. Group nearby sectors for bounded, CRC-verified storage uploads.
    regions=[]
    for x in actual:
        if regions and x-regions[-1][1]<=131072 and x+4096-regions[-1][0]<=8388608:regions[-1][1]=x+4096
        else:regions.append([x,x+4096])
    seq=mem.descriptor()['last_request_seq']
    for start,end in regions:
        seq+=1;p,detail=read_segment(mem,start,end-start,seq,120,3);data=p.read_bytes();require(data==after[start:end],'Post-repair difference')
        result['readback'].append({'offset':start,'length':end-start,'path':str(p.resolve()),'sha256':sha(data),'transport':detail});save()
    result['state']='verified';save();print('PASS: reviewed repair, entire affected regions read back, protected extents untouched',flush=True)
if __name__=='__main__':main()
