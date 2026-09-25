"""Create immutable recovery/initial gate files through bounded Bootstrap SWD.

No hardware access in plan mode. Execute requires exact live Bootstrap ELF,
matching MCU UID, independently verified whole NOR A/B files, unchanged live
metadata/free extent, and matching device-side FAT plan before commit. Never
flashes APP, replaces files, or grants a general raw NOR write interface.
"""
from pathlib import Path
import argparse,json,struct,subprocess,time
from bootstrap_storage import recovery_image,create_plan
from resource_install import require,sha,swapped,TC,Fat
from storage_backup import verify_unlock_manifest

KINDS={'recovery':(4,1),'gate-a':(5,5),'gate-b':(6,6),'gate-journal':(7,7),'device-log':(8,9)}

def prepare_payload(kind,uid,stock_app=None,container=None,product_elf=None,debug_elf=None):
    """All content and actual ELF budgets are checked before publishing a plan.

    Initial gate containers must be exactly the canonical generation1 files
    derived from this Product ELF. A JSON 'budget_passed' field is not proof.
    This function does not create output directories or access a device.
    """
    require(kind in KINDS,'Unsupported named container')
    number,command=KINDS[kind]
    if number==4:
        require(stock_app is not None,'Recovery requires approved --stock-app')
        payload=recovery_image(stock_app.read_bytes(),uid)
        if container is not None:require(container.read_bytes()==payload,'Recovery container differs from canonical pinned image')
        return number,command,payload,None
    if number==8:
        from gate_bundle import log_container
        payload=log_container(uid)
        require(container is None or container.read_bytes()==payload,'Log identity differs from canonical UID')
        return number,command,payload,None
    require(stock_app is None and container is not None,'Gate files use --container, not --stock-app')
    require(product_elf is not None and debug_elf is not None,'Gate provisioning requires actual --product-elf and --debug-elf budgets')
    from gate_bundle import image_container,initial_journal,PRODUCT_BYTES
    from memory_report import report
    from validate_image import Elf32,validate
    release=report(product_elf,'Product','Release')
    require(release['budget_passed'],
            'Product memory gate failed; no provisioning plan: '+repr(release['failures']))
    debug=report(debug_elf,'Product','Debug')
    require(debug['budget_passed'],
            'Product memory gate failed; no provisioning plan: '+repr(debug['failures']))
    app,layout=validate(Elf32(product_elf.read_bytes()))
    require(layout['layout_version']==2 and len(app)<=PRODUCT_BYTES,'Expected resident-gate Product layout2')
    app=app.ljust(PRODUCT_BYTES,b'\xff')
    payload=container.read_bytes()
    from gate_containers import check_container
    detail=check_container(payload,number,uid)
    canonical=image_container(app,uid,number-5) if number in (5,6) else initial_journal(app,uid)
    require(payload==canonical,'Gate container does not match the validated Product ELF and canonical initial state')
    detail.update(release_memory=release,debug_memory=debug,product_elf_sha256=sha(product_elf.read_bytes()),debug_elf_sha256=sha(debug_elf.read_bytes()))
    return number,command,payload,detail

def symbols(elf):
    listing=subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),'-S','--defined-only',str(elf)],text=True)
    return {p[3]:(int(p[0],16),int(p[1],16)) for line in listing.splitlines() if len(p:=line.split())==4}

def decode(data):
    require(len(data)==128,'REC mailbox length')
    v=struct.unpack('<32I',data)
    require(v[:2]==(0x31575342,1),'REC mailbox ABI')
    require(v[3]>=524288 and 0xc0000000<=v[2]<=0xc4000000-v[3],'REC arena outside SDRAM')
    require(0xc0000000<=v[31]<=0xc4000000-0x9000,'REC plan outside SDRAM')
    return dict(buffer=v[2],capacity=v[3],command=v[4],uid=list(v[6:9]),sequence=v[25],ack=v[26],
                state=v[27],error=v[28],first=v[29],bytes=v[30],metadata=v[31])

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('action',choices=('plan','execute'));p.add_argument('--backup',type=Path,required=True)
    p.add_argument('--kind',choices=tuple(KINDS),default='recovery')
    p.add_argument('--stock-app',type=Path,help='Canonical approved448KiB APP, not full512KiB flash')
    p.add_argument('--container',type=Path,help='Canonical named gate container; recovery may use this as an additional exact check')
    p.add_argument('--product-elf',type=Path);p.add_argument('--debug-elf',type=Path)
    p.add_argument('--output',type=Path,required=True);p.add_argument('--elf',type=Path);p.add_argument('--serial')
    p.add_argument('--continuity-proof',type=Path,help='Explicit independently audited CFWRIDE-only committed checkpoint delta')
    p.add_argument('--previous-install',type=Path,action='append',default=[],help='Ordered create-only step with sector journal and two physical whole-NOR hash sweeps')
    p.add_argument('--pending-install',type=Path,action='append',default=[],help='Initial gate A/B/BOOT batch prefix: physical region proofs, final whole-NOR verification still required before APP installation')
    p.add_argument('--resource-update',type=Path,help='Verified update of an originally empty NOODOE.RSC B slot, before gate-file batch')
    p.add_argument('--read-khz',type=int,default=950);a=p.parse_args()
    manifest_path=a.backup/'manifest.json';manifest=json.loads(manifest_path.read_text())
    recorded=manifest['identity'];proof=verify_unlock_manifest(manifest_path,recorded)
    uid=recorded['uid_words'];raw=Path(manifest['captures'][0]['path']).read_bytes()
    require(len(raw)==0x8000000 and sha(raw)==proof['sha256_a'],'Fresh backup proof mismatch')
    continuity=None
    if a.continuity_proof:
        from journal_continuity import apply_proof
        raw,continuity=apply_proof(raw,uid,a.continuity_proof)
    resource_delta=None
    if a.resource_update:
        from bootstrap_install_chain import apply_resource_update
        raw,resource_delta=apply_resource_update(raw,a.resource_update)
    from bootstrap_install_chain import apply_chain
    raw,chain=apply_chain(raw,uid,a.previous_install)
    if a.pending_install:
        from bootstrap_install_chain import apply_pending_batch
        require(not a.previous_install,'Do not mix pending and completed chain bases')
        raw,chain=apply_pending_batch(raw,uid,a.pending_install,a.kind)
    kind,prepare_command,payload,content=prepare_payload(a.kind,uid,a.stock_app,a.container,a.product_elf,a.debug_elf)
    plan,after=create_plan(raw,kind,payload,uid=uid)
    out=a.output;out.mkdir(parents=True,exist_ok=False)
    (out/'payload.bin').write_bytes(payload);(out/'plan.json').write_text(json.dumps(plan,indent=2))
    if content:(out/'content-validation.json').write_text(json.dumps(content,indent=2))
    if continuity:(out/'continuity-audit.json').write_text(json.dumps(continuity,indent=2))
    if chain:(out/'install-chain.json').write_text(json.dumps(chain,indent=2))
    regions=[dict(offset=0,length=0x9000),dict(offset=plan['address'],length=len(payload))]
    # Product may have flushed a final checkpoint after the full A/B reader
    # released its lease and before Bootstrap was installed. These independent
    # live preconditions prevent silently using an obsolete expected image.
    fs=Fat(raw);unchanged=[]
    for name in ('CFWCFG.DAT','CFWRIDE.DAT','CFWPIC.DAT','CFWREC.DAT','CFWA.DAT','CFWB.DAT','CFWBOOT.DAT'):
        if name not in fs.files:continue
        for cluster in fs.files[name][0]:
            address=fs.address(cluster)
            if unchanged and unchanged[-1]['name']==name and unchanged[-1]['offset']+unchanged[-1]['length']==address:
                unchanged[-1]['length']+=32768
            else:unchanged.append(dict(name=name,offset=address,length=32768))
    for r in unchanged:r['before_sha256']=sha(raw[r['offset']:r['offset']+r['length']])
    changed=[]
    for r in regions:
        off,n=r['offset'],r['length'];r.update(before_sha256=sha(raw[off:off+n]),after_sha256=sha(after[off:off+n]))
        for pos in range(off,off+n,4096):
            if raw[pos:pos+4096]!=after[pos:pos+4096]:
                (out/f'{pos:08x}-before.bin').write_bytes(raw[pos:pos+4096]);(out/f'{pos:08x}-after.bin').write_bytes(after[pos:pos+4096]);changed.append(pos)
    (out/'changed-sectors.json').write_text(json.dumps(changed));(out/'regions.json').write_text(json.dumps(regions,indent=2))
    (out/'unchanged-cfw-preconditions.json').write_text(json.dumps(unchanged,indent=2))
    print(a.kind,'plan ready:',hex(plan['address']),len(changed),'changed sectors',flush=True)
    if a.action=='plan':return
    require(a.elf and a.serial,'Explicit Bootstrap ELF and ST-LINK serial required')
    from bringup import Board
    from storage_swd_backup import LiveMemory,mailbox_symbol,read_segment,identity
    syms=symbols(a.elf);address,size=syms['g_bootstrap_storage_bench']
    require(size==128 and 0x20000000<=address<=0x20030000-size and not address%4,'Invalid REC mailbox symbol')
    require('BootstrapStorage_BenchProcess' in syms,'ELF has no Bootstrap REC service')
    board=Board(a.serial,out,50);board.read_frequency_khz=a.read_khz
    mem=LiveMemory(a.serial,out,mailbox_symbol(a.elf),a.read_khz,240)
    descriptor=mem.descriptor();app=mem.verify_app(a.elf);target=identity(mem)
    require(target['uid_words']==uid,'Target UID differs from backup')
    verify_unlock_manifest(manifest_path,target)
    evidence=dict(state='preflight',kind=a.kind,content=content,identity=target,app=app,before=[],after=[],proof=proof,continuity=continuity,install_chain=chain,resource_delta=resource_delta)
    def save(): (out/'result.json').write_text(json.dumps(evidence,indent=2))
    save();counter=0
    def box():
        nonlocal counter
        counter+=1;path=out/f'rec-mailbox-{counter:04d}.bin';board.command(path.stem,'-u',hex(address),hex(size),str(path))
        return decode(path.read_bytes())
    b=box();require(b['sequence']==b['ack'] and b['state'] in (0,8,9),'REC or another maintenance operation busy')
    arena_start,arena_end=b['buffer'],b['buffer']+len(payload)
    read_start=descriptor['buffer_address'];read_end=read_start+descriptor['buffer_capacity']
    require(arena_end<=read_start or read_end<=arena_start,'REC arena overlaps frozen NOR read buffer')
    require(arena_end<=b['metadata'] or b['metadata']+0x9000<=arena_start,'REC arena overlaps metadata workspace')
    request=b['sequence'];seq=descriptor['last_request_seq']
    for r in regions+unchanged:
        seq+=1;path,detail=read_segment(mem,r['offset'],r['length'],seq,120,3)
        match=sha(path.read_bytes())==r['before_sha256']
        evidence['before'].append(dict(**detail,precondition=r,matches_backup=match));save()
        require(match,'Live FAT/free extent/CFW journal differs from backup; no write. Preserve this delta for explicit continuity audit')
    board.command('rec-upload-arena','-w',str(out/'payload.bin'),hex(b['buffer']))
    require(mem.upload('rec-input-readback',b['buffer'],len(payload)).read_bytes()==payload,'REC arena readback mismatch')
    fields=struct.pack('<5I',prepare_command,0x42414b32,*uid)+bytes.fromhex(proof['sha256_a'])+bytes.fromhex(sha(payload))
    require(len(fields)==84,'REC request layout')
    (out/'request-fields.bin').write_bytes(fields)
    board.command('rec-fields','-w',str(out/'request-fields.bin'),hex(address+16))
    # Sequence is the sole publication point. Do not erase a previous sequence
    # while preparing a request, and never retry a timed-out mutation command.
    def command(value):
        nonlocal request
        request=(request+1)&0xffffffff or 1
        board.command(f'rec-command-{request}','-w32',hex(address+16),hex(value),'-w32',hex(address+100),hex(request))
        deadline=time.monotonic()+600
        while True:
            now=box()
            if now['ack']==request:
                require(now['error']==0,f'REC rejected/failed: {now}; inspect journal before any retry')
                return now
            require(time.monotonic()<deadline,'REC timed out; no automatic retry/reset')
            time.sleep(.2)
    evidence['state']='preparing';save();b=command(prepare_command)
    require(b['state']==6 and b['first']==plan['address'] and b['bytes']==len(payload),'Device selected a different extent')
    prepared=mem.upload('rec-prepared-metadata',b['metadata'],0x9000).read_bytes()
    require(prepared==bytes(swapped(after[:0x9000])),'Device FAT plan differs; COMMIT withheld')
    evidence['state']='writing';save();b=command(2);require(b['state']==8,'REC not committed')
    evidence['state']='readback';save()
    for r in regions:
        seq+=1;path,detail=read_segment(mem,r['offset'],r['length'],seq,120,3)
        require(sha(path.read_bytes())==r['after_sha256'],'REC physical postimage mismatch')
        evidence['after'].append(detail);save()
    evidence['state']='regions_verified';evidence['global_verify_pending']=True;evidence['reboot_pending']=True
    evidence['global_verification_method']='Two independent physical whole-NOR SHA sweeps against the audited expected postimage; original downloaded full A/B retained unchanged'
    evidence['expected_whole_nor_sha256']=sha(after);save()
    print('REC changed ranges verified. Two whole-NOR physical SHA sweeps and reboot validation remain pending; these sweeps do not create new downloaded A/B backups.')

if __name__=='__main__':main()
