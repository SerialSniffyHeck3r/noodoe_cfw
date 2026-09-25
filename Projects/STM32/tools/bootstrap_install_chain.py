"""Reconstruct audited create-only changes after a real full A/B backup.

This is an expected postimage, never a new downloaded backup. Completed chains
carry two physical whole-NOR hash sweeps. An explicitly pending initial A/B/BOOT
batch may carry region proofs between steps, but cannot authorize APP install;
two whole-NOR sweeps over the final combined postimage remain mandatory.
"""
from pathlib import Path
import json
from bootstrap_storage import create_plan
from resource_install import require, sha

KINDS = {'recovery': 4, 'gate-a': 5, 'gate-b': 6, 'gate-journal': 7}

def apply_install(raw, uid, directory, require_global=True):
    """Re-derive one FAT allocation; never trust a supplied arbitrary delta."""
    directory = Path(directory)
    result = json.loads((directory / 'result.json').read_text())
    require(result['state'] == 'regions_verified', 'Previous install is incomplete')
    require(result['identity']['uid_words'] == uid, 'Install chain UID mismatch')
    kind = KINDS[result['kind']]
    payload = (directory / 'payload.bin').read_bytes()
    plan, after = create_plan(raw, kind, payload, uid=uid)
    require(json.loads((directory / 'plan.json').read_text()) == plan,
            'Install chain plan/preimage mismatch')
    require(result['expected_whole_nor_sha256'] == sha(after), 'Install result postimage mismatch')
    regions = json.loads((directory / 'regions.json').read_text())
    bounds = [(0, 0x9000), (plan['address'], len(payload))]
    require([(r['offset'], r['length']) for r in regions] == bounds, 'Install changed range mismatch')
    changed = []
    for r in regions:
        off, n = r['offset'], r['length']
        require(r['before_sha256'] == sha(raw[off:off+n]) and
                r['after_sha256'] == sha(after[off:off+n]), 'Install region digest mismatch')
        for p in range(off, off+n, 4096):
            if raw[p:p+4096] == after[p:p+4096]:
                continue
            changed.append(p)
            require((directory / f'{p:08x}-before.bin').read_bytes() == raw[p:p+4096] and
                    (directory / f'{p:08x}-after.bin').read_bytes() == after[p:p+4096],
                    'Install sector journal mismatch')
    require(json.loads((directory / 'changed-sectors.json').read_text()) == changed,
            'Install sector journal incomplete')
    require(len(result['after']) == len(regions), 'Physical region readback missing')
    for observed, expected in zip(result['after'], regions):
        require(observed['offset'] == expected['offset'] and observed['length'] == expected['length'] and
                observed['sha256'] == expected['after_sha256'], 'Physical region readback mismatch')
    verification = None
    if require_global:
        verification = json.loads((directory / 'whole-nor-verification/result.json').read_text())
        require(verification['state'] == 'two_independent_device_hashes_match_expected' and
                verification['identity']['uid_words'] == uid and
                verification['expected_sha256'] == sha(after), 'Whole-NOR chain proof mismatch')
        passes = verification['passes']
        require(len(passes) == 2 and passes[0]['sequence'] != passes[1]['sequence'] and
                all(p['state'] == 8 and p['error'] == 0 and p['bytes'] == 0x8000000 and
                    p['sha256'] == sha(after) for p in passes), 'Two independent whole-NOR sweeps required')
    return after, dict(directory=str(directory.resolve()), kind=result['kind'],
                       before_sha256=sha(raw), after_sha256=sha(after),
                       physical_whole_nor_verified=verification is not None)

def apply_chain(raw, uid, directories):
    records = []
    for directory in directories:
        raw, record = apply_install(raw, uid, directory)
        records.append(record)
    return raw, records

def apply_pending_batch(raw, uid, directories, next_kind):
    """Only the three initial gate files can share one unfinished allocation batch.

    Every following device write still checks live FAT, all existing CFW files
    and the destination's physical preimage. Unrelated file creation/replacement
    cannot be chained under this flag. Final global verification is mandatory.
    """
    order=('gate-a','gate-b','gate-journal')
    require(next_kind in order and len(directories)==order.index(next_kind),
            'Pending gate batch is missing or reorders an initial file')
    records=[]
    for expected,directory in zip(order,directories):
        raw,record=apply_install(raw,uid,directory,require_global=False)
        require(record['kind']==expected,'Pending gate batch has an unrelated or reordered file')
        records.append(record)
    return raw,records

def apply_resource_update(raw, directory):
    """Adopt only a verified update of the previously empty resource B slot.

    The old valid A package, FAT, all unrelated files and original staging stay
    byte-identical. This expected image still requires the final global sweeps.
    """
    from resource_install import make_plan, Fat
    from cfw_storage_install import file_bytes
    directory=Path(directory);result=json.loads((directory/'result.json').read_text())
    require(result['state']=='verified','Resource update lacks physical readback')
    recorded=json.loads((directory/'plan.json').read_text())
    require(recorded['command']==2 and recorded['slot']==1,'Only existing resource B update is allowed')
    previous=file_bytes(Fat(raw),'NOODOE.RSC')
    require(previous[0x80000:]==b'\xff'*0x80000,'Resource B was not unused')
    plan,after=make_plan(raw,(directory/'prepared.bin').read_bytes(),2,1)
    recorded=dict(recorded);recorded.pop('source_provenance',None)
    require(recorded==plan,'Resource update plan/base mismatch')
    require(len(result['after'])==len(plan['regions']),'Resource physical readback incomplete')
    for i,(region,observed)in enumerate(zip(plan['regions'],result['after'])):
        off,n=region['offset'],region['length']
        require((directory/f'{i}-before.bin').read_bytes()==raw[off:off+n] and
                (directory/f'{i}-after.bin').read_bytes()==after[off:off+n], 'Resource region journal mismatch')
        require(observed['offset']==off and observed['length']==n and observed['sha256']==sha(after[off:off+n]),
                'Resource physical readback mismatch')
    return after,dict(directory=str(directory.resolve()),before_sha256=sha(raw),after_sha256=sha(after),
                      old_slot_preserved=True,new_slot=1,global_verification_pending=True)
