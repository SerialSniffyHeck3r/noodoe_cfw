"""Exercise the installed ARM compiler's real LTO output contract, offline."""
from pathlib import Path
import hashlib,json,subprocess,sys
P=Path(__file__).resolve().parents[3];sys.path.insert(0,str(P/'tools'))
from resource_install import TC
OUT=Path(__file__).resolve().parent/'output';OUT.mkdir(exist_ok=True)
def command(*args):
    r=subprocess.run(list(map(str,args)),capture_output=True,text=True)
    if r.returncode:raise RuntimeError(r.stdout+r.stderr)
    return r.stdout+r.stderr
source_a='extern unsigned f(unsigned);void _start(void){volatile unsigned n=*(volatile unsigned*)0x20000000;*(volatile unsigned*)0x20000004=f(n);while(1){}}\n'
source_b='__attribute__((noinline)) unsigned f(unsigned x){volatile unsigned a[12];for(unsigned i=0;i<12;++i)a[i]=x+i;return a[x%12];}\n'
(OUT/'a.c').write_text(source_a);(OUT/'b.c').write_text(source_b)
gcc=TC/'arm-none-eabi-gcc.exe';flags=['-mcpu=cortex-m4','-mthumb','-Oz','-flto']
for name in ('a','b'):
    command(gcc,*flags,'-c',OUT/(name+'.c'),'-o',OUT/(name+'.o'))
    assert not (OUT/(name+'.su')).exists() and not (OUT/(name+'.cyclo')).exists()
objects=[OUT/'a.o',OUT/'b.o'];binaries=[]
for name,reports in [('plain',[]),('reported',['-fstack-usage','-fcyclomatic-complexity'])]:
    command(gcc,*flags,*reports,'-nostdlib',*objects,'-Wl,-e,_start','-o',OUT/(name+'.elf'))
    command(TC/'arm-none-eabi-objcopy.exe','-O','binary',OUT/(name+'.elf'),OUT/(name+'.bin'))
    binaries.append((OUT/(name+'.bin')).read_bytes())
assert binaries[0]==binaries[1],'Report generation must not change linked machine code'
stack=list(OUT.glob('reported.elf.ltrans*.su'));complexity=list(OUT.glob('reported.elf.ltrans*.cyclo'))
assert stack and complexity,'Final LTO reports missing'
assert any(':f\t48\tstatic' in f.read_text() for f in stack),'Missing actual post-LTO stack frame'
command(gcc,'-mcpu=cortex-m4','-mthumb','-Os','-fno-lto','-fstack-usage','-fcyclomatic-complexity','-c',OUT/'b.c','-o',OUT/'boundary.o')
assert (OUT/'boundary.su').exists() and (OUT/'boundary.cyclo').exists()
result=dict(status='pass',linked_code_equal=True,linked_sha256=hashlib.sha256(binaries[0]).hexdigest(),
            lto_compile_reports=False,final_lto_reports=[x.name for x in stack+complexity],
            non_lto_boundary_reports=True,hardware_access=False)
(OUT/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
