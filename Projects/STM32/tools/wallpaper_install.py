"""Create-only WALLn.JPG provisioner. Exact ARM-emulated write plan, current
reconstructed NOR provenance and live preimages required. Existing photos and
resource container are preserved; apply by explicit reboot after readback.
"""
from pathlib import Path
import argparse,hashlib,json,struct,subprocess,time
from bringup import Board,require
from resource_install import Fat
from storage_swd_backup import LiveMemory,mailbox_symbol,read_segment
P=Path(__file__).resolve().parents[1]
TC=Path('C:/ST/STM32CubeIDE_1.18.1/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344/tools/bin')
sha=lambda d:hashlib.sha256(d).hexdigest()
def main():
 ap=argparse.ArgumentParser(description=__doc__)
 ap.add_argument('--serial',required=True);ap.add_argument('--elf',type=Path,required=True)
 ap.add_argument('--plan',type=Path,required=True);ap.add_argument('--backup',type=Path,required=True)
 ap.add_argument('--output',type=Path,required=True);ap.add_argument('--execute',action='store_true')
 ap.add_argument('--read-frequency-khz',type=int,choices=[100,400,950],default=100,help='Qualify against known bytes before increasing')
 a=ap.parse_args();require(a.execute,'Explicit execution required')
 plan=json.loads(a.plan.read_text());data=a.backup.read_bytes()
 require(plan['schema']==2 and len(data)==0x8000000 and sha(data)==plan['backup_sha256'],'Current baseline/plan mismatch')
 provenance=plan['source_provenance'];require(provenance['sha256']==sha(data) and provenance['not_fresh_independent_dump'],'Missing reconstruction provenance')
 base=Path(provenance['base']);manifest=json.loads((base/'manifest.json').read_text())
 require(manifest['verified'] and manifest['state']=='verified','Independent backup unavailable')
 require((base/'A.bin').read_bytes()==(base/'B.bin').read_bytes(),'Independent backup mismatch')
 for key in ('repair','resources'):require(json.loads(Path(provenance[key]).read_text())['state']=='verified','Prior journal not verified')
 Fat(data)
 a.output.mkdir(parents=True,exist_ok=False);b=Board(a.serial,a.output,100);b.read_frequency_khz=a.read_frequency_khz
 mem=LiveMemory(a.serial,a.output,mailbox_symbol(a.elf),a.read_frequency_khz,240)
 mem.descriptor();identity=mem.verify_app(a.elf);box=mem.descriptor()
 require([box['uid0'],box['uid1'],box['uid2']]==plan['uid'],'Wrong donor')
 nm=subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),'-S',str(a.elf)],text=True)
 syms={q[3]:(int(q[0],16),int(q[1],16)) for l in nm.splitlines() if len(q:=l.split())==4}
 power_address,power_size=syms['g_power_ui'];address,size=syms['g_photo_import']
 require(power_size==104 and size==44,'Photo/power diagnostic ABI')
 require(all(0x20000000<=a<=0x20030000-n and not a%4 for a,n in ((power_address,power_size),(address,size))),'Diagnostics outside main SRAM')
 diagnostic_counter=0
 def read_diagnostic(label,where,count):
  # Storage backup transport deliberately cannot read arbitrary app RAM. Bind
  # these two extra read-only ranges to the independently verified current ELF.
  nonlocal diagnostic_counter
  require(mem.app_verified and (where,count) in ((power_address,12),(address,44)),'Unbound diagnostic read')
  diagnostic_counter+=1;path=a.output/f'diagnostic-{diagnostic_counter:04d}-{label}.bin'
  require(not path.exists(),'Refusing stale diagnostic bytes')
  b.command(path.stem,'-u',hex(where),hex(count),str(path))
  data=path.read_bytes();require(len(data)==count,'Incomplete diagnostic read');return data
 power=struct.unpack('<3I',read_diagnostic('power-state',power_address,12));require(power[0]==0x50554947 and power[1]==3 and power[2]==2,'Valid IGN ON required for photo import')
 seq=box['last_request_seq'];evidence={'identity':identity,'reconstructed_baseline':str(a.backup),'source_provenance':provenance,'before':[],'imports':[],'after':[],'state':'checking'}
 def save(): (a.output/'result.json').write_text(json.dumps(evidence,indent=2))
 save()
 for i,r in enumerate(plan['regions']):
  seq=(seq+1)&0xffffffff or 1
  path,detail=read_segment(mem,r['offset'],r['length'],seq,60,3)
  require(sha(path.read_bytes())==r['before_sha256'],'Live FAT/data differs from reviewed plan; import cancelled')
  evidence['before'].append(detail);save()
 evidence['state']='backed_up';save()
 def mailbox(label):
  data=read_diagnostic(label,address,44);w=struct.unpack('<11I',data)
  require(w[0]==0x50494D31 and w[1]==2,'Photo mailbox changed');return w
 import zlib
 for item in plan['images']:
  slot=item['slot'];require(slot in (0x100,0x101,0x102),'Only create-only CFW wallpaper overrides')
  jpeg=Path(item['path']).read_bytes();require(sha(jpeg)==item['sha256'],'Source image changed')
  w=mailbox('photo-before');require(w[8]==w[10] and w[2] and 0<len(jpeg)<=w[3]<=131072,'Photo worker is not idle')
  buffer=w[2];require(0xC0010000<=buffer<buffer+len(jpeg)<=0xC4000000,'Invalid SDRAM input')
  # CLI accepts BIN; bytes remain the exact original JPEG.
  source=a.output/f'photo-{slot}.bin';source.write_bytes(jpeg)
  b.command(f'photo-{slot}-payload','-w',str(source),hex(buffer))
  payload=mem.upload(f'photo-{slot}-readback',buffer,len(jpeg)).read_bytes()
  require(payload==jpeg,'SDRAM upload mismatch; no import committed')
  command=(w[8]+1)&0xffffffff or 1
  b.command(f'photo-{slot}-request','-w32',hex(address+16),hex(slot),'-w32',hex(address+20),hex(len(jpeg)),
   '-w32',hex(address+24),hex(zlib.crc32(jpeg)),'-w32',hex(address+28),'0x42414B32','-w32',hex(address+32),hex(command))
  deadline=time.monotonic()+120
  while True:
   w=mailbox('photo-progress')
   if w[10]==command:break
   require(time.monotonic()<deadline,'Import timed out; inspect state before any retry');time.sleep(.2)
  require(w[9]==0,f'Photo import failed: {w[9]:#x}')
  evidence['imports'].append({'slot':slot,'sha256':sha(jpeg),'bytes':len(jpeg),'ack':command});save()
 for i,r in enumerate(plan['regions']):
  seq=(seq+1)&0xffffffff or 1
  path,detail=read_segment(mem,r['offset'],r['length'],seq,60,3)
  require(sha(path.read_bytes())==r['after_sha256'],'Post-import NOR differs from actual ARM plan')
  evidence['after'].append(detail);save()
 evidence['state']='verified';save();print('PASS: immutable wallpaper installed/read back; reboot required for activation',flush=True)
if __name__=='__main__':main()
