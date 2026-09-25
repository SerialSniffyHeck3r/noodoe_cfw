"""Offline installer preservation and legacy migration; no debugger access."""
from pathlib import Path
import sys, struct, zlib, json
H=Path(__file__).resolve().parent;P=H.parents[2]
sys.path.insert(0,str(P/'tools'))
from cfw_storage_install import make_plan, record, Fat, file_bytes, sha, SAFE_END
from cfw_legacy import migrate, LAYOUT
O=P.parents[1]/'analysis/2026-09-18-persistent-store'
uid=[3735583,875974927,892810041]
raw=(O/'offline/before.bin').read_bytes()
plan, after, payload=make_plan(raw,uid)
assert plan['additional_bytes']==1441792 and len(payload)==1441792
assert raw[SAFE_END:]==after[SAFE_END:] and not plan['migration']['imported']
before=Fat(raw);written=Fat(after)
assert all(file_bytes(before,n)==file_bytes(written,n) for n in before.files)
assert all(written.files[a['name']][1]==a['length'] for a in plan['files'])
try:make_plan(after,uid)
except ValueError:pass
else:raise AssertionError('Existing containers accepted for replacement')
source=bytearray(raw)
body=bytearray(312)
struct.pack_into('<4I',body,0,0x31544553,3,312,1)
struct.pack_into('<3I',body,16,*uid);struct.pack_into('<7I',body,28,*LAYOUT)
struct.pack_into('<4I',body,56,1,9,35,1);struct.pack_into('<2I',body,88,1,7)
struct.pack_into('<QI',body,240,123456,1);body[256:261]=b'Rider'
struct.pack_into('<I',body,308,zlib.crc32(body[:308]))
def install_legacy(body,sequence=5):
    b=bytearray(b'\xff'*4096);struct.pack_into('<3I',b,0,0x4e564d31,sequence,len(body))
    struct.pack_into('<I',b,16,~len(body)&0xffffffff);b[20:20+len(body)]=body
    struct.pack_into('<I',b,12,zlib.crc32(b[4:12]+body));struct.pack_into('<I',b,4092,0x434d5431)
    source[0x7f70000:0x7f71000]=b
install_legacy(body)
cfg,ride,info=migrate(source,uid)
assert info['imported'] and info['version']==3 and b'Rider' in cfg
assert struct.unpack_from('<Q',ride,256)[0]==123456
assert struct.unpack_from('<I',ride,296)[0]==1
struct.pack_into('<I',body,4,4);struct.pack_into('<I',body,308,zlib.crc32(body[:308]));install_legacy(body)
try:migrate(source,uid)
except ValueError:pass
else:raise AssertionError('Newer schema overwritten')
report=dict(result='pass',hardware_access=False,existing_files=len(before.files),
            preserved_nonempty=len(plan['existing_file_sha256']),additional_bytes=len(payload),
            free_bytes=plan['free_bytes_before'],planned_after_sha256=sha(after),
            cases=['all existing files byte identical','reserved tail byte identical','fixed containers only',
                   'same-name rejected','UID/layout/CRC legacy migration','unknown newer schema rejected'])
(O/'host-install-tests.json').write_text(json.dumps(report,indent=2));print(json.dumps(report))
