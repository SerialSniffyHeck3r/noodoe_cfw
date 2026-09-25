"""Read selected Product NOR regions under an owned writer-quiesce lease.

No NOR writes, reset, halt or automatic recovery. This is a regional preservation
check after stock recovery, not a replacement for a whole 128 MiB A/B backup.
Only completed sequential CFWRIDE records may differ from the audited baseline.
"""
from pathlib import Path
import argparse,json,time
from bootstrap_storage import create_plan,recovery_image,STOCK_SHA
from resource_install import Fat,require,sha,swapped
from journal_continuity import apply_proof,audit_delta,records,latest
from storage_backup import verify_unlock_manifest
from storage_swd_backup import LiveMemory,mailbox_symbol,identity,read_segment,save
from storage_swd_resume import journal_snapshot

FILES=(('CFWCFG.DAT',131072),('CFWRIDE.DAT',262144),
       ('CFWPIC.DAT',1048576),('CFWREC.DAT',524288))

def regions(expected,check_staging=False):
    """Derive physical ranges from the fully audited FAT, never fixed file offsets."""
    fs=Fat(expected);out=[dict(name='FAT',offset=0,length=0x9000)]
    for name,size in FILES:
        require(name in fs.files,'Missing '+name)
        chain,n=fs.files[name]
        require(n==size and len(chain)==size//32768,'Unexpected size/chain: '+name)
        require(chain==list(range(chain[0],chain[0]+len(chain))),
                'Regional verifier requires the audited contiguous file: '+name)
        offset=fs.address(chain[0]);require(offset+size<=0x7f70000,'Reserved extent: '+name)
        out.append(dict(name=name,offset=offset,length=size))
    out.append(dict(name='RESERVED_BEFORE_APP_STAGE',offset=0x7f70000,length=0x20000))
    if check_staging:out.append(dict(name='STOCK_APP_STAGE',offset=0x7f90000,length=0x70000))
    return out

def compare_region(region,expected,actual,uid,stock_app):
    """Pure comparison: incomplete/jumped/overwritten ride generations fail closed."""
    name=region['name'];off,n=region['offset'],region['length']
    require(len(actual)==n,'Incomplete region: '+name)
    old=expected[off:off+n]
    if name=='CFWRIDE.DAT':
        before=latest(records(old,uid));after=latest(records(actual,uid))
        delta=audit_delta(old,actual,uid) if old!=actual else None
        # audit_delta also enforces this, kept explicit in evidence and tests.
        slot=before[0]
        require(old[slot*4096:(slot+1)*4096]==actual[slot*4096:(slot+1)*4096],
                'Previous latest completed ride record was overwritten')
        return dict(exact=old==actual,previous_latest_preserved=True,
                    before_latest=list(before),after_latest=list(after),delta=delta)
    if name=='STOCK_APP_STAGE':
        require(len(stock_app)==0x70000 and sha(stock_app)==STOCK_SHA,'Unapproved stock APP')
        require(bytes(swapped(actual))==stock_app,'Stock staging differs from approved canonical APP')
        return dict(exact_approved_stock=True,canonical_sha256=STOCK_SHA)
    require(actual==old,'Unexpected physical NOR change: '+name)
    return dict(exact=True)

def run_reads(memory,elf,expected,uid,stock_app,selected,result,result_path,ready_timeout=180):
    """Own only a newly acquired lease; always release it, including failed reads."""
    started=time.monotonic()
    try:
        require(memory.quiesce(elf,True),'Product writer-quiesce interface is required')
        result['writes_quiesced']=True;save(result_path,result)
        counters=journal_snapshot(memory,elf);result['journal_before']=counters
        box=memory.descriptor();require(box['state']!=1,'Another StorageSWD transfer is busy')
        seq=box['last_request_seq']
        for region in selected:
            begin=time.monotonic();reads=[];actual=None
            # Two independent physical fills for the only permitted mutable file.
            for _ in range(2 if region['name']=='CFWRIDE.DAT' else 1):
                seq=(seq+1)&0xffffffff or 1
                path,evidence=read_segment(memory,region['offset'],region['length'],seq,ready_timeout,3)
                data=path.read_bytes();reads.append(evidence)
                require(actual is None or actual==data,'Independent CFWRIDE reads differ')
                actual=data
            entry=dict(**region,reads=reads,sha256=sha(actual),
                       baseline_sha256=sha(expected[region['offset']:region['offset']+region['length']]),
                       seconds=round(time.monotonic()-begin,3))
            result['regions'].append(entry);save(result_path,result)
            entry['comparison']=compare_region(region,expected,actual,uid,stock_app)
            if region['name']=='CFWRIDE.DAT':
                slot,generation=entry['comparison']['after_latest']
                require(counters[1]['active_sector']==slot and counters[1]['generation']==generation,
                        'Restored ride RAM generation disagrees with physical NOR')
            save(result_path,result)
            print(region['name'],hex(region['offset']),region['length'],'verified',flush=True)
        result['journal_after']=journal_snapshot(memory,elf)
        require(result['journal_after']==counters,'Journal changed under the quiesce lease')
        final=identity(memory);require(final==result['identity'],'Target identity changed during regional reads')
        result['state']='selected_regions_verified';result['verified']=True
    except BaseException as error:
        result.update(state='failed',verified=False,error=str(error));raise
    finally:
        result['seconds']=round(time.monotonic()-started,3)
        try:
            if memory.quiesce_owned:
                memory.quiesce(elf,False);result['write_lease_released']=True
        except BaseException as error:
            result.update(state='lease_release_failed',verified=False,write_lease_released=False,release_error=str(error))
            raise
        finally:save(result_path,result)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--backup',type=Path,required=True)
    p.add_argument('--continuity-proof',type=Path,required=True)
    p.add_argument('--whole-nor-proof',type=Path,help='Earlier bootstrap_nor_verify result.json; no claim of a fresh whole read')
    p.add_argument('--stock-app',type=Path,required=True)
    p.add_argument('--elf',type=Path,required=True);p.add_argument('--serial',required=True)
    p.add_argument('--output',type=Path,required=True);p.add_argument('--read-khz',type=int,default=950)
    p.add_argument('--check-stock-staging',action='store_true')
    a=p.parse_args();manifest_path=a.backup/'manifest.json';m=json.loads(manifest_path.read_text())
    proof=verify_unlock_manifest(manifest_path,m['identity']);uid=m['identity']['uid_words']
    raw=Path(m['captures'][0]['path']).read_bytes();raw,continuity=apply_proof(raw,uid,a.continuity_proof)
    stock=a.stock_app.read_bytes();plan,expected=create_plan(raw,4,recovery_image(stock,uid))
    selected=regions(expected,a.check_stock_staging);prior=None
    if a.whole_nor_proof:
        prior=json.loads(a.whole_nor_proof.read_text())
        require(prior.get('state')=='two_independent_device_hashes_match_expected' and
                prior.get('expected_sha256')==sha(expected) and prior['identity']['uid_words']==uid and
                len(prior.get('passes',[]))==2 and all(x['sha256']==sha(expected) and x['bytes']==0x8000000 and
                    x['state']==8 and not x['error'] for x in prior['passes']), 'Earlier whole-NOR proof mismatch')
        prior=dict(path=str(a.whole_nor_proof.resolve()),sha256=sha(a.whole_nor_proof.read_bytes()),
                   expected_nor_sha256=sha(expected),scope='Earlier device SHA sweeps, not a fresh whole-NOR acquisition')
    out=a.output;out.mkdir(parents=True,exist_ok=False)
    (out/'expected-rec-plan.json').write_text(json.dumps(plan,indent=2))
    mem=LiveMemory(a.serial,out,mailbox_symbol(a.elf),a.read_khz,300)
    target=identity(mem);require(target['uid_words']==uid,'Wrong MCU')
    # Fresh backup validation already occurred before hardware; bind its UID now.
    require(target['usb_serial'].upper()==m['identity']['usb_serial'].upper(),'Backup target mismatch')
    app=mem.verify_app(a.elf)
    result=dict(schema=1,state='started',verified=False,identity=target,app=app,
                original_backup_proof=proof,continuity=continuity,expected_nor_sha256=sha(expected),
                prior_whole_nor_proof=prior,regions=[],selected_ranges=selected,
                unique_physical_bytes=sum(x['length'] for x in selected),
                whole_nor_read=False,downloaded_full_backup=False,
                scope='Only listed physical regions freshly read. Other NOR bytes are not reverified by this run.',
                write_lease_released=False)
    path=out/'result.json';save(path,result)
    run_reads(mem,a.elf,expected,uid,stock,selected,result,path)
    print('Selected physical regions verified; no new whole-NOR backup was created.')

if __name__=='__main__':main()
