"""Create-only CFW containers in a fully audited stock FAT12 image.

plan: offline artifacts only. execute: fresh verified full A/B backup, current
APP/UID and live metadata/free-cluster readbacks are mandatory. Never formats,
repairs, replaces an existing name, touches resource/staging or flashes APP.
"""
from pathlib import Path
import argparse, hashlib, json, struct, subprocess, time, zlib
from resource_install import Fat, require, sha, swapped, TC
from cfw_legacy import migrate

NAMES = ('CFWCFG.DAT', 'CFWRIDE.DAT', 'CFWPIC.DAT')
SHORT = (b'CFWCFG  DAT', b'CFWRIDE DAT', b'CFWPIC  DAT')
SIZES = (131072, 262144, 1048576)
SAFE_END = 0x7f70000
MAGIC, COMMIT = 0x314a4643, 0x31544d43

def record(purpose, uid, payload=b'', generation=0):
    require(len(payload) <= 3968 and len(uid) == 3, 'Record bounds/UID')
    b = bytearray(b'\xff' * 4096)
    struct.pack_into('<8I', b, 0, MAGIC, 1, purpose, generation, len(payload), *uid)
    b[64:64+len(payload)] = payload
    struct.pack_into('<2I', b, 4088, zlib.crc32(b[:4088]), COMMIT)
    return bytes(b)

def file_bytes(fs, name):
    chain, n = fs.files[name]
    return b''.join(fs.b[fs.address(c):fs.address(c)+32768] for c in chain)[:n]

def photos(fs):
    """Originals are read-only migration inputs, never overwritten/unlinked."""
    result = []
    for slot in range(3):
        name = f'WALL{slot}.JPG'
        if name not in fs.files:
            choices = [p for p in fs.files if p.upper().startswith(f'ALBUM/{slot}/') and p.upper().endswith('.JPG')]
            require(len(choices) == 1, f'No unambiguous migration photo for slot {slot}')
            name = choices[0]
        b = file_bytes(fs, name)
        require(0 < len(b) <= 131072, f'JPEG capacity: {name}')
        # Pillow is host-side validation only; production PhotoStore still fully
        # decodes every complete bank before exposing it to the image loader.
        from PIL import Image
        import io
        with Image.open(io.BytesIO(b)) as im:
            require(im.format == 'JPEG' and not im.info.get('progressive') and 0 < im.width <= 480 and 0 < im.height <= 480, 'Unsupported JPEG')
            im.load()
        result.append((name, b))
    return result

def make_plan(raw, uid):
    fs = Fat(raw)
    require(not any(n in fs.files for n in NAMES), 'CFW filename collision; never replace existing files')
    # Allocated orphans are not free capacity, but indicate an incomplete FAT
    # allocation graph. Explicit repair belongs to a separate, backed-up task.
    require(all(fs.fat(c) in (0, 0xff7) or c in fs.refs for c in range(2, 4080)), 'Unowned allocated cluster')
    old_hashes = {n: sha(file_bytes(fs, n)) for n, (_, size) in fs.files.items() if size}
    free = [c for c in range(2, 4080) if fs.fat(c) == 0 and c not in fs.refs and fs.address(c)+32768 <= SAFE_END]
    source_photos = photos(fs)
    config, ride, migration = migrate(raw, uid)
    images = [record(1, uid, config)+b'\xff'*(SIZES[0]-4096), record(2, uid, ride)+b'\xff'*(SIZES[1]-4096)]
    pic = bytearray(b'\xff' * SIZES[2])
    pic[:4096] = record(3, uid, struct.pack('<2I', 0x31465043, 1))
    for slot, (_, data) in enumerate(source_photos):
        off = 65536 + slot*2*163840
        pic[off:off+4096] = record(16+slot, uid, struct.pack('<3I', 0x314a5043, len(data), zlib.crc32(data)), 1)
        pic[off+4096:off+4096+len(data)] = data
    images.append(bytes(pic))
    allocations = []
    for f, (name, short, n, image) in enumerate(zip(NAMES, SHORT, SIZES, images)):
        count = n//32768
        first = next((c for c in free if c+count <= 4080 and fs.address(c)+n <= SAFE_END and all(fs.fat(k) == 0 and k not in fs.refs for k in range(c, c+count))), None)
        require(first is not None, 'Insufficient safe contiguous capacity')
        entry = next((i for i in range(512) if fs.b[0x5000+i*32] in (0, 0xe5)), None)
        require(entry is not None, 'Root directory full')
        for k in range(count): fs.setfat(first+k, 0xfff if k+1 == count else first+k+1)
        e = bytearray(32); e[:11] = short; e[11] = 0x20
        struct.pack_into('<H', e, 26, first); struct.pack_into('<I', e, 28, n)
        fs.b[0x5000+entry*32:0x5000+(entry+1)*32] = e
        off = fs.address(first); fs.b[off:off+n] = image
        allocations.append(dict(name=name, first_cluster=first, root_index=entry, offset=off, length=n))
    after = bytes(swapped(fs.b)); checked = Fat(after)
    require(all(sha(file_bytes(checked, n)) == h for n, h in old_hashes.items()), 'Existing file changed')
    require(raw[SAFE_END:] == after[SAFE_END:], 'Reserved tail changed')
    regions = [dict(offset=0, length=0x9000)] + [dict(offset=a['offset'], length=a['length']) for a in allocations]
    for r in regions:
        o, n = r['offset'], r['length']; r.update(before_sha256=sha(raw[o:o+n]), after_sha256=sha(after[o:o+n]))
    payload = b''.join(images)
    plan = dict(schema=1, before_sha256=sha(raw), after_sha256=sha(after), uid_words=uid,
                free_bytes_before=len(free)*32768, additional_bytes=sum(SIZES), files=allocations, regions=regions, migration=migration,
                metadata_crc=zlib.crc32(swapped(raw[:0x9000])), payload_crc=zlib.crc32(payload),
                existing_file_sha256=old_hashes, cluster_owners=fs.refs,
                photos=[dict(source=n, bytes=len(b), sha256=sha(b)) for n, b in source_photos])
    return plan, after, payload

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('action', choices=('plan', 'execute')); ap.add_argument('--backup', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True); ap.add_argument('--elf', type=Path); ap.add_argument('--serial')
    ap.add_argument('--read-khz', type=int, default=950)
    args = ap.parse_args(); manifest = json.loads((args.backup/'manifest.json').read_text())
    require(manifest.get('verified') and manifest.get('state') == 'verified', 'Verified backup manifest required')
    raw = (args.backup/'A.bin').read_bytes(); require(raw == (args.backup/'B.bin').read_bytes(), 'Independent backups differ')
    require(len(raw)==0x8000000 and sha(raw)==manifest['comparison']['sha256_a']==manifest['comparison']['sha256_b'], 'Backup manifest hash differs')
    plan, after, payload = make_plan(raw, manifest['identity']['uid_words'])
    out = args.output; out.mkdir(parents=True, exist_ok=False)
    (out/'plan.json').write_text(json.dumps(plan, indent=2)); (out/'payload.bin').write_bytes(payload)
    for i, r in enumerate(plan['regions']):
        o, n = r['offset'], r['length']; (out/f'{i}-before.bin').write_bytes(raw[o:o+n]); (out/f'{i}-after.bin').write_bytes(after[o:o+n])
    changed=[]
    for off in range(4096, SAFE_END, 4096):
        if raw[off:off+4096] != after[off:off+4096]:
            d=out/'sectors';d.mkdir(exist_ok=True);(d/f'{off:08x}-before.bin').write_bytes(raw[off:off+4096]);(d/f'{off:08x}-after.bin').write_bytes(after[off:off+4096]);changed.append(off)
    (out/'changed-sectors.json').write_text(json.dumps(changed)); print('Planned',len(changed),'sectors; free bytes',plan['free_bytes_before'],flush=True)
    if args.action == 'plan': return
    require(args.elf and args.serial, 'Explicit ELF and target required')
    from bringup import Board
    from storage_swd_backup import LiveMemory, mailbox_symbol, read_segment
    board=Board(args.serial,out,100);board.read_frequency_khz=args.read_khz
    mem=LiveMemory(args.serial,out,mailbox_symbol(args.elf),args.read_khz,240)
    descriptor=mem.descriptor();identity=mem.verify_app(args.elf)
    require([descriptor[f'uid{i}'] for i in range(3)]==plan['uid_words'],'Wrong donor')
    evidence=dict(state='preflight',identity=identity,before=[],after=[])
    def save(): (out/'result.json').write_text(json.dumps(evidence,indent=2))
    save();seq=descriptor['last_request_seq']
    for r in plan['regions']:
        seq+=1;path,detail=read_segment(mem,r['offset'],r['length'],seq,120,3)
        require(sha(path.read_bytes())==r['before_sha256'],'Live FAT/free clusters changed; no write')
        evidence['before'].append(detail);save()
    nm=subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),'-S',str(args.elf)],text=True)
    symbols={p[3]:(int(p[0],16),int(p[1],16)) for line in nm.splitlines() if len(p:=line.split())==4}
    address,size=symbols['g_cfw_install'];require(size==72,'Installer ABI mismatch');counter=0
    def readbox():
        nonlocal counter
        counter+=1;path=out/f'status-{counter:04d}.bin';board.command(path.stem,'-u',hex(address),hex(size),str(path))
        b=struct.unpack('<18I',path.read_bytes());require(b[:2]==(0x31494643,1),'Wrong installer');return b
    b=readbox();require(b[3]==sum(SIZES) and b[13]==b[14] and b[15]!=1,'Installer busy/size')
    require(0xc0010000<=b[2]<=0xc4000000-len(payload),'Invalid SDRAM arena')
    board.command('upload-payload','-w',str(out/'payload.bin'),hex(b[2]))
    require(mem.upload('payload-readback',b[2],len(payload)).read_bytes()==payload,'Input mismatch')
    request=(b[13]+1)&0xffffffff or 1
    fields=[a['first_cluster'] for a in plan['files']]+[a['root_index'] for a in plan['files']]+[plan['metadata_crc'],plan['payload_crc'],0x42414b32]
    command=['-w32',hex(address+52),'0']
    for i,value in enumerate(fields): command+=['-w32',hex(address+16+i*4),hex(value)]
    command+=['-w32',hex(address+52),hex(request)]
    evidence['state']='writing';save();board.command('install-request',*command)
    deadline=time.monotonic()+360
    while True:
        b=readbox()
        if b[14]==request:break
        require(time.monotonic()<deadline,'Install timed out; inspect journal before any retry');time.sleep(.2)
    require(b[16]==0,f'Device rejected/failed: {b[16]}')
    evidence['state']='readback';save()
    for r in plan['regions']:
        seq+=1;path,detail=read_segment(mem,r['offset'],r['length'],seq,120,3)
        require(sha(path.read_bytes())==r['after_sha256'],'Unexpected mutation');evidence['after'].append(detail);save()
    # A fresh complete A/B backup is still required for acceptance of unchanged
    # regions. These bounded readbacks establish only the planned changed ranges.
    evidence['state']='regions_verified';evidence['requires_full_backup_and_reboot']=True;save()
    print('Planned regions verified. Reboot and full A/B comparison remain required.')

if __name__=='__main__':main()
