"""Read named recovery diagnostics using the exact installed ELF; no mutations.

Snapshots are live diagnostic samples, not atomic transaction/readback proof.
Every invocation first compares the running APP bytes against the ELF.
"""
from pathlib import Path
import argparse,json,struct,subprocess
from bringup import Board
from storage_swd_backup import LiveMemory,mailbox_symbol
from bootstrap_build import TC

FIELDS={
 'g_bootstrap':'magic version heartbeat nor ram display bt stage error maintenance_until requests replies'.split(),
 'g_app_recovery':'state result source_ready verified error reason display_error physical_ms'.split(),
}
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--elf',type=Path,required=True);p.add_argument('--serial',required=True)
    p.add_argument('--output',type=Path,required=True);p.add_argument('--read-khz',type=int,default=950)
    p.add_argument('--symbols',nargs='+',default=['g_bootstrap','g_app_recovery','g_bsp_fault','g_bsp_bringup','g_storage_swd','g_bootstrap_storage_bench'])
    a=p.parse_args();out=a.output;out.mkdir(parents=True,exist_ok=False)
    listing=subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),'-S','--defined-only',str(a.elf)],text=True)
    symbols={v[3]:(int(v[0],16),int(v[1],16)) for line in listing.splitlines() if len(v:=line.split())==4}
    mem=LiveMemory(a.serial,out,mailbox_symbol(a.elf),a.read_khz,120)
    identity=mem.verify_app(a.elf)
    board=Board(a.serial,out,100);board.read_frequency_khz=a.read_khz
    operations=[];entries={}
    for name in a.symbols:
        if name not in symbols:continue
        address,n=symbols[name]
        if not (0x20000000<=address<=0x20030000-n and 0<n<=4096 and n%4==0):raise RuntimeError('Invalid diagnostic symbol '+name)
        operations+=['-u',hex(address),hex(n),str(out/(name+'.bin'))]
        entries[name]=dict(address=address,bytes=n)
    board.command('live-diagnostics',*operations)
    for name,e in entries.items():
        data=(out/(name+'.bin')).read_bytes()
        if len(data)!=e['bytes']:raise RuntimeError('Incomplete diagnostic '+name)
        e['words']=list(struct.unpack('<'+'I'*(len(data)//4),data))
        if len(FIELDS.get(name,[]))==len(e['words']):e['decoded']=dict(zip(FIELDS[name],e['words']))
    result=dict(app=identity,diagnostics=entries)
    (out/'result.json').write_text(json.dumps(result,indent=2));print(json.dumps(entries,indent=2))
if __name__=='__main__':main()
