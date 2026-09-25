"""Explicit offline audit of post-backup CFWRIDE checkpoints; never a writer.

The original full A/B proof remains untouched. Two independent successful raw
regional acquisitions may establish only completed next-generation journal
records. The resulting baseline is explicitly derived, not another full backup.
"""
from pathlib import Path
import argparse,json,os,struct,zlib
from resource_install import Fat,require,sha,swapped
from storage_backup import verify_unlock_manifest

LENGTH=262144

def extent(raw):
    fs=Fat(raw);require('CFWRIDE.DAT' in fs.files,'CFWRIDE missing')
    chain,n=fs.files['CFWRIDE.DAT'];require(n==LENGTH and len(chain)==8,'CFWRIDE size/chain')
    require(chain==list(range(chain[0],chain[0]+8)),'Continuity supports this contiguous journal only')
    address=fs.address(chain[0]);require(address+LENGTH<=0x7f70000,'Reserved journal extent')
    return address

def records(data,uid):
    require(len(data)==LENGTH,'Journal length');logical=swapped(data);found={}
    for off in range(0,LENGTH,4096):
        b=logical[off:off+4096]
        if b==b'\xff'*4096:continue
        w=struct.unpack_from('<8I',b)
        require(w[0:3]==(0x314a4643,1,2) and w[4]<=3968 and list(w[5:8])==uid,'Invalid journal schema/purpose/UID')
        require(struct.unpack_from('<2I',b,4088)==(zlib.crc32(b[:4088]),0x31544d43),'Incomplete or corrupt journal record')
        found[off//4096]=w[3]
    require(found,'No completed journal record');return found

def latest(found):
    winner=None
    for slot,generation in found.items():
        if winner is None or 0<((generation-winner[1])&0xffffffff)<0x80000000:winner=(slot,generation)
    require(sum(g==winner[1] for g in found.values())==1,'Ambiguous newest generation')
    require(all(g==winner[1] or 0<((winner[1]-g)&0xffffffff)<0x80000000 for g in found.values()),'Ambiguous generation ordering')
    return winner

def audit_delta(old,new,uid):
    before=records(old,uid);after=records(new,uid);slot,generation=latest(before)
    changed=[i for i in range(64) if old[i*4096:(i+1)*4096]!=new[i*4096:(i+1)*4096]]
    require(0<len(changed)<64,'Delta must preserve the previous latest record')
    expected=[(slot+i)%64 for i in range(1,len(changed)+1)]
    require(set(changed)==set(expected) and slot not in changed,'Changed sectors are not the next journal slots')
    for i,index in enumerate(expected,1):require(after.get(index)==((generation+i)&0xffffffff),'Nonconsecutive or reversed generation')
    end=latest(after);require(end==(expected[-1],(generation+len(changed))&0xffffffff),'Unexpected newest record')
    return dict(previous_sector=slot,previous_generation=generation,new_sector=end[0],new_generation=end[1],changed_sectors=changed)

def select_evidence(value,address):
    candidates=[]
    if isinstance(value,dict):
        if 'committed_response' in value:candidates.append(value)
        if isinstance(value.get('read'),dict):candidates.append(value['read'])
        candidates+=value.get('before',[])
    matches=[e for e in candidates if e.get('offset')==address and e.get('length')==LENGTH]
    require(len(matches)==1,'Expected exactly one independent CFWRIDE read evidence object')
    return matches[0]

def checked_capture(e,address,uid):
    require(e.get('offset')==address and e.get('length')==LENGTH,'Capture extent mismatch')
    c=e['committed_response'];seq=e['sequence']
    require(seq and c['state']==2 and c['result']==0 and c['init_result']==0,'Capture was not completed successfully')
    require(c['active_seq']==c['response_seq']==seq and c['response_offset']==address and c['response_length']==c['completed']==LENGTH,'Capture response identity mismatch')
    require([c['uid0'],c['uid1'],c['uid2']]==uid and c['nor_capacity']==0x8000000 and c['jedec_id']==0xc2201b,'Capture target mismatch')
    matches=[]
    for read in e['reads']:
        if not read.get('match'):continue
        path=Path(read['path']);b=path.read_bytes()
        require(len(b)==LENGTH and sha(b)==e['sha256'] and zlib.crc32(b)==e['crc32']==c['crc32']==read['crc32'],'Regional file disagrees with device CRC/SHA')
        matches.append((path,b))
    require(matches,'No successful physical regional acquisition')
    return matches[0]

def derive(raw,uid,captures):
    require(len(captures)==2,'Two independent regional reads required');address=extent(raw)
    a,first=checked_capture(captures[0],address,uid);b,second=checked_capture(captures[1],address,uid)
    require(not os.path.samefile(a,b) and captures[0]['sequence']!=captures[1]['sequence'],'Same request/file is not an independent read')
    require(first==second,'Independent regional reads differ')
    audit=audit_delta(raw[address:address+LENGTH],first,uid)
    rebased=raw[:address]+first+raw[address+LENGTH:]
    audit.update(offset=address,length=LENGTH,before_region_sha256=sha(raw[address:address+LENGTH]),after_region_sha256=sha(first),
                 original_full_backup_sha256=sha(raw),derived_baseline_sha256=sha(rebased))
    return rebased,audit

def apply_proof(raw,uid,path):
    proof=json.loads(Path(path).read_text())
    require(proof.get('schema')==1 and proof.get('kind')=='CFWRIDE_COMMITTED_JOURNAL_DELTA' and proof.get('uid_words')==uid,'Continuity proof identity/schema')
    rebased,audit=derive(raw,uid,proof['captures'])
    require(audit==proof.get('audit'),'Continuity proof changed or no longer matches original full backup')
    return rebased,dict(proof_path=str(Path(path).resolve()),**audit)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--backup',type=Path,required=True)
    p.add_argument('--first-read',type=Path,required=True);p.add_argument('--second-read',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();m=json.loads((a.backup/'manifest.json').read_text());verify_unlock_manifest(a.backup/'manifest.json',m['identity'])
    raw=Path(m['captures'][0]['path']).read_bytes();address=extent(raw);uid=m['identity']['uid_words']
    captures=[select_evidence(json.loads(path.read_text()),address) for path in (a.first_read,a.second_read)]
    _,audit=derive(raw,uid,captures)
    proof=dict(schema=1,kind='CFWRIDE_COMMITTED_JOURNAL_DELTA',uid_words=uid,captures=captures,audit=audit,
               original_manifest=str((a.backup/'manifest.json').resolve()),source_evidence=[str(x.resolve()) for x in (a.first_read,a.second_read)],
               scope='Derived baseline only; original complete A/B backups remain unchanged. Full post-install comparison is required.')
    with a.output.open('x') as f:json.dump(proof,f,indent=2)
    print(json.dumps(audit,indent=2))

if __name__=='__main__':main()
