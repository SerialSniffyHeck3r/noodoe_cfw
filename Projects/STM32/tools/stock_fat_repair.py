"""Offline, evidence-based FAT12 reconstruction; never opens a device.

Keep every readable current file, recover only independently hash-validated
stock assets, and retain unidentified nonblank clusters in RECOVERY/ORPHANS.BIN.
New directory/data blocks occupy proven blank free clusters. BPB, old data and
all reserved tail bytes remain unchanged. This is not a formatter.
"""
from pathlib import Path
import argparse,collections,hashlib,json,math,struct
from resource_install import swapped,Fat,require,sha
CB=32768;LIMIT=4080;SAFE_END=0x7f70000
def batch_bytes(before,after,addresses):
    """Physical bytes; host and device verify the same SHA-bound preimages."""
    require(0<len(addresses)<=120 and len(set(addresses))==len(addresses),'Batch count/duplicates')
    body=bytearray()
    for a in addresses:
        require(4096<=a<=SAFE_END-4096 and a%4096==0,'Protected/unaligned repair sector')
        body+=struct.pack('<I',a)+hashlib.sha256(before[a:a+4096]).digest()+hashlib.sha256(after[a:a+4096]).digest()+after[a:a+4096]
    header=bytearray(64);struct.pack_into('<4I',header,0,0x31505253,1,len(addresses),len(body));header[16:48]=hashlib.sha256(body).digest()
    return bytes(header+body).ljust(524288,b'\xff')
def address(c):return 0x9000+(c-2)*CB
def checksum(s):
    n=0
    for v in s:n=(((n&1)<<7)+(n>>1)+v)&255
    return n
def entries(data):
    pending=[]
    for off in range(0,len(data),32):
        e=bytes(data[off:off+32])
        if not e or e[0]==0:return
        if e[0]==0xe5:pending=[];continue
        if e[11]==15:pending.append(e);continue
        short=e[:8].decode('cp437').rstrip()+('.'+e[8:11].decode('cp437').rstrip() if e[8:11].strip() else '')
        name=short
        if pending and pending[0][0]==(len(pending)|64) and [x[0]&31 for x in pending]==list(range(len(pending),0,-1)) and all(x[13]==checksum(e[:11]) for x in pending):
            chars=b''.join(x[1:11]+x[14:26]+x[28:32] for x in reversed(pending))
            name=chars.decode('utf-16le').split('\0',1)[0].rstrip('\uffff')
        pending=[]
        require(name and not any(ord(c)<32 or c in '/\\' for c in name),'Invalid directory name')
        yield name,e
def directory_entry(name,short,template,cluster,size):
    e=bytearray(template);e[:11]=short;struct.pack_into('<H',e,20,0);struct.pack_into('<H',e,26,cluster);struct.pack_into('<I',e,28,size)
    units=list(struct.unpack('<'+'H'*(len(name.encode('utf-16le'))//2),name.encode('utf-16le')))+[0]
    units += [65535]*((-len(units))%13);parts=[]
    for i in range(len(units)//13-1,-1,-1):
        x=bytearray(32);x[0]=(i+1)|(64 if i==len(units)//13-1 else 0);x[11]=15;x[13]=checksum(short)
        chunk=struct.pack('<13H',*units[i*13:i*13+13]);x[1:11]=chunk[:10];x[14:26]=chunk[10:22];x[28:32]=chunk[22:];parts.append(bytes(x))
    return b''.join(parts)+e
def plan(raw,recovery):
    b=swapped(raw);require(len(b)==0x8000000,'Full NOR required')
    require(b[0x1000:0x3000]==b[0x3000:0x5000],'FAT copies disagree')
    require(struct.unpack_from('<H',b,11)[0]==4096 and b[13]==8 and struct.unpack_from('<I',b,32)[0]==0x7f80,'Unexpected geometry')
    fv=lambda c:(struct.unpack_from('<H',b,0x1000+c+c//2)[0]>>(4 if c&1 else 0))&4095
    def chain(c):
        out=[]
        while 2<=c<LIMIT and c not in out:
            out.append(c);v=fv(c)
            if v>=0xff8:return out,True
            c=v
        return out,False
    dirs={'':bytes(32)};files={};issues=[];original=[]
    def put(path,data,e,candidates,method):
        key=path.casefold()
        if key in files:
            require(files[key]['data']==data,'Conflicting file evidence: '+path);return
        files[key]={'path':path,'data':data,'entry':e,'candidates':candidates,'method':method}
    def walk(path,c=0,ancestors=()):
        if c:
            require(c not in ancestors,'Directory cycle')
            cs,ok=chain(c)
            if not cs:issues.append({'path':path,'reason':'invalid directory cluster'});return
            block=b[address(c):address(c)+CB]
            if block[:11]!=b'.          ' or struct.unpack_from('<H',block,26)[0]!=c:
                issues.append({'path':path,'reason':'directory body overwritten','cluster':c});return
            data=b''.join(b[address(x):address(x)+CB] for x in (cs if ok else [c]))
            if not ok:issues.append({'path':path,'reason':'directory FAT chain broken','cluster':c})
        else:data=b[0x5000:0x9000]
        for name,e in entries(data):
            if name in ('.','..') or e[11]&8:continue
            target=(path+'/' if path else '')+name;cl=struct.unpack_from('<H',e,26)[0];n=struct.unpack_from('<I',e,28)[0]
            if e[11]&16:
                dirs[target]=e;walk(target,cl,ancestors+(c,))
            else:
                cs,ok=chain(cl) if cl else ([],n==0)
                if ok and n<=len(cs)*CB:
                    data=b''.join(b[address(x):address(x)+CB] for x in cs)[:n]
                    put(target,data,e,[cs[:math.ceil(n/CB)]],'current FAT-readable')
                    original.append({'path':target,'bytes':n,'sha256':sha(data)})
                else:issues.append({'path':target,'reason':'file FAT chain truncated/free','cluster':cl,'bytes':n})
    walk('')
    # Restore stock paths only from the already audited, manifest-matched bytes.
    recovered=json.loads((recovery/'manifest-matched-index.json').read_text())
    for r in recovered:
        path=r['manifest_path'];data=(recovery/r['saved_path']).read_bytes()
        require(len(data)==r['bytes'] and sha(data)==r['sha256'] and hashlib.md5(data).hexdigest()==r['md5'],'Recovery source changed')
        if path.casefold() in files:continue  # Current readable files always win.
        cs=[]
        for v in r['source_candidates']:
            a=int(v['image_offset'],0)
            if (a-0x9000)%CB==0 and a>=0x9000 and b[a:a+len(data)]==data:
                c=2+(a-0x9000)//CB;cs.append(list(range(c,c+math.ceil(len(data)/CB))))
        e=bytearray(32);e[11]=32;put(path,data,bytes(e),cs,'manifest MD5 and SHA256 verified')
    # A truncated resource_config has a complete independently parsed source.
    path='resource_config.json';rpath=recovery/'recovered-validated/resource_config.json'
    if path.casefold() not in files:
        data=rpath.read_bytes();json.loads(data);require(sha(data)=='4c9230e61e214dc127e12049e8c46a5d9959f2f97d01847c31c59be508824050','Resource manifest changed')
        e=bytearray(32);e[11]=32;put(path,data,bytes(e),[list(range(2603,2603+math.ceil(len(data)/CB)))],'validated complete JSON')
    # Normalize case to existing directory names, retaining original names.
    canonical={x.casefold():x for x in dirs}
    for f in list(files.values()):
        parts=f['path'].split('/');parent=''
        for part in parts[:-1]:
            desired=(parent+'/' if parent else '')+part;k=desired.casefold()
            if k not in canonical:
                canonical[k]=desired;e=bytearray(32);e[11]=16;dirs[desired]=bytes(e)
            parent=canonical[k]
        f['path']=(parent+'/' if parent else '')+parts[-1]
    require('recovery' not in canonical,'Existing RECOVERY directory requires a new reviewed name')
    e=bytearray(32);e[11]=16;dirs['RECOVERY']=bytes(e)
    used=set();chains={};relocated=[]
    for key,f in files.items():
        count=math.ceil(len(f['data'])/CB)
        for cs in f['candidates']:
            if len(cs)==count and all(2<=c<LIMIT and address(c)+CB<=SAFE_END and c not in used for c in cs) and b''.join(b[address(c):address(c)+CB] for c in cs)[:len(f['data'])]==f['data']:
                chains[key]=cs;used.update(cs);break
    # Preserve every unexplained data cluster, even if the damaged FAT called it
    # free. This prevents a repaired allocator from destroying recoverable bytes.
    orphan=[c for c in range(2,LIMIT) if address(c)+CB<=SAFE_END and c not in used and (fv(c)!=0 or b[address(c):address(c)+CB] not in (bytes(CB),b'\xff'*CB))]
    used.update(orphan)
    free=[c for c in range(2,LIMIT) if address(c)+CB<=SAFE_END and c not in used]
    # Prefer the blank tail, so provisioning changes stay in few narrow ranges.
    free.sort(reverse=True)
    def allocate(n):
        require(len(free)>=n+32,'Insufficient blank space for repair plus resource container')
        cs=[free.pop(0) for _ in range(n)];used.update(cs);return cs
    out=bytearray(b)
    def store(cs,data):
        for i,c in enumerate(cs):
            chunk=data[i*CB:(i+1)*CB];out[address(c):address(c)+len(chunk)]=chunk
    for key,f in files.items():
        if key not in chains:
            cs=allocate(math.ceil(len(f['data'])/CB));chains[key]=cs;store(cs,f['data']);relocated.append(f['path'])
    archive={'source_raw_sha256':sha(raw),'original_files':original,'issues':issues,'orphan_cluster_order':orphan,'relocated':relocated,'unrecoverable_font_directory':True,'note':'Historical missing contents are not invented. Every unexplained nonblank safe cluster is retained in ORPHANS.BIN; original metadata is BEFORE.BIN.'}
    for name,data in [('REPORT.JSON',json.dumps(archive,ensure_ascii=False,indent=2).encode()),('BEFORE.BIN',bytes(b[:0x9000]))]:
        e=bytearray(32);e[11]=32;path='RECOVERY/'+name;key=path.casefold();put(path,data,bytes(e),[],'recovery audit');chains[key]=allocate(math.ceil(len(data)/CB));store(chains[key],data)
    e=bytearray(32);e[11]=32;key='recovery/orphans.bin';files[key]={'path':'RECOVERY/ORPHANS.BIN','data':None,'bytes':len(orphan)*CB,'entry':bytes(e),'method':'unassigned original clusters retained'};chains[key]=orphan
    # Allocate new directories; old directory bodies remain in the orphan file.
    dc={path:allocate(1) for path in dirs if path}
    newfat=bytearray(8192)
    def setfat(c,v):
        off=c+c//2;n=struct.unpack_from('<H',newfat,off)[0];struct.pack_into('<H',newfat,off,(n&15)|(v<<4) if c&1 else (n&0xf000)|v)
    setfat(0,0xff8);setfat(1,0xfff)
    for c in range(2,LIMIT):
        if address(c)+CB>SAFE_END:setfat(c,0xff7)
    for cs in list(chains.values())+list(dc.values()):
        for i,c in enumerate(cs):setfat(c,cs[i+1] if i+1<len(cs) else 0xfff)
    children=collections.defaultdict(list)
    for path,e in dirs.items():
        if path:parent,_,name=path.rpartition('/');children[parent].append((name,e,dc[path][0],0))
    for key,f in files.items():
        parent,_,name=f['path'].rpartition('/');cs=chains[key];n=len(f['data']) if f['data'] is not None else f['bytes'];children[parent].append((name,f['entry'],cs[0] if cs else 0,n))
    for path in dirs:
        payload=bytearray();seen=set()
        if path:
            for name,c in [('.',dc[path][0]),('..',dc[path.rpartition('/')[0]][0] if '/' in path else 0)]:
                e=bytearray(32);e[:11]=name.encode().ljust(11,b' ');e[11]=16;struct.pack_into('<H',e,26,c);payload+=e
        else:
            label=bytearray(32);label[:11]=b'NOODOE     ';label[11]=8;payload+=label
        for name,e,c,n in sorted(children[path],key=lambda x:x[0].casefold()):
            short=e[:11]
            if not short.strip(b'\0 ') or short in seen:
                base,_,ext=name.rpartition('.');base=base if ext else name;ext=ext[:3].upper() if ext else ''
                clean=''.join(x for x in base.upper() if x.isascii() and x.isalnum()) or 'FILE'
                if len(clean)<=8 and clean.ljust(8).encode()+ext.ljust(3).encode() not in seen:short=clean.ljust(8).encode()+ext.ljust(3).encode()
                else:
                    for i in range(1,10000):
                        suffix='~'+str(i);short=(clean[:8-len(suffix)]+suffix).ljust(8).encode()+ext.ljust(3).encode()
                        if short not in seen:break
            seen.add(short);payload+=directory_entry(name,short,e,c,n)
        payload+=bytes(32);require(len(payload)<=(CB if path else 16384),'Directory exceeds one-cluster recovery limit')
        if path:store(dc[path],payload)
        else:out[0x5000:0x9000]=payload+bytes(16384-len(payload))
    out[0x1000:0x3000]=newfat;out[0x3000:0x5000]=newfat
    after=bytes(swapped(out));strict=Fat(after)
    require(after[:4096]==raw[:4096] and after[SAFE_END:]==raw[SAFE_END:],'Protected extent changed')
    # Rewalk long names and compare all desired readable files independently.
    verified={}
    def verify(path,c=0):
        cs=[]
        if c:
            while True:
                cs.append(c);v=strict.fat(c)
                if v>=0xff8:break
                c=v
        data=b''.join(out[address(x):address(x)+CB] for x in cs) if cs else out[0x5000:0x9000]
        for name,e in entries(data):
            if name in ('.','..') or e[11]&8:continue
            target=(path+'/' if path else '')+name;cl=struct.unpack_from('<H',e,26)[0];n=struct.unpack_from('<I',e,28)[0]
            if e[11]&16:verify(target,cl)
            else:
                h=hashlib.sha256();left=n
                while left:
                    count=min(left,CB);h.update(out[address(cl):address(cl)+count]);left-=count;cl=strict.fat(cl)
                verified[target.casefold()]=(n,h.hexdigest())
    verify('')
    for key,f in files.items():
        if f['data'] is not None:require(verified[key]==(len(f['data']),sha(f['data'])),'Reconstructed file differs: '+f['path'])
    claimed=set(strict.refs)
    require(all(strict.fat(c)==0 or c in claimed or strict.fat(c)==0xff7 for c in range(2,LIMIT)),'Orphan allocation remains')
    changed=[a for a in range(4096,SAFE_END,4096) if raw[a:a+4096]!=after[a:a+4096]]
    require(all(a<0x9000 or raw[a:a+4096] in (bytes(4096),b'\xff'*4096) for a in changed),'Repair would overwrite nonblank original data')
    # Data first, secondary FAT, primary FAT, root publication last.
    ordered=sorted(a for a in changed if a>=0x9000)+[a for a in changed if 0x3000<=a<0x5000]+[a for a in changed if 0x1000<=a<0x3000]+[a for a in changed if 0x5000<=a<0x9000]
    return {'schema':1,'before_sha256':sha(raw),'after_sha256':sha(after),'current_readable_files':len(original),'files_after':len(files),'directories_after':len(dirs)-1,'issues':issues,'orphan_clusters_preserved':len(orphan),'relocated':relocated,'changed_sectors':ordered,'verified_files':verified,'free_clusters':sum(strict.fat(c)==0 for c in range(2,LIMIT))},after
def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--backup',type=Path,required=True);ap.add_argument('--recovery',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args()
    manifest=json.loads((a.backup/'manifest.json').read_text());require(manifest['verified'],'Unverified source')
    raw=(a.backup/'A.bin').read_bytes();require(raw==(a.backup/'B.bin').read_bytes(),'Independent backups differ')
    report,after=plan(raw,a.recovery);a.output.mkdir(parents=True,exist_ok=False)
    (a.output/'plan.json').write_text(json.dumps(report,indent=2,ensure_ascii=False));(a.output/'after.bin').write_bytes(after)
    for i,address_ in enumerate(report['changed_sectors']):
        (a.output/f'{i:04d}-{address_:08x}-before.bin').write_bytes(raw[address_:address_+4096]);(a.output/f'{i:04d}-{address_:08x}-after.bin').write_bytes(after[address_:address_+4096])
    print(json.dumps({k:v for k,v in report.items() if k not in ('issues','verified_files','changed_sectors','relocated')},indent=2));print('changed sectors',len(report['changed_sectors']))
if __name__=='__main__':main()
