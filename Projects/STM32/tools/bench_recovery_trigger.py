"""Exercise the existing APP recovery/stock BL using an explicit SWD intent.

Writes only the exact ELF's16-byte retained intent and debug control. It does
NOT program internal flash or NOR. This proves the device recovery path, not
a Bluetooth transport exchange or the physical-button gesture.
"""
from pathlib import Path
import argparse,json,struct,subprocess
from bringup import Board
from storage_swd_backup import LiveMemory,mailbox_symbol
from bootstrap_build import TC
from validate_image import Elf32,validate

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--elf',type=Path,required=True);p.add_argument('--serial',required=True)
    p.add_argument('--output',type=Path,required=True);p.add_argument('--read-khz',type=int,default=950)
    p.add_argument('--confirm-stock',action='store_true',help='Explicitly request the approved stock recovery, not just its confirmation screen')
    a=p.parse_args();validate(Elf32(a.elf.read_bytes()))
    listing=subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),'-S','--defined-only',str(a.elf)],text=True)
    symbols={v[3]:(int(v[0],16),int(v[1],16)) for line in listing.splitlines() if len(v:=line.split())==4}
    intents=[v for line in listing.splitlines() if len(v:=line.split())==4 and v[3]=='intent']
    if len(intents)!=1:raise RuntimeError('Ambiguous/missing retained recovery intent')
    address,size=int(intents[0][0],16),int(intents[0][1],16)
    if size!=16 or not 0x20006a00<=address<=0x20030000-16 or address%8:raise RuntimeError('Intent outside approved retained SRAM')
    if 'AppRecovery_EarlyRun' not in listing:raise RuntimeError('Missing recovery entry')
    out=a.output;out.mkdir(parents=True,exist_ok=False)
    mem=LiveMemory(a.serial,out,mailbox_symbol(a.elf),a.read_khz,120)
    app=mem.verify_app(a.elf)
    metadata=struct.unpack('<5I',mem.upload('metadata-before',0x08008000,20).read_bytes())
    if metadata[0]!=0x000e0000 or metadata[4]:raise RuntimeError('Unapproved resident or existing pending installation')
    reason=2 if a.confirm_stock else 1
    record=struct.pack('<4I',0,reason,(~reason)&0xffffffff,reason^0xd615a73b)
    (out/'intent-fields.bin').write_bytes(record)
    evidence=dict(app=app,intent_address=address,reason=reason,state='preflight',writes=['storage drain request','retained SRAM intent','debug control'],physical_buttons_tested=False,wireless_tested=False)
    (out/'result.json').write_text(json.dumps(evidence,indent=2))
    board=Board(a.serial,out,100);board.read_frequency_khz=a.read_khz
    def words(name,expected_size):
        addr,n=symbols[name]
        if n!=expected_size or not 0x20000000<=addr<=0x20030000-n:raise RuntimeError('Diagnostic layout '+name)
        path=out/(name+'.bin');board.command('precondition-'+name,'-u',hex(addr),hex(n),str(path))
        return struct.unpack('<'+'I'*(n//4),path.read_bytes())
    evidence['storage_quiesced']=mem.quiesce(a.elf,True)
    update=words('g_runtime_update',68)
    if update[8] or update[11] not in (0,6) or update[13]:raise RuntimeError('OTA owner or radio session is active')
    if not evidence['storage_quiesced']:
        # Bootstrap has no normal persistent writers. Its sole owner must have
        # completed its startup/provision work and any diagnostic transfer.
        status=words('g_bootstrap',48)
        if status[7] not in (0,8,9) or mem.descriptor()['state']==1:raise RuntimeError('Bootstrap storage not idle')
    (out/'result.json').write_text(json.dumps(evidence,indent=2))
    board.prepare()
    board.command('publish-intent','-halt','-w',str(out/'intent-fields.bin'),hex(address),'-w32',hex(address),'0x32564352')
    checked=board.dump('intent-readback',address,16,resume=False)
    if checked!=struct.pack('<I',0x32564352)+record[4:]:raise RuntimeError('Retained intent readback mismatch; no reset')
    board.command('enter-recovery-through-stock-bl','-w32','0xE0042008',hex(board.freeze_before),'-rst','-run')
    evidence['state']='requested_device_result_pending'
    (out/'result.json').write_text(json.dumps(evidence,indent=2));print(json.dumps(evidence,indent=2))
if __name__=='__main__':main()
