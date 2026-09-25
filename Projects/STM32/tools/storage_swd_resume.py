"""Continue an interrupted independent B capture without discarding its evidence.

Only an exact, CRC/SHA-validated B prefix may be appended. A is never used as
the source of B bytes. The original manifest is archived before continuation;
full independent A/B comparison is still mandatory before verified=True.
Device access uses storage_swd_backup's read-only NOR mailbox contract.
"""
from __future__ import annotations
import argparse
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import time
import zlib

import storage_swd_backup as backup


def validate_segments(path: Path, segments: list[dict], length: int) -> str:
    """Reject gaps, tails, reordered blocks, damaged files and inconsistent CRC evidence."""
    backup.require(path.is_file() and path.stat().st_size == length,
                   'Capture length differs from committed segment evidence')
    digest = hashlib.sha256()
    offset = 0
    with path.open('rb') as source:
        for segment in segments:
            count = segment['length']
            backup.require(segment['offset'] == offset and 0 < count <= backup.BUFFER_BYTES
                           and count % 4096 == 0, 'Invalid/noncontiguous committed segment')
            data = source.read(count)
            crc = zlib.crc32(data)
            response = segment['committed_response']
            backup.require(len(data) == count and crc == segment['crc32'] == response['crc32']
                           and hashlib.sha256(data).hexdigest() == segment['sha256'],
                           'Committed capture bytes fail CRC/SHA')
            backup.require(response['state'] == 2 and response['result'] == 0
                           and response['response_offset'] == offset
                           and response['response_length'] == response['completed'] == count
                           and response['active_seq'] == response['response_seq'] == segment['sequence'],
                           'Committed firmware response does not describe this segment')
            backup.require(any(read.get('match') is True and read.get('crc32') == crc
                               for read in segment['reads']), 'Missing successful independent read')
            digest.update(data)
            offset += count
        backup.require(offset == length and not source.read(1), 'Uncommitted bytes/gap in capture')
    return digest.hexdigest()


def validate_resume(directory: Path, manifest: dict) -> tuple[Path, list[dict], int]:
    """Finish local integrity checks before issuing any device command or changing files."""
    backup.require(manifest.get('state') in ('failed','resume_failed') and not manifest.get('verified'),
                   'Only an explicitly failed, unverified capture may resume')
    captures = manifest['captures']
    backup.require(len(captures) == 1 and captures[0]['terminal_done_validated']
                   and captures[0]['length'] == backup.CAPACITY, 'Need one completed A capture')
    first = directory / 'A.bin'
    backup.require(first.resolve() == Path(captures[0]['path']).resolve(), 'Unexpected A path')
    backup.require(validate_segments(first, captures[0]['segments'], backup.CAPACITY)
                   == captures[0]['sha256'], 'Completed A hash differs')
    progress = manifest['progress']
    length = progress['received']
    backup.require(progress['capture'] == 'B' and 0 < length <= backup.CAPACITY
                   and not (directory / 'B.bin').exists(), 'Need an incomplete independent B capture')
    partial = directory / 'B.partial'
    backup.require(not os.path.samefile(first, partial), 'A/B must be independent files')
    validate_segments(partial, progress['segments'], length)
    return partial, progress['segments'].copy(), length


def journal_expectations(raw: bytes, uid: list[int]) -> tuple[list[dict],list[tuple[int,int]]]:
    """Read persistent generation/sector from A, never infer it from live RAM."""
    from resource_install import Fat
    from cfw_storage_install import file_bytes
    fs=Fat(raw);expected=[];regions=[(0,0x9000)]
    for purpose,name in enumerate(('CFWCFG.DAT','CFWRIDE.DAT'),1):
        backup.require(name in fs.files,'Resume continuity needs allocated CFW journals')
        b=file_bytes(fs,name);latest=None
        for off in range(0,len(b),4096):
            rec=b[off:off+4096];words=struct.unpack_from('<8I',rec)
            if words[0]!=0x314a4643 or words[1]!=1 or words[2]!=purpose or words[4]>3968 or list(words[5:])!=uid:continue
            if struct.unpack_from('<2I',rec,4088)!=(zlib.crc32(rec[:4088]),0x31544d43):continue
            gen=words[3]
            if latest is None or 0<((gen-latest['generation'])&0xffffffff)<0x80000000:latest=dict(generation=gen,active_sector=off//4096)
        backup.require(latest is not None,'No valid original journal generation')
        expected.append(latest)
    for name in ('CFWCFG.DAT','CFWRIDE.DAT','CFWPIC.DAT','NOODOE.RSC','CFWREC.DAT'):
        if name not in fs.files:continue
        for c in fs.files[name][0]:
            address=fs.address(c)
            if regions[-1][0]+regions[-1][1]==address:regions[-1]=(regions[-1][0],regions[-1][1]+32768)
            else:regions.append((address,32768))
    return expected,regions


def journal_snapshot(memory: backup.LiveMemory, elf: Path) -> list[dict]:
    """Read only a size-checked symbol from the already matched live ELF."""
    backup.require(memory.app_verified,'Exact live APP must be verified first')
    listing=subprocess.check_output([str(backup.NM),'-S','--defined-only',str(elf)],text=True)
    matches=[p for line in listing.splitlines() if len(p:=line.split())==4 and p[3]=='g_cfw_store']
    backup.require(len(matches)==1 and int(matches[0][1],16)==96,'Journal diagnostic ABI')
    address=int(matches[0][0],16);backup.require(0x20000000<=address<=0x20030000-96,'Journal diagnostic outside SRAM')
    path=memory.directory/f'{memory.counter+1:05d}-journal-state.bin'
    memory._command('journal-state',['-u',hex(address),'96',str(path)],True)
    data=path.read_bytes();backup.require(len(data)==96,'Truncated journal diagnostics')
    w=struct.unpack('<24I',data);backup.require(w[:4]==(0x31534643,1,1,0),'Journals not initialized/healthy')
    return [dict(generation=w[4+10*f+2],active_sector=w[4+10*f+3],writes=w[4+10*f+7]) for f in range(2)]


def continuity(memory,elf,manifest,directory):
    """Prove persisted files still equal A, then retain stable journal counters.

    This is additional continuity evidence, not a replacement for independently
    reading the rest of B and comparing the full two128MiB files afterwards.
    """
    raw=(directory/'A.bin').read_bytes();expected,regions=journal_expectations(raw,manifest['identity']['uid_words'])
    before=journal_snapshot(memory,elf)
    backup.require(all(all(before[i][k]==v for k,v in e.items()) for i,e in enumerate(expected)),'Journal generation changed since A')
    evidence=[];seq=memory.descriptor()['last_request_seq']
    for off,n in regions:
        seq=(seq+1)&0xffffffff or 1
        path,detail=backup.read_segment(memory,off,n,seq,180,3)
        backup.require(path.read_bytes()==raw[off:off+n],'Live FAT/CFW data differs from original A; restart independent backup')
        evidence.append(detail)
    backup.require(journal_snapshot(memory,elf)==before,'Journal counters changed during continuity validation')
    return dict(expected=expected,diagnostics=before,regions=evidence)


def main() -> None:
    """Revalidate identity/APP, resume B, then compare the complete independently read files."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', type=Path, required=True)
    parser.add_argument('--serial', required=True)
    parser.add_argument('--elf', type=Path, required=True)
    parser.add_argument('--read-khz', type=int, default=950)
    parser.add_argument('--adopt-quiesced-lease',action='store_true',help='Explicitly take over a still-held drained lease after the old client has terminated')
    args = parser.parse_args()
    directory = args.directory.resolve()
    manifest_path = directory / 'manifest.json'
    manifest = json.loads(manifest_path.read_text())
    partial, segments, received = validate_resume(directory, manifest)
    if args.adopt_quiesced_lease:
        backup.require(manifest.get('writes_quiesced') is True and manifest.get('write_lease_released') is not True,
                       'Prior evidence does not retain a quiesced lease')
    backup.require(manifest['stlink_serial'] == args.serial, 'ST-LINK serial differs')
    _, _, image = backup.elf_app_image(args.elf)
    backup.require(image['binary_sha256'] == manifest['app_verification']['canonical_sha256'],
                   'Resume ELF differs from the original running image')
    stamp = dt.datetime.now(dt.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    output = directory / ('resume-' + stamp)
    output.mkdir(exist_ok=False)
    (output / 'prior-manifest.json').write_bytes(manifest_path.read_bytes())
    memory = backup.LiveMemory(args.serial, output, backup.mailbox_symbol(args.elf), args.read_khz, 300)
    attempt = dict(started_utc=stamp, from_bytes=received, read_khz=args.read_khz,
                   evidence_directory=str(output), state='started')
    manifest.setdefault('resumptions', []).append(attempt)
    started = time.monotonic()
    try:
        backup.require(backup.identity(memory) == manifest['identity'], 'Device identity/metadata changed')
        attempt['app_verification'] = memory.verify_app(args.elf)
        attempt['writes_quiesced'] = memory.quiesce(args.elf, True,args.adopt_quiesced_lease)
        attempt['adopted_quiesced_lease']=args.adopt_quiesced_lease
        if attempt['writes_quiesced']:
            attempt['continuity']=continuity(memory,args.elf,manifest,directory)
        manifest.update(state='resuming_B', verified=False)
        backup.save(manifest_path, manifest)
        sequence = memory.descriptor()['last_request_seq']
        with partial.open('ab') as destination:
            for offset in range(received, backup.CAPACITY, backup.BUFFER_BYTES):
                length = min(backup.BUFFER_BYTES, backup.CAPACITY - offset)
                sequence = (sequence + 1) & 0xffffffff or 1
                path, evidence = backup.read_segment(memory, offset, length, sequence, 180, 3)
                destination.write(path.read_bytes())
                destination.flush()
                os.fsync(destination.fileno())
                segments.append(evidence)
                manifest['progress'] = dict(capture='B', received=offset + length, segments=segments)
                backup.save(manifest_path, manifest)
                print(f'B: {offset + length}/{backup.CAPACITY} bytes CRC-validated', flush=True)
        digest = validate_segments(partial, segments, backup.CAPACITY)
        backup.require(backup.identity(memory) == manifest['identity'], 'Final device identity changed')
        if attempt['writes_quiesced']:
            backup.require(journal_snapshot(memory,args.elf)==attempt['continuity']['diagnostics'],'Journal counters changed while resuming')
        comparison = backup.compare(directory / 'A.bin', partial)
        manifest['comparison'] = comparison
        backup.require(comparison['byte_identical'], 'Independent A/B differ; writes remain locked')
        destination = directory / 'B.bin'
        backup.require(not destination.exists(), 'B destination appeared concurrently')
        partial.rename(destination)
        comparison['b'] = str(destination)
        manifest['captures'].append(dict(path=str(destination), address=0, length=backup.CAPACITY,
                                        sha256=digest, terminal_done_validated=True, segments=segments,
                                        resumed=True))
        attempt.update(state='verified', elapsed_seconds=time.monotonic() - started)
        manifest.update(state='verified', verified=True)
        manifest.pop('error', None)
        backup.save(manifest_path, manifest)
        print(json.dumps(comparison, indent=2), flush=True)
    except BaseException as error:
        attempt.update(state='failed', error=f'{type(error).__name__}: {error}')
        manifest.update(state='failed', verified=False, error=attempt['error'])
        backup.save(manifest_path, manifest)
        raise
    finally:
        if memory.quiesce_owned:
            try:
                memory.quiesce(args.elf, False)
                attempt['write_lease_released'] = True
            except BaseException as error:
                manifest.update(state='resume_failed', verified=False, resume_error=str(error))
                backup.save(manifest_path, manifest)
                raise
            backup.save(manifest_path, manifest)


if __name__ == '__main__':
    main()
