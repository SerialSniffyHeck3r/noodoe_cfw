"""Extra ownership and reported-write-failure tests, separate from reset cuts."""
from run import *
out=P.parents[1]/'analysis/2026-09-23-uninstall/tests-extra';elf,sym=build(out)
pages,ranges,meta=fixture();m=bytearray(meta);q=dict(pages);c=3000;a=0x9000+(c-2)*32768
for cl in range(c,c+4):
 o=4096+cl+cl//2;v=cl+1 if cl<c+3 else 0xfff;x=int.from_bytes(m[o:o+2],'little');x=(x&15)|(v<<4) if cl&1 else (x&0xf000)|v;m[o:o+2]=x.to_bytes(2,'little')
o=0x5000+11*32;m[o:o+11]=b'CFWTEXT DAT';m[o+11]=32;struct.pack_into('<H',m,o+26,c);put(m,o+28,0x20000)
m[0x3000:0x5000]=m[0x1000:0x3000]
for off in range(0,0x9000,4096):q[off]=pair(m[off:off+4096])
h=block();put(h,0,0x314a4643,1,4,1,8,1,2,3);put(h,64,0x31545854,1);q[a+0x1f000]=pair(seal(h));q[a]=bytes([31])*4096
normal=Model(elf,sym,q);normal.run();assert normal.read(a,0x20000)==b'\xff'*0x20000
rows=[{'test':'uid-owned-text-removed','pass':True}]
for invalid in ['foreign-text','unknown-format','long-name']:
 bad=dict(q)
 if invalid=='foreign-text':h2=bytearray(pair(bad[a+0x1f000]));put(h2,20,9);bad[a+0x1f000]=pair(seal(h2))
 elif invalid=='unknown-format':bad[a+0x1f000]=FF
 else:h2=bytearray(pair(bad[0x5000]));h2[10*32+11]=15;bad[0x5000]=pair(h2)
 test=Model(elf,sym,bad);r=test.call('TestInit');r=r or test.call('TestAudit');assert r and not test.mutations
 rows.append({'test':invalid,'pass':True})
class IOFail(Model):
 def hook(self,u,address,size,name):
  try:super().hook(u,address,size,name)
  except PowerCut:
   self.cut=None;u.reg_write(UC_ARM_REG_R0,1);u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
baseline=Model(elf,sym,pages);baseline.run()
for name in ['TestErase','TestProgram','TestJournalProgram']:
 cut=next(i for i,x in enumerate(baseline.mutations) if x[0]==name)
 test=IOFail(elf,sym,pages,cut=cut,tear=True)
 try:test.run();raise ValueError('I/O failure ignored')
 except AssertionError:pass
 assert test.call('TestState')==5
 resumed=Model(elf,sym,test.pages,test.journal);resumed.run()
 for k in set(baseline.pages)|set(resumed.pages):assert baseline.pages.get(k,FF)==resumed.pages.get(k,FF)
 rows.append({'test':'reported-failure-'+name,'pass':True})
(out/'results.json').write_text(json.dumps({'hardware':False,'tests':rows},indent=2));print(json.dumps(rows))
