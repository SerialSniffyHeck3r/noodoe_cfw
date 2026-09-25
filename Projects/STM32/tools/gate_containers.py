"""Validate canonical initial gate files; pure byte operations, no device I/O.

This format check is not authorization. Installation also requires a current
UID-bound backup, exact live preimages, and independently checked Product ELF
budgets. Runtime journal/image replacement remains owned by firmware.
"""
import hashlib,struct,zlib
from resource_install import require
IMAGE_BYTES=0x60000
CONTAINER_BYTES=0x80000
IDENTITY_OFFSET=0x7f000
JOURNAL_BYTES=0x10000
COMMIT=0x31544d43
def words(data,offset,count=1):return struct.unpack_from('<'+'I'*count,data,offset)
def record(data,magic,reserved,version=1):
    require(len(data)==4096,'Gate record size')
    require(words(data,0,2)==(magic,version),'Gate record magic/version')
    require(words(data,4088,2)==(zlib.crc32(data[:4088]),COMMIT),'Gate record CRC/commit')
    require(data[reserved:4088]==b'\xff'*(4088-reserved),'Gate reserved bytes changed')
def check_container(data,kind,uid):
    """Kinds5/6/7 mirror BootstrapStorage without accepting arbitrary names."""
    require(len(uid)==3 and all(0<=v<=0xffffffff for v in uid) and any(uid),'Observed target UID required')
    uid=tuple(uid)
    if kind in (5,6):
        require(len(data)==CONTAINER_BYTES,'Gate image container size')
        h=data[:4096];identity=data[IDENTITY_OFFSET:]
        record(h,0x314d4947,128);record(identity,0x31444947,32)
        require(words(h,8,2)==(4096,CONTAINER_BYTES) and words(h,16,3)==uid,'Gate image header/UID')
        require(words(h,28,2)==(0x08020000,IMAGE_BYTES) and words(h,36)[0]>0,'Gate image target/generation')
        require(words(h,72,3)==(0x51534352,1,1) and words(h,116)[0]==1 and words(h,124)[0]==0,'Gate image resource requirement/ABI')
        require(words(identity,8,2)==(kind-5,CONTAINER_BYTES) and words(identity,16,3)==uid and words(identity,28)[0]==1,'Gate immutable identity/type/UID')
        image=data[4096:4096+IMAGE_BYTES]
        require(hashlib.sha256(image).digest()==h[40:72] and image[0x200:0x22c]==h[72:116],'Gate image hash/resource identity')
        sp,reset=words(image,0,2)
        require(sp==0x2002ff00 and reset&1 and 0x08020000<=reset<0x08080000,'Gate Product vector')
        require(data[4096+IMAGE_BYTES:IDENTITY_OFFSET]==b'\xff'*(IDENTITY_OFFSET-4096-IMAGE_BYTES),'Gate image padding')
        return dict(kind=kind,uid=list(uid),image_sha256=h[40:72].hex(),generation=words(h,36)[0],version=words(h,120)[0],resource_sha256=h[84:116].hex())
    if kind==8:
        require(len(data)==0x40000,'Log file size')
        h=data[0x3f000:];record(h,0x31494c4e,28)
        require(words(h,8,2)==(0x40000,64) and words(h,16,3)==uid,'Log identity UID/size')
        require(data[:0x3f000]==b'\xff'*0x3f000,'Initial log must be empty')
        return dict(kind=kind,uid=list(uid))
    require(kind==7 and len(data)==JOURNAL_BYTES,'Gate journal kind/size')
    h=data[:4096];record(h,0x314a4247,184,2)
    require(words(h,12,5)==(1,0,0xffffffff,0,0),'Initial journal is unconfirmed, without candidate/attempts')
    require(words(h,120,3)==(1,0xffffffff,0) and h[132:184]==bytes(52),'Initial trial has no previous confirmed CFW')
    require(words(h,96,3)==uid and words(h,116)[0]==1,'Gate journal UID/ABI')
    require(words(h,8)[0]>0 and words(h,108)[0]>0 and words(h,112)[0]==0,'Initial journal sequence/generation')
    require(h[64:96]==bytes(32) and data[4096:]==b'\xff'*(JOURNAL_BYTES-4096),'Initial journal unused records/candidate')
    return dict(kind=kind,uid=list(uid),image_sha256=h[32:64].hex(),generation=words(h,108)[0])
