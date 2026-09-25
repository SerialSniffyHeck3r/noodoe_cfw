"""Fill only an empty resource B slot through the bounded Bootstrap owner.

Requires a full physical A/B backup, exact live ELF/UID, live FAT/container
preimages and device PREPARE agreement. No FAT, old A or APP modification.
"""
from pathlib import Path
import argparse,json,struct,time
from resource_install import make_plan,Fat,check_slot,require,sha,swapped
from cfw_storage_install import file_bytes
from storage_backup import verify_unlock_manifest
from bootstrap_recovery_install import symbols,decode

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('action',choices=('plan','execute'))
 for name in ('backup','slot-file','output'):p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--elf',type=Path);p.add_argument('--serial');p.add_argument('--read-khz',type=int,default=950);a=p.parse_args()
 m=json.loads((a.backup/'manifest.json').read_text());proof=verify_unlock_manifest(a.backup/'manifest.json',m['identity']);uid=m['identity']['uid_words']
 raw=Path(m['captures'][0]['path']).read_bytes();require(sha(raw)==proof['sha256_a'],'Backup changed')
 old=file_bytes(Fat(raw),'NOODOE.RSC');check_slot(old[:0x80000]);require(old[0x80000:]==b'\xff'*0x80000,'Only empty B may be filled')
 payload=a.slot_file.read_bytes();plan,after=make_plan(raw,payload,2,1);plan['source_provenance']=dict(independent_backup_sha256=sha(raw))
 out=a.output;out.mkdir(parents=True,exist_ok=False);(out/'prepared.bin').write_bytes(payload);(out/'plan.json').write_text(json.dumps(plan,indent=2))
 for i,r in enumerate(plan['regions']):
  off,n=r['offset'],r['length'];(out/f'{i}-before.bin').write_bytes(raw[off:off+n]);(out/f'{i}-after.bin').write_bytes(after[off:off+n])
 if a.action=='plan':print(json.dumps(plan,indent=2));return
 require(a.elf and a.serial,'Exact live Bootstrap ELF and probe required')
 from storage_swd_backup import LiveMemory,mailbox_symbol,identity,read_segment
 from bringup import Board
 mem=LiveMemory(a.serial,out,mailbox_symbol(a.elf),a.read_khz,240);app=mem.verify_app(a.elf);target=identity(mem)
 require(target['uid_words']==uid,'Wrong target');verify_unlock_manifest(a.backup/'manifest.json',target)
 sym=symbols(a.elf);require('BootstrapResources_Write'in sym,'Bootstrap has no bounded resource writer')
 address,n=sym['g_bootstrap_storage_bench'];require(n==128 and 0x20000000<=address<=0x20030000-128,'Mailbox bounds')
 board=Board(a.serial,out,50);board.read_frequency_khz=a.read_khz;counter=0
 evidence=dict(state='preflight',identity=app,target=target,before=[],after=[],proof=proof)
 def save():(out/'result.json').write_text(json.dumps(evidence,indent=2))
 def box():
  nonlocal counter
  counter+=1;f=out/f'resource-mailbox-{counter:04d}.bin';board.command(f.stem,'-u',hex(address),'128',str(f));return decode(f.read_bytes())
 save();b=box();require(b['sequence']==b['ack']and b['state']in(0,8,9),'Bootstrap storage busy')
 d=mem.descriptor();seq=d['last_request_seq'];request=b['sequence']
 require(b['buffer']+len(payload)<=d['buffer_address'] or d['buffer_address']+d['buffer_capacity']<=b['buffer'],'Source overlaps read buffer')
 for r in plan['regions']:
  seq+=1;f,detail=read_segment(mem,r['offset'],r['length'],seq,120,3);require(sha(f.read_bytes())==r['before_sha256'],'Live resource/FAT differs; no write');evidence['before'].append(detail);save()
 board.command('resource-upload','-w',str(out/'prepared.bin'),hex(b['buffer']))
 require(mem.upload('resource-upload-check',b['buffer'],len(payload)).read_bytes()==payload,'SDRAM input mismatch')
 fields=struct.pack('<5I',8,0x42414b32,*uid)+bytes.fromhex(proof['sha256_a'])+bytes.fromhex(sha(payload));(out/'fields.bin').write_bytes(fields)
 board.command('resource-fields','-w',str(out/'fields.bin'),hex(address+16))
 def command(cmd):
  nonlocal request
  request=(request+1)&0xffffffff or 1
  board.command(f'resource-command-{request}','-w32',hex(address+16),hex(cmd),'-w32',hex(address+100),hex(request));deadline=time.monotonic()+600
  while True:
   b=box()
   if b['ack']==request:require(b['error']==0,'Resource command failed: '+repr(b));return b
   require(time.monotonic()<deadline,'Resource timeout; do not retry automatically');time.sleep(.2)
 evidence['state']='preparing';save();b=command(8)
 require(b['state']==6 and b['bytes']==0x80000 and b['first']==plan['regions'][1]['offset']+0x80000,'Device B extent differs')
 require(mem.upload('resource-prepared-metadata',b['metadata'],0x9000).read_bytes()==bytes(swapped(raw[:0x9000])),'FAT changed in prepare; commit withheld')
 evidence['state']='writing';save();require(command(2)['state']==8,'Resource not committed')
 evidence['state']='readback';save()
 for r in plan['regions']:
  seq+=1;f,detail=read_segment(mem,r['offset'],r['length'],seq,120,3);require(sha(f.read_bytes())==r['after_sha256'],'Physical resource readback differs');evidence['after'].append(detail);save()
 evidence.update(state='verified',global_verify_pending=True,expected_whole_nor_sha256=sha(after),fat_unchanged=True,old_resource_a_unchanged=True);save();print('Resource B verified; whole-NOR batch verification still pending.',flush=True)
if __name__=='__main__':main()
