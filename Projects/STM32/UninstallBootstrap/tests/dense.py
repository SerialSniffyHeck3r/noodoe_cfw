"""Non-erased file/slack coverage; restart near the end of physical erasure."""
from run import *
out=P.parents[1]/'analysis/2026-09-23-uninstall/tests-dense'
elf,sym=build(out);pages,ranges,meta=fixture()
for address,size in ranges:
 for a in range(address,address+size,4096):
  if a not in pages:pages[a]=bytes([0x69])*4096
base=Model(elf,sym,pages);base.run()
for a,n in ranges:assert base.read(a,n)==FF*(n//4096)
for a,b in pages.items():
 if a>=0x9000 and not any(x<=a<x+n for x,n in ranges):assert base.pages.get(a,FF)==b
cuts=[i for i,m in enumerate(base.mutations) if m[0]=='TestErase' and any(m[1]==a+n-4096 for a,n in ranges)]
rows=[]
for cut in cuts:
 m=Model(elf,sym,pages,cut=cut,tear=True)
 try:m.run();raise AssertionError('cut not reached')
 except PowerCut:pass
 resumed=Model(elf,sym,m.pages,m.journal);resumed.run()
 for a in set(base.pages)|set(resumed.pages):assert base.pages.get(a,FF)==resumed.pages.get(a,FF)
 rows.append({'cut':cut,'result':'pass'})
 print(json.dumps(rows[-1]),flush=True)
(out/'results.json').write_text(json.dumps({'hardware':False,'erased_bytes':sum(n for a,n in ranges),'mutations':len(base.mutations),'cuts':rows},indent=2))
