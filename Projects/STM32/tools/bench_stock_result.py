"""Proof of actual resident-BL stock recovery; never programs or resets.

Run after the explicitly requested APP recovery has finished. Two independent
full internal-flash reads must match the approved stock APP and preserved
resident/device sectors. A pending installer or mismatch stops, not retries it.
Default acquisition is live/read-only. Explicit --halted-reads permits temporary
debug-freeze and halt/run control only after an already completed full stock
read and current completed metadata have independently established installation.
"""
from pathlib import Path
import argparse, hashlib, json, struct, time
from bringup import Board, require

def sha(data): return hashlib.sha256(data).hexdigest()

COMPLETED_METADATA=[0x000e0000,0x00100005,0x7f90,0x70000,0]

def validate_completed(data,before,stock):
    """Exact full image/preservation proof, reusable before any debug control."""
    require(len(data)==0x80000,'Incomplete flash read')
    require(data[0x10000:]==stock,'Resident did not install the complete approved stock APP')
    require(data[:0x8000]==before[:0x8000],'Resident sectors S0/S1 changed')
    require(data[0x8014:0xc000]==before[0x8014:0xc000],'Unrelated metadata sector contents changed')
    require(data[0xc000:0x10000]==before[0xc000:0x10000],'Device sector S3 changed')
    words=list(struct.unpack_from('<5I',data,0x8000))
    require(words==COMPLETED_METADATA,'Unexpected completed stock install metadata')
    return words

def acquire(b,out,result,save,halted):
    """Restore only our debug controls in finally, including uncertain failures."""
    halt_attempted=False
    try:
        if halted:
            path=out/'dhcsr-before-halt.bin'
            b.command('check-running-before-halt','-u','0xE000EDF0','4',str(path))
            require(not struct.unpack('<I',path.read_bytes())[0]&(1<<17),
                    'Target already halted; refusing to take another debugger halt')
            result['debug_prepare_attempted']=True;save()
            b.prepare()
            result['debug_freeze_before']=b.freeze_before
            halt_attempted=True;result['halt_attempted']=True;save()
            b.command('halt-completed-stock','-halt')
        for tag in ('A','B'):
            path=out/f'full-{tag}.bin'
            b.command('read-full-'+tag,'-u','0x08000000','0x80000',str(path))
            data=path.read_bytes();require(len(data)==0x80000,'Incomplete flash read')
            result['sha256_'+tag]=sha(data);save()
    finally:
        errors=[]
        if halted and hasattr(b,'freeze_before'):
            try:
                b.command('restore-debug-freeze','-w32','0xE0042008',hex(b.freeze_before))
                result['debug_freeze_restored']=True
            except BaseException as error:errors.append('restore debug freeze: '+str(error))
        if halt_attempted:
            try:
                b.command('resume-completed-stock','-run');result['resume_command_succeeded']=True
            except BaseException as error:errors.append('resume: '+str(error))
        if errors:
            result['cleanup_errors']=errors;save()
            raise RuntimeError('; '.join(errors))
        save()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--before',type=Path,required=True)
    p.add_argument('--stock-app',type=Path,required=True)
    p.add_argument('--serial',required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--read-khz',type=int,default=4000)
    p.add_argument('--completed-read',type=Path,help='Existing complete512KiB stock read, validated before any target control')
    p.add_argument('--halted-reads',action='store_true',help='Explicit temporary debug freeze/halt for two reads; requires --completed-read; always restore/run, never reset')
    p.add_argument('--observe-runtime',action='store_true',help='Record read-only DHCSR/VTOR/PCSR samples after exact flash proof; does not prove UI health')
    a=p.parse_args();before=a.before.read_bytes();stock=a.stock_app.read_bytes()
    require(len(before)==0x80000 and len(stock)==0x70000,'Image dimensions')
    require(sha(before[:0x8000])=='f8b379c3fac078a8e01d8b6c36fccc5bb5ea5852a0db008822ef6871e0f38df5','Unapproved preimage resident')
    require(sha(stock)=='162faeecc64bf56d828570168785e66d1a7da0152ef912bf83f9a19094d91edf','Unapproved stock image')
    require(not a.halted_reads or a.completed_read is not None,'Halted reads require an independently completed stock read')
    completed=a.completed_read.read_bytes() if a.completed_read else None
    if completed is not None:validate_completed(completed,before,stock)
    out=a.output;out.mkdir(parents=True,exist_ok=False)
    b=Board(a.serial,out,100);b.read_frequency_khz=a.read_khz
    result=dict(state='reading',wireless_tested=False,physical_button_gesture_tested=False,
                halted_reads=a.halted_reads,debug_control_writes=a.halted_reads,
                internal_flash_programmed=False,nor_written=False,reset_issued=False,
                completed_read_path=str(a.completed_read.resolve()) if a.completed_read else None,
                completed_read_sha256=sha(completed) if completed is not None else None)
    def save(): (out/'result.json').write_text(json.dumps(result,indent=2))
    save()
    # Read without halt: do not freeze the resident installer in mid-copy.
    b.command('read-metadata','-u','0x08008000','20',str(out/'metadata.bin'))
    metadata=list(struct.unpack('<5I',(out/'metadata.bin').read_bytes()))
    result['metadata']=metadata;save()
    require(metadata==COMPLETED_METADATA,'Resident mismatch, unexpected metadata or update still pending')
    try:acquire(b,out,result,save,a.halted_reads)
    except BaseException as error:
        result.update(state='acquisition_failed',error=str(error));save();raise
    first=(out/'full-A.bin').read_bytes();second=(out/'full-B.bin').read_bytes()
    require(first==second,'Independent full flash acquisitions differ')
    words=validate_completed(first,before,stock)
    if completed is not None:require(first==completed,'New full reads differ from previously completed stock evidence')
    result.update(state='resident_stock_install_verified',stock_app_sha256=sha(stock),
        resident_sha256=sha(first[:0x8000]),device_sector_sha256=sha(first[0xc000:0x10000]),
        metadata=words,metadata_words_intentionally_changed=True,
        metadata_sector_tail_preserved=True,runtime_health_verified=False)
    save()
    if a.observe_runtime:
        # Core-register transfers can require debug halt; these memory-mapped
        # observations do not change debug control or enable trace. PCSR may
        # be unavailable/stale, so it never acts as a health acceptance gate.
        observations=[];result['runtime_observations']=observations
        try:
            for index in range(3):
                files={name:out/f'runtime-{index}-{name}.bin' for name in ('dhcsr','vtor','pcsr')}
                ops=[]
                for name,address in [('dhcsr',0xE000EDF0),('vtor',0xE000ED08),('pcsr',0xE000101C)]:
                    ops+=['-u',hex(address),'4',str(files[name])]
                b.command(f'runtime-read-only-{index}',*ops)
                values={name:struct.unpack('<I',path.read_bytes())[0] for name,path in files.items()}
                status=values['dhcsr'];pc=values['pcsr']&~1
                values.update(halted=bool(status&(1<<17)),sleeping=bool(status&(1<<18)),
                    lockup=bool(status&(1<<19)),pcsr_points_into_app=0x08010000<=pc<0x08080000,
                    interpretation='Live diagnostic sample only; PCSR may be stale/unavailable and does not establish UI or RTOS health.')
                observations.append(values);save()
                if index<2:time.sleep(.2)
        except Exception as error:
            result['runtime_observation_error']=str(error);save()
    print(json.dumps(result,indent=2))

if __name__=='__main__':main()
