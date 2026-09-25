"""Explicit APP-only bench installation with exact preimage/readback evidence.

This is the wired bench entry/recovery tool, not evidence of stock SPP OTA.
The caller supplies the current full internal-flash snapshot; no fallback,
erase-all, option-byte write, or automatic retry/restore exists.
"""
from pathlib import Path
import argparse, hashlib, json, struct, time, subprocess
from bringup import Board
from validate_image import Elf32, validate
from bootstrap_build import TC
from verified_flash import read_expected

BL_SHA='f8b379c3fac078a8e01d8b6c36fccc5bb5ea5852a0db008822ef6871e0f38df5'
def sha(b): return hashlib.sha256(b).hexdigest()
def require(ok,msg):
    if not ok: raise RuntimeError(msg)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--elf',type=Path,required=True)
    p.add_argument('--expected-current',type=Path,required=True)
    p.add_argument('--uid',type=lambda s:int(s,0),nargs=3,required=True)
    p.add_argument('--serial',required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--read-khz',type=int,default=950)
    a=p.parse_args()
    image,layout=validate(Elf32(a.elf.read_bytes()))
    require(layout.get('layout_version',1)==1,'Layout2 Product requires a gate-aware installation; never write it at08010000')
    image=image.ljust(0x70000,b'\xff')
    before=a.expected_current.read_bytes()
    require(len(before)==0x80000,'Expected current snapshot must be full512KiB')
    require(sha(before[:0x8000])==BL_SHA,'Resident BL differs from approved version')
    require(struct.unpack_from('<I',before,0x8000)[0]==0x000e0000,'Resident metadata version differs')
    require(struct.unpack_from('<I',before,0x8010)[0]==0,'Pending stock installer metadata; do not overwrite APP')
    # validate_image reports a selected diagnostics symbol list, not every
    # function. Inspect the real ELF symbol table for the recovery entry.
    listing=subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),'-S','--defined-only',str(a.elf)],text=True)
    if any(line.split()[-1:] == ['AppRecovery_EarlyRun'] for line in listing.splitlines()):
        intents=[v for line in listing.splitlines() if len(v:=line.split())==4 and v[3]=='intent']
        require(len(intents)==1,'Missing/ambiguous retained recovery intent')
        addr,n=int(intents[0][0],16),int(intents[0][1],16)
        require(n==16 and 0x20006a00<=addr<=0x20030000-16 and not addr%8,'Recovery intent overlaps resident BL scratch or invalid RAM')
    out=a.output;out.mkdir(parents=True,exist_ok=False)
    (out/'candidate.bin').write_bytes(image)
    result=dict(state='preflight',elf=str(a.elf.resolve()),candidate_sha256=sha(image),before_sha256=sha(before),wireless_test=False)
    def save(): (out/'result.json').write_text(json.dumps(result,indent=2))
    save();board=Board(a.serial,out,100);board.read_frequency_khz=a.read_khz
    board.prepare()
    uid=board.dump('uid',0x1fff7a10,12)
    require(list(struct.unpack('<3I',uid))==a.uid,'MCU UID mismatch')
    current=read_expected(board,'before-full',0x08000000,before)
    require(current==before,'Current flash differs from explicit preimage; no write')
    result['state']='programming';save();board.program(out/'candidate.bin')
    result['state']='readback';save()
    after=read_expected(board,'after-full',0x08000000,before[:0x10000]+image)
    require(after[:0x10000]==before[:0x10000],'Preserved lower64KiB changed')
    require(after[0x10000:]==image,'Complete APP readback mismatch')
    result.update(state='verified',after_sha256=sha(after),preserved_sha256=sha(after[:0x10000]),
        before_pages_reconfirmed=(out/'before-full-verification.json').exists(),
        after_pages_reconfirmed=(out/'after-full-verification.json').exists(),
        after_verified_image=str((out/('after-full-verified-a.bin' if (out/'after-full-verification.json').exists() else 'after-full.bin')).resolve()))
    save()
    board.command('reset-through-stock-bl','-w32','0xE0042008',hex(board.freeze_before),'-rst','-run')
    time.sleep(3)
    result['state']='installed_readback_verified_runtime_unchecked';save()
    print(json.dumps(result,indent=2))

if __name__=='__main__':main()
