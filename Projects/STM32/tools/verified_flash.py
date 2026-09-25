"""Bounded rereads of inconsistent immutable flash pages; never synthesize bytes.

Keep original dumps and every reread. A replacement page must come from TWO
matching fresh device reads. Known-image checks additionally require those
actual bytes to equal the expected page. No flash programming exists here.
"""
import hashlib, json
PAGE=4096

def reconcile(board, name, address, first, second, expected=False):
    if not first or len(first)!=len(second) or not 0x08000000<=address<address+len(first)<=0x08080000:
        raise RuntimeError('Invalid immutable flash comparison range')
    a,b=bytearray(first),bytearray(second)
    repaired=[]
    for offset in range(0,len(first),PAGE):
        end=min(offset+PAGE,len(first))
        if a[offset:end]==b[offset:end]:continue
        # A persistent mismatch is a failure. There is no majority-bit mixing,
        # threshold widening or unbounded retry until an expected result appears.
        c=board.dump(f'{name}-page-{offset:06x}-c',address+offset,end-offset,resume=False)
        d=board.dump(f'{name}-page-{offset:06x}-d',address+offset,end-offset,resume=False)
        if len(c)!=end-offset or c!=d or (expected and c!=second[offset:end]):
            raise RuntimeError(f'Flash page {address+offset:#x} did not match two fresh reads/reference')
        if not expected and c not in (first[offset:end],second[offset:end]):
            raise RuntimeError(f'Flash page {address+offset:#x} changed or has no repeatable original observation')
        a[offset:end]=c
        b[offset:end]=d
        repaired.append(dict(offset=offset,size=end-offset,sha256=hashlib.sha256(c).hexdigest()))
    if a!=b:raise RuntimeError('Verified flash reconstruction mismatch')
    if repaired:
        (board.folder/f'{name}-verified-a.bin').write_bytes(a)
        (board.folder/f'{name}-verified-b.bin').write_bytes(b)
        (board.folder/f'{name}-verification.json').write_text(json.dumps(dict(
            address=hex(address),size=len(a),expected_reference=expected,repairs=repaired,
            first_sha256=hashlib.sha256(first).hexdigest(),second_sha256=hashlib.sha256(second).hexdigest(),
            verified_sha256=hashlib.sha256(a).hexdigest()),indent=2))
    return bytes(a),bytes(b)

def read_expected(board,name,address,expected,resume=False):
    raw=board.dump(name,address,len(expected),resume=False)
    actual,_=reconcile(board,name,address,raw,expected,expected=True)
    if resume:board.resume(f'{name}-resume')
    return actual
