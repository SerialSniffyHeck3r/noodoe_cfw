"""Check two exact-ELF live diagnostic captures without accessing hardware."""
import argparse,json
from pathlib import Path

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('samples',type=Path,nargs=2)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();samples=[json.loads(x.read_text()) for x in a.samples]
    checks=[]
    def check(ok,name):
        checks.append(dict(name=name,passed=bool(ok)))
    def words(sample,name):return sample['diagnostics'][name]['words']
    apps=[s.get('app',{}) for s in samples]
    check(bool(apps[0].get('elf_sha256')) and apps[0].get('elf_sha256')==apps[1].get('elf_sha256') and
          apps[0].get('canonical_sha256')==apps[1].get('canonical_sha256'),
          'both samples attest the same exact ELF and APP')
    for index,s in enumerate(samples):
        prefix=f'sample{index+1}: '
        app=s.get('app',{})
        check(app.get('canonical_image_match') is True and app.get('compared_bytes',0)>0 and
              app.get('compared_bytes')==app.get('live_span_bytes') and
              bool(app.get('canonical_sha256')) and app.get('canonical_sha256')==app.get('live_span_sha256'),
              prefix+'live APP exact-byte attestation present')
        r=words(s,'g_resources');check(r[2]==4 and r[3]==0 and r[5]==r[6],prefix+'resources ready')
        f=words(s,'g_bsp_fault');check(f[0]==0 and f[1]==0,prefix+'no retained fault')
        check(words(s,'g_system_error')[3]==0,prefix+'no system error')
        m=words(s,'g_graphics_memory');check(m[0]==0x43434d31 and m[1]==1 and
            0x10000000<=m[2]<=0x10010000-49152 and m[3]==49152 and m[4]>0 and m[5]==0,
            prefix+'48KiB CCM guard checked and intact')
        rt=words(s,'g_noodoe_runtime');check(rt[2]==1 and rt[3]==0,prefix+'runtime started')
        check(rt[18]>=12288,prefix+'RTOS minimum free heap >=12KiB')
        ps=words(s,'g_app_persistence');check(ps[1:5]==[1,1,1,0] and ps[10]==0 and ps[12]==1,prefix+'settings and ride restored once')
        c=words(s,'g_config_store');check(c[0]==1 and c[1]==0 and c[2]==c[3],prefix+'configuration saved')
        ph=words(s,'g_photo_store');check(ph[0]==1 and ph[1]==0 and ph[16]==0 and ph[17]==0,prefix+'photo store ready')
    first,last=[words(s,'g_noodoe_runtime') for s in samples]
    check(all(0<((last[i]-first[i])&0xffffffff)<0x80000000 for i in (4,5,6)),'IO/storage/graphics heartbeats progress')
    result=dict(passed=all(c['passed'] for c in checks),checks=checks,
                sample_paths=[str(x.resolve()) for x in a.samples],
                radio_tested=False,ambient_sensor_tested=False,persistent_bytes_verified=False,
                interpretation='Live owner/resource/restoration checks, not historical fault absence: normal boot clears the fault magic/code. Exact NOR preservation is a separate physical readback proof.')
    a.output.write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
    if not result['passed']:raise SystemExit(1)

if __name__=='__main__':main()
