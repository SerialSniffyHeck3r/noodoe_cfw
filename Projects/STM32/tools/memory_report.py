"""Report actual linked address-space occupancy and enforce Product budgets."""
from pathlib import Path
import argparse,json,subprocess
from validate_image import Elf32,validate,SHF_ALLOC
TC=Path('C:/ST/STM32CubeIDE_1.18.1/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344/tools/bin')
def report(path,profile,configuration):
    elf=Elf32(path.read_bytes());binary,manifest=validate(elf)
    def bank(start,end):
        sections=[s for s in elf.sections if s['flags']&SHF_ALLOC and s['size'] and start<=s['addr']<end]
        used=max([s['addr']+s['size']-start for s in sections]+[0])
        return dict(capacity=end-start,used=used,free=end-start-used,sections={s['name']:s['size'] for s in sections})
    capacity=manifest['flash_capacity']
    mailbox=256 if manifest['layout_version']==2 else 0
    r=dict(profile=profile,configuration=configuration,layout_version=manifest['layout_version'],flash=dict(capacity=capacity,used=len(binary),free=capacity-len(binary)),sram=bank(0x20000000,0x20030000-mailbox),ccm=bank(0x10000000,0x10010000),recovery_mailbox_bytes=mailbox)
    rows=[]
    nm=subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),'-S','--size-sort',str(path)],text=True)
    for line in nm.splitlines():
        w=line.split()
        if len(w)==4 and w[2] in 'bBdDrR':rows.append(dict(address=hex(int(w[0],16)),bytes=int(w[1],16),kind=w[2],name=w[3]))
    r['largest_objects']=sorted(rows,key=lambda x:x['bytes'],reverse=True)[:30]
    r['rtos_heap_bytes']=next(x['bytes'] for x in rows if x['name']=='ucHeap')
    r['runtime_requirements']={'rtos_min_free':12288,'fps_average_min':29,'ccm_guard_failures':0,'note':'ELF cannot prove dynamic watermarks or FPS; require hardware diagnostics.'}
    failures=[]
    if profile=='Product':
        # Retired modules must be absent from the final executable, not merely
        # hidden in the UI. Weak default IRQ vectors are intentionally allowed.
        retired=('USBBridge_', 'StorageBackup_Process', 'ObdService_', 'UiMenu_',
                 'HAL_PCD_', 'USBD_', 'sdp_client_')
        linked=[line.split()[-1] for line in nm.splitlines() if len(line.split())>=3]
        r['retired_symbols']=[name for name in linked if name.startswith(retired)]
        if r['retired_symbols']:failures.append('Retired code linked: '+', '.join(r['retired_symbols']))
        if manifest['layout_version']!=2:failures.append('Product must use independent RecoveryGate layout2')
        for key,minimum in [('flash',65536 if configuration=='Release' else 32768),('sram',32768),('ccm',12288)]:
            if r[key]['free']<minimum:failures.append(f'{key}: free {r[key]["free"]} < {minimum}')
        if r['ccm']['sections'].get('.ccm_bss')!=49152+64:failures.append('LVGL guarded48KiB CCM pool missing')
        if r['rtos_heap_bytes']!=49152:failures.append('FreeRTOS48KiB reservation changed')
    r['budget_passed']=not failures;r['failures']=failures;return r
def main():
    a=argparse.ArgumentParser(description=__doc__);a.add_argument('elf',type=Path);a.add_argument('--profile',required=True);a.add_argument('--configuration',required=True);a.add_argument('--output',type=Path,required=True);a=a.parse_args()
    r=report(a.elf,a.profile,a.configuration);a.output.write_text(json.dumps(r,indent=2)+'\n');print(json.dumps({k:r[k] for k in ['flash','sram','ccm','budget_passed','failures']}));raise SystemExit(0 if r['budget_passed'] else 1)
if __name__=='__main__':main()
