"""Two fresh device-side whole NOR SHA sweeps against the audited REC postimage.

This downloads digests, not NOR backup files. The original full A/B and explicit
journal continuity proof remain the recovery evidence; no verified-backup
manifest is created. No erase/program/reset/APP installation is supported.
"""
from pathlib import Path
import argparse,json,struct,time
from bootstrap_recovery_install import decode,symbols
from bootstrap_storage import recovery_image,create_plan
from resource_install import require,sha
from storage_backup import verify_unlock_manifest

def completed_snapshot(read_box, sequence):
    """Acquire the digest only after an observed completion publication.

    ST-LINK reads the mailbox in chunks, so the read which first observes ack
    may contain digest bytes from before ack. Discard it, then require two
    matching completed reads before consuming the result.
    """
    first=read_box();second=read_box()
    keys=('sequence','ack','state','error','bytes')
    require(all(x[0]['sequence']==sequence and x[0]['ack']==sequence for x in (first,second)),
            'Mailbox ownership changed after completion')
    require(all(first[0][k]==second[0][k] for k in keys) and first[1]==second[1],
            'Hash completion snapshot is not stable')
    return second

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--backup',type=Path,required=True)
    p.add_argument('--continuity-proof',type=Path)
    source=p.add_mutually_exclusive_group(required=True)
    source.add_argument('--stock-app',type=Path)
    source.add_argument('--install-record',type=Path,help='Completed bounded create to verify, not a backup')
    p.add_argument('--previous-install',type=Path,action='append',default=[])
    p.add_argument('--pending-install',type=Path,action='append',default=[],help='Initial gate batch prefix; verifies final A+B+BOOT postimage together')
    p.add_argument('--resource-update',type=Path)
    p.add_argument('--elf',type=Path,required=True);p.add_argument('--serial',required=True)
    p.add_argument('--output',type=Path,required=True);p.add_argument('--read-khz',type=int,default=950);a=p.parse_args()
    manifest=json.loads((a.backup/'manifest.json').read_text());proof=verify_unlock_manifest(a.backup/'manifest.json',manifest['identity'])
    raw=Path(manifest['captures'][0]['path']).read_bytes();uid=manifest['identity']['uid_words'];continuity=None
    if a.continuity_proof:
        from journal_continuity import apply_proof
        raw,continuity=apply_proof(raw,uid,a.continuity_proof)
    resource_delta=None
    if a.resource_update:
        from bootstrap_install_chain import apply_resource_update
        raw,resource_delta=apply_resource_update(raw,a.resource_update)
    from bootstrap_install_chain import apply_chain,apply_install
    raw,chain=apply_chain(raw,uid,a.previous_install)
    if a.pending_install:
        from bootstrap_install_chain import apply_pending_batch
        require(not a.previous_install and a.install_record is not None,'Pending batch requires final install record')
        kind=json.loads((a.install_record/'result.json').read_text())['kind']
        require(kind=='gate-journal','Global batch verification requires all three initial files')
        raw,chain=apply_pending_batch(raw,uid,a.pending_install,kind)
    if a.install_record:
        expected,step=apply_install(raw,uid,a.install_record,require_global=False)
        plan=json.loads((a.install_record/'plan.json').read_text())
    else:
        plan,expected=create_plan(raw,4,recovery_image(a.stock_app.read_bytes(),uid));step=None
    expected_sha=sha(expected)
    out=a.output;out.mkdir(parents=True,exist_ok=False);(out/'expected-plan.json').write_text(json.dumps(plan,indent=2))
    from bringup import Board
    from storage_swd_backup import LiveMemory,mailbox_symbol,identity
    mem=LiveMemory(a.serial,out,mailbox_symbol(a.elf),a.read_khz,240);mem.descriptor();app=mem.verify_app(a.elf);target=identity(mem)
    require(target['uid_words']==uid,'Wrong MCU');verify_unlock_manifest(a.backup/'manifest.json',target)
    address,n=symbols(a.elf)['g_bootstrap_storage_bench'];require(n==128 and 0x20000000<=address<=0x20030000-128,'Hash mailbox range/ABI')
    board=Board(a.serial,out,50);board.read_frequency_khz=a.read_khz;counter=0
    result=dict(state='started',kind='whole_nor_device_hash_verification',downloaded_backup=False,expected_sha256=expected_sha,identity=target,app=app,original_backup_proof=proof,continuity=continuity,install_chain=chain,install_step=step,resource_delta=resource_delta,passes=[])
    def save(): (out/'result.json').write_text(json.dumps(result,indent=2))
    def box():
        nonlocal counter
        counter+=1;path=out/f'hash-mailbox-{counter:04d}.bin';board.command(path.stem,'-u',hex(address),'128',str(path));b=path.read_bytes();return decode(b),b[68:100].hex()
    save()
    owned_sequence=None
    def cancel_owned():
        nonlocal owned_sequence
        if owned_sequence is None:return
        b,_=box()
        require(b['sequence']==owned_sequence,'Hash ownership changed; cancellation withheld')
        if b['ack']==owned_sequence:
            owned_sequence=None;return
        seq=(owned_sequence+1)&0xffffffff or 1
        # Do not reset or reflash. A new cancellation publication releases the
        # read-only lease at the next bounded operation.
        board.command('hash-cancel-command','-w32',hex(address+16),'0x3')
        board.command('hash-cancel-sequence','-w32',hex(address+100),hex(seq))
        deadline=time.monotonic()+30
        while True:
            b,_=box()
            if b['ack']==seq:
                result['cancellation']=dict(sequence=seq,state=b['state'],error=b['error'])
                owned_sequence=None;return
            require(time.monotonic()<deadline,'Cancellation acknowledgement timed out')
            time.sleep(.2)
    try:
        for index in range(2):
            b,_=box();require(b['sequence']==b['ack'] and b['state'] in (0,8,9),'Another operation owns Bootstrap storage')
            seq=(b['sequence']+1)&0xffffffff or 1
            fields=out/f'hash-fields-{index}.bin';fields.write_bytes(struct.pack('<5I',4,0x42414b32,*uid))
            board.command(f'hash-fields-{index}','-w',str(fields),hex(address+16))
            # Mark the possible lease before publication: even a transport error
            # can occur after the MCU received the new sequence.
            owned_sequence=seq
            board.command(f'hash-start-{index}','-w32',hex(address+100),hex(seq));started=time.monotonic()
            while True:
                b,digest=box()
                if b['ack']==seq:
                    b,digest=completed_snapshot(box,seq);owned_sequence=None;break
                require(time.monotonic()-started<600,'Hash sweep timed out; no automatic retry/reset');time.sleep(3)
            record=dict(sequence=seq,bytes=0x8000000,state=b['state'],error=b['error'],sha256=digest,seconds=time.monotonic()-started)
            result['passes'].append(record);save()
            require(b['state']==8 and b['error']==0 and b['bytes']==0x8000000,'Hash sweep failed')
            require(digest==expected_sha,'Whole NOR differs from audited expected postimage')
            print('Whole NOR sweep',index+1,'matches',digest,flush=True)
    except BaseException as error:
        result['state']='failed';result['failure']=str(error)
        try:cancel_owned()
        except BaseException as cleanup_error:result['cancellation_error']=str(cleanup_error)
        save();raise
    result['state']='two_independent_device_hashes_match_expected';save()
    print('Whole NOR device hashes verified. This is not a new downloaded A/B backup.')

if __name__=='__main__':main()
