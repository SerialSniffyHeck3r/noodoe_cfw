"""Synchronize source selection and the shared Debug / Product Release policies.

Debug uses -Os/-g3 without LTO for every profile. Release retains the existing
Product LTO boundaries; no vendor or Cube source is modified.
"""
import argparse
import copy
import json
from pathlib import Path
from build_optimization import sync_debug, validate_debug
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[1]
# Preserve the established Product Release per-file LTO configuration.
PHOTO_LTO_FILES={
 'Graphics/Port/src/graphics_background.c',
 'Graphics/UI/src/scalar_transition.c',
 'App_Logic/UI/src/wallpaper.c',
 'App_Logic/UI/src/wallpaper_runtime.c',
 'Graphics/Port/src/graphics_background_math.c',
 'App_Logic/Settings/src/settings_quick_presenter.c',
 'App_Logic/UI/src/dashboard_pages.c',
 'Middlewares/Noodoe/Photos/src/PhotoService.c',
 'Middlewares/Noodoe/Photos/src/photo_jpeg.c',
 'Middlewares/Noodoe/Storage/src/StorageService.c',
 'Middlewares/Noodoe/Storage/src/storage_disk.c',
 'Middlewares/Third_Party/FatFs/src/ff.c',
}
PHOTO_LTO_FILES.update(p.relative_to(ROOT).as_posix() for p in (ROOT/'App_Logic/UI/src').glob('ui_*.c'))
# Link-time inlining must not consume symbols interposed with --wrap. Keep the
# defining translation units opaque, and keep FreeRTOS's hand-written naked
# assembly outside LTO to preserve its literal-pool/entry-point contracts.
PRODUCT_LTO_BOUNDARIES={
 'Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F/port.c',
 'Middlewares/Third_Party/LVGL/src/draw/eve/lv_eve.c',
 'Middlewares/Third_Party/LVGL/src/draw/eve/lv_draw_eve_image.c',
 'Middlewares/Third_Party/LVGL/src/draw/eve/lv_draw_eve_letter.c',
 'Middlewares/Third_Party/LVGL/src/draw/eve/lv_draw_eve_ram_g.c',
 'Drivers/BSP/src/bsp_fault.c',
}
PRODUCT_LTO_FLAGS='-Oz -flto'
LTO_REPORT_FLAGS='-fstack-usage -fcyclomatic-complexity'
PRODUCT_LTO_KEEP=(
 'Reset_Handler','main','SystemInit','BSP_BootEarly','BSP_BootRuntimeReady',
 'StartDefaultTask','BSP_BringupMark','BSP_BringupHalTick','BSP_FaultRecord',
 'BSP_FaultClear','g_bsp_boot','g_bsp_bringup','g_bsp_fault',
)

def _sync_product_lto(config,product):
    """Product Release-only whole-program size policy, regenerated from owned XML.

    One distinguishable list item per tool permits exact profile rollback.
    Debug is handled separately by the uniform Os/g3/no-LTO policy.
    Symbol roots preserve the image-validator/SWD contract through LTO.
    """
    product=product and config.get('name')=='Release'
    cc='com.st.stm32cube.ide.mcu.gnu.managedbuild.tool.c.compiler'
    prefix=config.get('id')+'.noodoe.lto_boundary.'
    for node in list(config.findall('fileInfo')):
        if node.get('id','').startswith(prefix):config.remove(node)
    compiler=config.find(f'folderInfo[@resourcePath=""]/toolChain/tool[@superClass="{cc}"]')
    if compiler is None:raise ValueError('Missing root C compiler')
    for tool in config.findall('.//tool'):
        # File tools inherit a project instance; the remaining actual tools
        # identify the compiler directly through their extension superclass.
        kind=tool.get('superClass','')
        is_cc=kind==cc or kind==compiler.get('id') or 'compiler' in kind and not 'assembler' in kind
        is_linker=kind.endswith(('tool.c.linker','tool.cpp.linker'))
        if not is_cc and not is_linker:continue
        cls=(cc if is_cc else kind)+'.option.otherflags'
        opt=tool.find(f'option[@superClass="{cls}"]')
        if opt is None:
            if not product:continue
            opt=ET.SubElement(tool,'option',id=tool.get('id')+'.product_lto',superClass=cls,valueType='stringList')
        for node in list(opt):
            value=node.get('value','')
            if value==PRODUCT_LTO_FLAGS or value.startswith('-Wl,--undefined=') and value.split('=',1)[1] in PRODUCT_LTO_KEEP:
                opt.remove(node)
        if product:
            ET.SubElement(opt,'listOptionValue',builtIn='false',value=PRODUCT_LTO_FLAGS)
            if is_linker:
                for symbol in PRODUCT_LTO_KEEP:ET.SubElement(opt,'listOptionValue',builtIn='false',value='-Wl,--undefined='+symbol)
    if not product:return
    entries=config.find('sourceEntries')
    for path in sorted(PRODUCT_LTO_BOUNDARIES):
        if config.find(f'fileInfo[@resourcePath="{path}"]') is not None:
            # Current boundaries do not overlap the existing photo/UI file
            # policy. A future conflict must be handled explicitly, not lost.
            existing=config.find(f'fileInfo[@resourcePath="{path}"]')
            tool=existing.find('tool')
        else:
            ident=prefix+Path(path).stem
            existing=ET.Element('fileInfo',id=ident,name=Path(path).name,resourcePath=path,rcbsApplicability='disable',toolsToInvoke=ident+'.tool')
            tool=ET.SubElement(existing,'tool',id=ident+'.tool',name=compiler.get('name','MCU GCC Compiler'),superClass=compiler.get('id'))
            for original in compiler.findall('inputType'):ET.SubElement(tool,'inputType',id=ident+'.input',superClass=original.get('superClass'))
            config.insert(list(config).index(entries),existing)
        cls=cc+'.option.otherflags';opt=tool.find(f'option[@superClass="{cls}"]')
        if opt is None:opt=ET.SubElement(tool,'option',id=tool.get('id')+'.boundary',superClass=cls,valueType='stringList')
        for node in list(opt):
            if node.get('value') in (PRODUCT_LTO_FLAGS,'-Os -fno-lto'):opt.remove(node)
        ET.SubElement(opt,'listOptionValue',builtIn='false',value='-Os -fno-lto')

def _sync_lto_artifacts(config,product):
    """Model real GCC outputs: LTO reports belong to final code generation.

    ST declares unconditional .su/.cyclo output types independently of its
    true-by-default compiler options. Slim LTO objects produce neither file,
    so model an empty output-name list and disable the corresponding flags
    only on LTO compilation; request both reports from the final LTO link.
    Ordinary translation units retain their existing compiler reports. These
    are output-policy settings, not warning suppression or optimization flags.
    """
    cc='com.st.stm32cube.ide.mcu.gnu.managedbuild.tool.c.compiler'
    marker='.noodoe.lto_artifact.'
    tools=config.findall('.//tool')
    # Exact policy rollback: never leave disabled reports in another profile.
    for tool in tools:
        for output in list(tool.findall('outputType')):
            if marker in output.get('id',''):tool.remove(output)
        for opt in list(tool.findall('option')):
            if marker in opt.get('id',''):tool.remove(opt)
            else:
                for item in list(opt.findall('listOptionValue')):
                    if item.get('value')==LTO_REPORT_FLAGS:opt.remove(item)
    root=config.find(f'folderInfo[@resourcePath=""]/toolChain/tool[@superClass="{cc}"]')
    if root is None:raise ValueError('Missing root C compiler')
    any_lto=False
    root_lto=False
    for tool in tools:
        kind=tool.get('superClass','')
        if not (kind==cc or kind==root.get('id') or 'compiler' in kind and not 'assembler' in kind):continue
        lto=product and config.get('name')=='Release'
        for opt in tool.findall(f'option[@superClass="{cc}.option.otherflags"]'):
            for item in opt.findall('listOptionValue'):
                for flag in item.get('value','').split():
                    if flag=='-flto' or flag.startswith('-flto='):lto=True
                    elif flag=='-fno-lto':lto=False
        any_lto=any_lto or lto
        if tool is root:root_lto=lto
        if lto:
            # ST's output types are unconditional: disabling the compiler
            # boolean alone does not remove their peer make targets. CDT uses
            # a semicolon-separated outputNames list; ";" is an explicit
            # zero-element list. Retain inherited buildVariable/extensions.
            for suffix in ('su','cyclo'):
                parent=(root.get('id')+marker+'output.'+suffix
                        if kind==root.get('id') and root_lto else cc+'.output.'+suffix)
                ET.SubElement(tool,'outputType',id=tool.get('id')+marker+'output.'+suffix,
                              superClass=parent,outputNames=';')
        elif kind==root.get('id') and root_lto:
            # A non-LTO file boundary inherits the root's empty report outputs.
            # Declare its real native-code reports as local output types.
            for suffix,variable in (('su','SU_FILES'),('cyclo','CYCLO_FILES')):
                ET.SubElement(tool,'outputType',id=tool.get('id')+marker+'output.'+suffix,
                              outputs=suffix,namePattern='%.'+suffix,
                              buildVariable=variable,primaryOutput='false')
        # File boundaries inherit a disabled root in Product Release and must
        # explicitly restore per-TU reports. Debug non-LTO tools stay untouched.
        if not lto and not (product and config.get('name')=='Release'):continue
        for suffix in ('fstackusage','cyclomaticcomplexity'):
            cls=cc+'.option.'+suffix
            original=tool.find(f'option[@superClass="{cls}"]')
            if original is not None:
                if original.get('value')!=str(not lto).lower():
                    raise ValueError('Conflicting explicit LTO report setting: '+cls)
                continue
            ET.SubElement(tool,'option',id=tool.get('id')+marker+suffix,
                          superClass=cls,valueType='boolean',value=str(not lto).lower())
    if any_lto:
        for tool in tools:
            kind=tool.get('superClass','')
            if not kind.endswith(('tool.c.linker','tool.cpp.linker')):continue
            cls=kind+'.option.otherflags';opt=tool.find(f'option[@superClass="{cls}"]')
            if opt is None:opt=ET.SubElement(tool,'option',id=tool.get('id')+marker+'flags',superClass=cls,valueType='stringList')
            ET.SubElement(opt,'listOptionValue',builtIn='false',value=LTO_REPORT_FLAGS)

def _sync_release_files(config, product=False):
    """Remove obsolete owned exceptions; retain Product Release's known files.

    Only project-owned IDs are migrated. User file settings are preserved and
    conflicting Release targets fail instead of being silently overwritten.
    """
    prefixes=tuple(config.get('id')+'.noodoe.'+group+'.'
        for group in ('product','memory','graphics','ambient','bt','dash'))
    # 고정 ID로 소유한 예외만 제거한다. Graphics 전환/반복 sync에서 예외가
    # 누적되지 않으며 Release와 다른 파일의 사용자 설정은 건드리지 않는다.
    for node in list(config.findall('fileInfo')):
        if node.get('id','').startswith(prefixes+(config.get('id')+'.noodoe.lto_boundary.',)): config.remove(node)
    entries=config.find('sourceEntries')
    compiler_class='com.st.stm32cube.ide.mcu.gnu.managedbuild.tool.c.compiler'
    compiler=config.find(f'folderInfo[@resourcePath=""]/toolChain/tool[@superClass="{compiler_class}"]')
    if compiler is None or entries is None: raise ValueError('Missing root compiler/source entries')
    if config.get('name')!='Release' or not product:return
    for path in sorted(PHOTO_LTO_FILES):
        group='product'
        if config.find(f'fileInfo[@resourcePath="{path}"]') is not None:
            raise ValueError(f'Conflicting user file settings: {path}')
        name=Path(path).name; suffix='.noodoe.'+group+'.'+Path(path).stem
        tool_id=compiler.get('id')+suffix
        file_info=ET.Element('fileInfo',id=config.get('id')+suffix,
            name=name,rcbsApplicability='disable',resourcePath=path,toolsToInvoke=tool_id)
        # CDT fileInfo의 부모 tool 인스턴스 상속이다. 폴더 toolChain의
        # extension 상속과 다르며 최신 루트 include를 그대로 따라간다.
        tool=ET.SubElement(file_info,'tool',id=tool_id,
            name=compiler.get('name','MCU/MPU GCC Compiler'),superClass=compiler.get('id'))
        for option_name,level in (('optimization.level','os'),('debuglevel','g3')):
            option_class=compiler_class+'.option.'+option_name
            parent=compiler.find(f'option[@superClass="{option_class}"]')
            parent_id=parent.get('id') if parent is not None else option_class
            # tool은 프로젝트 부모 인스턴스를 상속하지만 option resolver는
            # 설치된 extension ID를 요구한다. 옵션 인스턴스 ID를 부모로 쓰면
            # CDT가 Missing superclass를 남기고 기본 -O0를 조용히 적용한다.
            ET.SubElement(tool,'option',id=parent_id+suffix,superClass=option_class,
                value=option_class+'.value.'+level,valueType='enumerated')
        # 컴파일 입력 타입만 상속하고 linker/builder/custom step은 만들지
        # 않는다. toolsToInvoke가 빈 문자열이면 CDT가 예외를 무시하므로 명시한다.
        if product and path in PHOTO_LTO_FILES:
            flags_class=compiler_class+'.option.otherflags'
            flags=ET.SubElement(tool,'option',id=tool_id+'.photo_lto',superClass=flags_class,valueType='stringList')
            parent_flags=compiler.find(f'option[@superClass="{flags_class}"]')
            if parent_flags is not None:
                for value in parent_flags:flags.append(copy.deepcopy(value))
            ET.SubElement(flags,'listOptionValue',builtIn='false',value='-flto')
            ET.SubElement(flags,'listOptionValue',builtIn='false',value='-Oz')
        for original in compiler.findall('inputType'):
            ET.SubElement(tool,'inputType',id=original.get('id')+suffix,
                superClass=original.get('superClass'))
        config.insert(list(config).index(entries),file_info)

def synchronize(root=ROOT, profile=None):
    """Update both configurations idempotently; persist an explicit profile."""
    policy_path=root/'tools/build_profile.json'
    policy=json.loads(policy_path.read_text(encoding='utf-8'))
    if profile is not None:
        if profile not in policy['profiles']: raise ValueError('Unknown profile')
        policy['profile']=profile
        policy_path.write_text(json.dumps(policy,indent=2)+'\n',encoding='utf-8')
    product=policy['profile']=='Product'
    integrated=policy['profile'] in ('Integrated','Product')
    owned_roots=('App_Logic','Middlewares/Noodoe')
    modules=[json.loads(p.read_text(encoding='utf-8')) for base in owned_roots
             for p in sorted((root/base).glob('**/module.build.json'))]
    vendors=[v for m in modules for v in m.get('vendor_roots',[])]
    includes={'../Drivers/BSP/inc'}
    if product:includes.update(('../RecoveryGate/include','../RecoveryGate/src'))
    includes.update('../'+p.relative_to(root).as_posix() for base in owned_roots
                    for p in (root/base).glob('*/inc'))
    includes.update('../'+p for m in modules for p in m.get('include_paths',[]))
    includes.difference_update(('../Middlewares/Noodoe/USB/inc','../Middlewares/Noodoe/OBD/inc'))
    defines={'NOODOE_INTEGRATED='+str(int(integrated)), 'NOODOE_PRODUCT='+str(int(product))}
    if product: defines.add('configAPPLICATION_ALLOCATED_HEAP=1')
    defines.update(d for m in modules for d in m.get('defines',[]))
    tree=ET.parse(root/'.cproject'); document=tree.getroot()
    for config in document.findall('.//configuration'):
        if config.get('name') not in ('Debug','Release'): continue
        # Installed CubeIDE's misc linker option is a stringList. The wrapper
        # reserves RAM_G above512KiB for actual GPU snapshots without patches
        # to the pinned LVGL allocator source.
        for tool in config.findall('.//tool'):
            superclass=tool.get('superClass','')
            if not superclass.endswith(('tool.c.linker','tool.cpp.linker')): continue
            option_class=superclass+'.option.otherflags'
            opt=tool.find(f'option[@superClass="{option_class}"]')
            if opt is None:
                opt=ET.SubElement(tool,'option',id=option_class+'.noodoe.capture',superClass=option_class,valueType='stringList')
            flag='-Wl,--wrap=lv_draw_eve_ramg_get_addr'
            if not any(n.get('value')==flag for n in opt):
                ET.SubElement(opt,'listOptionValue',builtIn='false',value=flag)
            clip_flag='-Wl,--wrap=lv_eve_scissor'
            if not any(n.get('value')==clip_flag for n in opt):
                ET.SubElement(opt,'listOptionValue',builtIn='false',value=clip_flag)
            for node in list(opt):
                if node.get('value')=='-Oz' and (not product or config.get('name')=='Debug'):opt.remove(node)
            if product and config.get('name')=='Release' and not any(n.get('value')=='-Oz' for n in opt):
                ET.SubElement(opt,'listOptionValue',builtIn='false',value='-Oz')
            large_flag='-Wl,--wrap=lv_draw_eve_label'
            for node in list(opt):
                if node.get('value')==large_flag and not product:opt.remove(node)
            if product and not any(n.get('value')==large_flag for n in opt):
                ET.SubElement(opt,'listOptionValue',builtIn='false',value=large_flag)
            theme_flag='-Wl,--wrap=lv_eve_color'
            for node in list(opt):
                if node.get('value')==theme_flag and not product:opt.remove(node)
            if product and not any(n.get('value')==theme_flag for n in opt):
                ET.SubElement(opt,'listOptionValue',builtIn='false',value=theme_flag)
            image_flag='-Wl,--wrap=lv_draw_eve_image'
            for node in list(opt):
                if node.get('value')==image_flag and not product:opt.remove(node)
            if product and not any(n.get('value')==image_flag for n in opt):
                ET.SubElement(opt,'listOptionValue',builtIn='false',value=image_flag)
        entries=config.find('sourceEntries')
        if entries is None: raise ValueError('Missing source entries')
        def entry(name):
            node=next((n for n in entries if n.get('name')==name),None)
            if node is None:
                node=ET.SubElement(entries,'entry',dict(flags='VALUE_WORKSPACE_PATH|RESOLVED',kind='sourcePath',name=name))
            return node
        # App sources have their own root. Noodoe middleware is already below
        # Middlewares: a nested root would compile it twice. Remove legacy roots.
        for node in list(entries):
            if node.get('name') in ('Services','Middlewares/Noodoe'):
                entries.remove(node)
        entry('App_Logic').set('excluding','' if product else 'UI|'+ '|'.join(['Settings/src/app_persistence.c', 'Settings/src/settings_product.c', 'Vehicle/src/oil_usage_product.c', 'Settings/src/app_settings.c', 'Settings/src/settings_catalog.c', 'Settings/src/settings_connections.c', 'Settings/src/settings_device.c', 'Settings/src/settings_display.c', 'Settings/src/settings_maintenance.c', 'Settings/src/settings_motion.c', 'Settings/src/settings_system.c', 'Settings/src/settings_power.c', 'Settings/src/settings_time.c', 'Settings/src/settings_ui.c', 'Settings/src/settings_vehicle.c', 'Settings/src/settings_test_port.c', 'Settings/src/settings_quick_presenter.c']))
        graphics=entry('Graphics')
        # Bootstrap has its own minimal link, embedded stock image and main
        # service. Never compile that installer into the ordinary Product APP.
        app_entry=entry('App_Logic')
        app_entry.set('excluding','|'.join(filter(None,[app_entry.get('excluding',''),'Bootstrap'])))
        graphics_excluded=[]
        for p in (root/'Graphics/UI/src').glob('*.c'):
            product_source=p.name.startswith(('ui_','product_','odometer_','speed_home_','dashboard_pages_','page_transition','scalar_transition','button_hints','scene_transition','settings_','power_'))
            exclude=(not product_source if product else product_source or
                (integrated and p.name.startswith('GT_') and not p.name.startswith('GT_00_')))
            if exclude: graphics_excluded.append(p.relative_to(root/'Graphics').as_posix())
        graphics.set('excluding','|'.join(sorted(graphics_excluded)))
        parent=entry('Middlewares'); excluded=set(filter(None,parent.get('excluding','').split('|')))
        # LVGL owns a nested source root; exclusion on its ancestor would not
        # affect that root. The project-owned derivative replaces just this TU.
        default_font='font/lv_font_montserrat_14.c'
        lvgl_entry=entry('Middlewares/Third_Party/LVGL/src')
        lvgl_excluded=set(filter(None,lvgl_entry.get('excluding','').split('|')))
        if product:lvgl_excluded.add(default_font)
        else:lvgl_excluded.discard(default_font)
        lvgl_entry.set('excluding','|'.join(sorted(lvgl_excluded)))
        for vendor in vendors:
            path=vendor['path']; base=root/path; selected=set(vendor['sources'])
            if not base.is_dir(): raise ValueError(f'Missing vendor root {path}')
            for source in selected:
                if not (base/source).is_file(): raise ValueError(f'Missing source {path}/{source}')
            unwanted=[]
            def visit(folder):
                for p in folder.iterdir():
                    rel=p.relative_to(base).as_posix()
                    if p.is_dir():
                        if not any(s.startswith(rel+'/') for s in selected): unwanted.append(rel)
                        else: visit(p)
                    elif p.suffix.lower() in ('.c','.cpp','.cc','.s') and rel not in selected: unwanted.append(rel)
            visit(base)
            entry(path).set('excluding','|'.join(sorted(unwanted)))
            excluded.add(Path(path).relative_to('Middlewares').as_posix())
        excluded.update(('Noodoe/USB','Noodoe/OBD','Noodoe/Storage/src/StorageBackup.c'))
        parent.set('excluding','|'.join(sorted(excluded)))
        for base, removed in {
            'Drivers': ('BSP/src/bsp_usb_device.c','STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pcd.c','STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pcd_ex.c','STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_usb.c'),
            'Core': ('Src/usb_otg.c',),
            'App_Logic': ('UI/src/ui_obd.c',),
        }.items():
            node=entry(base)
            node.set('excluding','|'.join(sorted(set(filter(None,node.get('excluding','').split('|')))|set(removed))))
        for opt in config.findall('.//option[@valueType="includePath"]'):
            # Service public include directories and module manifests are the
            # only include contract; TI archive metadata is never an API path.
            for n in list(opt):
                value=n.get('value','')
                if (value=='../Services' or value.startswith(('../Services/','../Middlewares/Noodoe/','../App_Logic/','../Middlewares/Third_Party/USB_DEVICE/'))) and value not in includes:
                    opt.remove(n)
            present={n.get('value') for n in opt}
            for inc in sorted(includes-present): ET.SubElement(opt,'listOptionValue',builtIn='false',value=inc)
        for opt in config.findall('.//option[@valueType="definedSymbols"]'):
            for n in list(opt):
                if n.get('value','').startswith(('NOODOE_INTEGRATED=','NOODOE_PRODUCT=','configAPPLICATION_ALLOCATED_HEAP=')): opt.remove(n)
            present={n.get('value') for n in opt}
            for value in sorted(defines-present): ET.SubElement(opt,'listOptionValue',builtIn='false',value=value)
        _sync_release_files(config, product)
        sync_debug(config)
        _sync_product_lto(config,product)
        _sync_lto_artifacts(config,product)
        validate_debug(config)
    # CDT requires its processing instruction; ElementTree otherwise drops it
    # and Eclipse reports that this is no longer a CDT project.
    output=b'<?xml version="1.0" encoding="UTF-8" standalone="no"?>\n<?fileVersion 4.0.0?>\n'+ET.tostring(document,encoding='utf-8')
    if output!=(root/'.cproject').read_bytes(): (root/'.cproject').write_bytes(output)
    print(f'Service profile {policy["profile"]}: {len(modules)} modules, {len(vendors)} vendor roots')

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile',choices=['Integrated','Graphics','Product'])
    args=parser.parse_args(); synchronize(profile=args.profile)
