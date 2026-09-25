"""Build the independent, typed temporary Diagnostic APP.

Uses unmodified Cube startup/HAL/clock/IRQ/RTOS sources and the same APP linker.
No Eclipse project switching, no hardware access and no generated-source edits.
"""
from pathlib import Path
import argparse,concurrent.futures,hashlib,json,subprocess
from build_inputs import fingerprint
from check_generated import main as check_generated
P=Path(__file__).resolve().parents[1]
TC=Path('C:/ST/STM32CubeIDE_1.18.1/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344/tools/bin')
def build(out):
    # This standalone build must obey the same Cube clock/peripheral contract
    # as Product. Never publish a Bootstrap with stale generated HSI/USB code.
    if check_generated():raise ValueError('Cube-generated contract failed')
    out=out.resolve();out.mkdir(parents=True,exist_ok=True)
    includes={P/'Core/Inc',P/'Drivers/CMSIS/Include',P/'Drivers/CMSIS/Device/ST/STM32F4xx/Include',P/'Drivers/STM32F4xx_HAL_Driver/Inc',P/'Drivers/BSP/inc',P/'Graphics/Port/inc',P/'Graphics/UI/inc',P/'Middlewares/Third_Party/LVGL'}
    modules=[json.loads(p.read_text()) for base in ('App_Logic','Middlewares/Noodoe') for p in (P/base).glob('**/module.build.json')]
    includes.add(P/'RecoveryGate/include');includes.add(P/'Diagnostic')
    includes.add(P/'RecoveryGate/src')
    for base in ('App_Logic','Middlewares/Noodoe'):
        includes.update((P/base).glob('*/inc'))
    for m in modules:includes.update(P/p for p in m.get('include_paths',[]))
    rt=P/'Middlewares/Third_Party/FreeRTOS/Source'
    includes.update([rt/'include',rt/'CMSIS_RTOS_V2',rt/'portable/GCC/ARM_CM4F'])
    sources=list((P/'Core/Src').glob('*.c'))+[P/'Core/Startup/startup_stm32f429ietx.s']
    sources += [p for p in (P/'Drivers/STM32F4xx_HAL_Driver/Src').glob('*.c') if 'template' not in p.name]
    bsp=['bsp_boot.c','bsp_reset.s','bsp_fault.c','bsp_bringup.c','bsp_cpu_load.c','BSP_Buttons.c','BSP_Display.c','bsp_eve.c','bsp_eve_bus.c','bsp_lcd_panel.c','bsp_backlight.c','BSP_NOR.c','BSP_Power.c','BSP_RAM.c','bsp_bt_hci.c','BSP_BT_ResetDiagnostic.c','BSP_Watchdog.c','BSP_Ambient.c','BSP_AmbientBitbang.c','BSP_AmbientAddress.c']
    sources += [P/'Drivers/BSP/src'/p for p in bsp]
    sources += list(rt.glob('*.c'))+[rt/'CMSIS_RTOS_V2/cmsis_os2.c',rt/'portable/GCC/ARM_CM4F/port.c',rt/'portable/MemMang/heap_4.c']
    sources += list((P/'Diagnostic').glob('*.c'))
    sources.append(P/'App_Logic/Bootstrap/src/bootstrap_confirm.c')
    for name in ('Bluetooth','Health'):
        sources += list((P/'Middlewares/Noodoe'/name/'src').glob('*.c'))
    sources += [P/'Drivers/BSP/src/noodoe_crc32.c',P/'Middlewares/Noodoe/Ambient/src/AmbientService.c',P/'Middlewares/Noodoe/Ambient/src/AmbientDiagnostics.c',P/'Middlewares/Noodoe/Update/src/NDCP.c']
    for name in ('cfw_files.c','cfw_store.c','config_store.c','storage_disk.c'):
        sources.append(P/'Middlewares/Noodoe/Storage/src'/name)
    for name in ('gate_policy.c','gate_format.c','event_log.c'):
        sources.append(P/'RecoveryGate/src'/name)
    sources.append(P/'Middlewares/Noodoe/Update/src/Update_SHA256.c')
    bt=json.loads((P/'Middlewares/Noodoe/Bluetooth/module.build.json').read_text())
    for v in bt['vendor_roots']:sources += [P/v['path']/s for s in v['sources']]
    sources.append(P/'Graphics/UI/src/bootstrap_screen.c')
    defs=['USE_HAL_DRIVER','STM32F429xx','NOODOE_DIAGNOSTIC=1','NOODOE_BOOTSTRAP=0','NOODOE_PRODUCT=0','NOODOE_INTEGRATED=0','configAPPLICATION_ALLOCATED_HEAP=1','NDEBUG']
    flags=['-mcpu=cortex-m4','-mthumb','-mfpu=fpv4-sp-d16','-mfloat-abi=hard','-Os','-g3','-ffunction-sections','-fdata-sections','-fstack-usage','-Wall','--specs=nano.specs']+['-D'+d for d in defs]+['-I'+str(i) for i in sorted(includes)]
    inputs_digest=fingerprint(sources,includes)
    def compile(src):
        obj=out/(hashlib.sha256(str(src).encode()).hexdigest()[:12]+'-'+src.stem+'.o')
        stamp=obj.with_suffix('.hash')
        digest=hashlib.sha256((str(src)+inputs_digest+repr(flags)).encode()).hexdigest()
        if obj.exists() and stamp.exists() and stamp.read_text()==digest:return obj
        cmd=[str(TC/'arm-none-eabi-gcc.exe'),*flags,'-c',str(src),'-o',str(obj)]
        r=subprocess.run(cmd,capture_output=True,text=True)
        obj.with_suffix('.log').write_text(r.stdout+r.stderr,encoding='utf-8')
        if r.returncode:raise RuntimeError(str(src)+'\n'+r.stdout+r.stderr)
        stamp.write_text(digest)
        return obj
    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as executor:objects=list(executor.map(compile,sources))
    elf=out/'diagnostic.elf'
    cmd=[str(TC/'arm-none-eabi-gcc.exe'),*flags,'--specs=nosys.specs',*map(str,objects),'-Wl,-T,'+str(P/'Linker/Noodoe_Product.ld'),'-Wl,--gc-sections','-Wl,-Map,'+str(out/'diagnostic.map'),'-Wl,--print-memory-usage','-o',str(elf),'-lm']
    r=subprocess.run(cmd,capture_output=True,text=True);(out/'link.log').write_text(r.stdout+r.stderr)
    if r.returncode:raise RuntimeError(r.stdout+r.stderr)
    subprocess.run([str(TC/'arm-none-eabi-objcopy.exe'),'-O','binary','--gap-fill=0xFF',str(elf),str(out/'diagnostic.bin')],check=True)
    image=(out/'diagnostic.bin').read_bytes()
    if len(image)>0x60000:raise ValueError('Diagnostic exceeded Product partition')
    used=len(image)
    image+=bytes([255])*(0x60000-len(image));(out/'diagnostic.bin').write_bytes(image)
    result=dict(bytes=len(image),used_bytes=used,free_bytes=0x60000-used,partition_bytes=0x60000,sha256=hashlib.sha256(image).hexdigest(),sources=len(sources),profile='Diagnostic',role=3,update_target=4,required_gate_feature=1,wireless_verified=False)
    (out/'manifest.json').write_text(json.dumps(result,indent=2)+'\n');print(r.stdout);print(json.dumps(result,indent=2))
if __name__=='__main__':
    a=argparse.ArgumentParser(description=__doc__);a.add_argument('--output',type=Path,required=True);args=a.parse_args();build(args.output)
