"""Actual ARM key DB + Bootstrap journal, mocked NOR; no device access."""
from pathlib import Path
import argparse
parser=argparse.ArgumentParser();parser.add_argument('--opt',choices=['O0','Os'],default='Os');options=parser.parse_args()
TEST=Path(__file__).resolve().parent;HERE=TEST.parent/'bootstrap_storage';DEST=TEST/('output-'+options.opt);DEST.mkdir(exist_ok=True)
(DEST/'FreeRTOS.h').write_text('#pragma once\n#define taskENTER_CRITICAL() do{}while(0)\n#define taskEXIT_CRITICAL() do{}while(0)\n')
(DEST/'task.h').write_text('#include "FreeRTOS.h"\n')
code=(HERE/'fast_run.py').read_text().split('u=machine(raw)')[0]
code=code.replace("OUT=HERE/'fast-output'",'OUT=DEST').replace("opt='-Os'","opt='-"+options.opt+"'")
code=code.replace('order=[4,0,1,2,3,8,5,6,7]','order=[1]')
extra="""
sources += [P/'Middlewares/Noodoe/Storage/src/bootstrap_bond.c',P/'Middlewares/Noodoe/Bluetooth/src/bluetooth_keys.c',P/'Middlewares/Noodoe/Bluetooth/src/bluetooth_key_codec.c',TEST/'test.c']
setup=setup.replace("    args+=list(map(str,sources))","    args+=['-DNOODOE_BOOTSTRAP=1','-I',str(DEST),'-I',str(P/'Middlewares/Noodoe/Bluetooth/inc'),'-I',str(P/'Middlewares/Third_Party/BTstack/src')]\\n    args+=list(map(str,sources))")
setup=setup.replace('-u,GateReader','-u,GateReader,-u,BondStart,-u,BondStep,-u,BondError,-u,BondRestore,-u,BondRekey')
"""
code=code.replace("exec(compile('\\n'.join",extra+"\nexec(compile('\\n'.join")
__file__=str(HERE/'fast_run.py')
exec(compile(code,__file__,'exec'))
original_machine=machine
def machine(blob):
 m=original_machine(blob)
 def crc(u,pc,size,data):
  a=u.reg_read(UC_ARM_REG_R0);n=u.reg_read(UC_ARM_REG_R1)
  u.reg_write(UC_ARM_REG_R0,zlib.crc32(u.mem_read(a,n)));u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
 m.hook_add(UC_HOOK_CODE,crc,begin=syms['Noodoe_Crc32'],end=syms['Noodoe_Crc32']);return m
rows=[]
base=expected
def disk(m):return bytes(m.mem_read(0x90000000,0x8000000))
def restore(blob):
 m=machine(blob);e=call(m,'BondRestore');return e,bytes(m.mem_read(0x11000000,152))
def records(blob):
 data=swapped(blob[first[1]:first[1]+131072]);valid=[]
 for off in range(0,131072,4096):
  p=data[off:off+4096]
  if struct.unpack_from('<I',p,4092)[0]==0x31544d43 and zlib.crc32(p[:4088])==struct.unpack_from('<I',p,4088)[0]:valid.append((struct.unpack_from('<I',p,12)[0],off,p))
 return valid
for cut in range(2,22):
 m=machine(base);assert call(m,'BondStart')==0
 while call(m,'BondStep',1)==1:pass
 # Phase 2 means full ownership/content inspection has completed, no erase.
 for _ in range(cut-2):call(m,'BondStep',1)
 saved=disk(m);rs=records(saved);assert len(rs)>=1
 assert any(g==0 for g,_,_ in rs),'old valid record lost'
 e,keys=restore(saved);assert e==0
 assert saved[:first[1]]==base[:first[1]] and saved[first[1]+131072:]==base[first[1]+131072:]
 rows.append({'power_cut_phase':cut,'valid_records':len(rs)})
 print('cut',cut,'passed',flush=True)
m=machine(base);assert call(m,'BondStart')==0;assert call(m,'BondStep',10000)==0;assert call(m,'BondError')==0
saved=disk(m);e,keys=restore(saved);assert e==0 and keys[8:14]==bytes([1,2,3,4,5,6]) and keys[14:30]==b'Z'*16
assert len(records(saved))==2
# Duplicate saves preserve sectors; a fresh bond while writing requires a new
# journal record before completion, not a stale-key success.
m=machine(saved);assert call(m,'BondRestore')==0;assert call(m,'BondStart')==0;assert call(m,'BondStep',10000)==0;assert disk(m)==saved
m=machine(base);assert call(m,'BondStart')==0
while call(m,'BondStep',1)!=20:pass
call(m,'BondRekey');assert call(m,'BondStep',10000)==0;e,keys=restore(disk(m));assert not e and keys[14:30]==b'\xa5'*16
# Schema/UID/CRC corruption is refused, never reformatted.
for label,offset,value in [('future schema',4,2),('wrong UID',20,99),('bad CRC',4088,0)]:
 bad=bytearray(swapped(base));p=bytearray(bad[first[1]:first[1]+4096]);struct.pack_into('<I',p,offset,value)
 if offset!=4088:struct.pack_into('<I',p,4088,zlib.crc32(p[:4088]))
 bad[first[1]:first[1]+4096]=p;bad=bytes(swapped(bad));m=machine(bad);assert call(m,'BondStart')==0;call(m,'BondStep',10000);assert call(m,'BondError')!=0;assert disk(m)==bad
 rows.append({'rejected':label})
# Preserve unrelated configuration and older peers; fresh RAM key wins.
old_keys=bytearray(152);struct.pack_into('<II',old_keys,0,1,20)
old_keys[8:32]=bytes([1,2,3,4,5,6])+b'X'*16+bytes([4,1])
old_keys[32:56]=bytes([6,5,4,3,2,1])+b'Y'*16+bytes([4,1])
fields={0x201:b'Rider',0x202:bytes(old_keys),0x1002:struct.pack('<I',25),0x5432:b'future field'}
payload=struct.pack('<II',0x31474643,1)+b''.join(struct.pack('<HH',k,len(v))+v for k,v in fields.items())
configured=bytearray(swapped(base));configured[first[1]:first[1]+4096]=record(1,[1,2,3],payload,7);configured=bytes(swapped(configured))
m=machine(configured);assert call(m,'BondStart')==0;assert call(m,'BondStep',10000)==0;assert not call(m,'BondError')
latest=max(records(disk(m)))[2];n=struct.unpack_from('<I',latest,16)[0];p=latest[64:64+n];result={};o=8
while o<len(p):k,n=struct.unpack_from('<HH',p,o);result[k]=p[o+4:o+4+n];o+=n+4
assert {k:v for k,v in result.items() if k!=0x202}=={k:v for k,v in fields.items() if k!=0x202}
assert result[0x202][14:30]==b'Z'*16 and result[0x202][38:54]==b'Y'*16
# A fragmented file crosses to a non-adjacent next journal cluster.
fs=Fat(base);cluster=(first[1]-0x9000)//32768+2;far=1000;far_address=0x9000+(far-2)*32768
fs.b[far_address:far_address+32768]=fs.b[first[1]+32768:first[1]+65536]
fs.setfat(cluster,far);fs.setfat(far,cluster+2);fs.setfat(cluster+1,0)
fs.b[first[1]+7*4096:first[1]+8*4096]=record(1,[1,2,3],payload,8)
fragmented=bytes(swapped(fs.b));m=machine(fragmented);assert call(m,'BondStart')==0;assert call(m,'BondStep',10000)==0;assert not call(m,'BondError')
saved=disk(m);assert saved[:far_address]==fragmented[:far_address] and saved[far_address+4096:]==fragmented[far_address+4096:]
assert saved[far_address:far_address+4096]!=fragmented[far_address:far_address+4096]
(DEST/'results.json').write_text(json.dumps({'optimization':options.opt,'cases':rows,'round_trip':True,'rekey_during_write':True,'deduplicated':True,'unknown_fields_preserved':True,'fragmented_cfg':True,'hardware':False},indent=2))
print('PASS',options.opt,len(rows),'cut/corruption cases + actual key DB roundtrip/rekey/dedup/fragmented CFG')
