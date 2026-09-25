"""Actual Product RuntimeUpdate COMMIT gate; hardware/resource worker mocked."""
from pathlib import Path
# Reuse the established compiler, CMSIS includes, fixture and linker setup.
base=Path(__file__).with_name('runtime_update_run.py').read_text()
exec(compile(base.split('results=[]')[0],__file__,'exec'))
common+=['-DNOODOE_PRODUCT=1','-I',str(PROJECT/'Middlewares/Noodoe/Resources/inc')]
results=[]
for opt in ('-O0','-Os'):
    elf=OUT/('resource-gate-'+opt[1:]+'.elf')
    r=subprocess.run([str(TC/'arm-none-eabi-gcc.exe'),*common,opt,'-DRUNTIME_UPDATE_TEST_PORT','-I',str(HERE),'-I',str(OUT),'-nostdlib',*map(str,SOURCES),str(HERE/'resource_gate_test.c'),'-T',str(OUT/'runtime_update_test.ld'),'-Wl,-e,ResourceGate_Test','-lgcc','-o',str(elf)],capture_output=True,text=True)
    assert not r.returncode,r.stderr
    nm=subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),str(elf)],text=True);symbols={w[2]:int(w[0],16) for l in nm.splitlines() if len(w:=l.split())==3}
    u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
    for at,n in [(0x10000000,0x20000),(0x20000000,0x20000),(0x11000000,0x80000),(0xc0000000,0x40000)]:u.mem_map(at,n)
    data=elf.read_bytes();phoff=struct.unpack_from('<I',data,28)[0];phsize,phnum=struct.unpack_from('<HH',data,42)
    for i in range(phnum):
        kind,off,va,_,n,_,_,_=struct.unpack_from('<8I',data,phoff+i*phsize)
        if kind==1 and n:u.mem_write(va,data[off:off+n])
    u.reg_write(UC_ARM_REG_SP,0x2001fff0);u.reg_write(UC_ARM_REG_LR,STOP|1);u.reg_write(UC_ARM_REG_XPSR,0x1000000)
    u.emu_start(symbols['ResourceGate_Test']|1,STOP,count=10000000)
    assert u.reg_read(UC_ARM_REG_PC)==STOP
    failure=u.reg_read(UC_ARM_REG_R0);assert not failure,f'{opt} CHECK line {failure}'
    results.append({'optimization':opt,'status':'PASS','cases':['pending-incompatible-lock','matching-ID-commit','invalid-record-magic','invalid-version','invalid-required','new-transaction-isolation','explicit-resource-free-APP']})
print(json.dumps(results));(OUT/'resource-gate-results.json').write_text(json.dumps(results,indent=2))
