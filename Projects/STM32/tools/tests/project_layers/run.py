"""Exercise real synchronization against regenerated metadata in an isolated fixture.
Does not edit the real .cproject, generate code in CubeMX, build, or access a board.
"""
from pathlib import Path
import hashlib,importlib.util,json,shutil,subprocess,sys,uuid,xml.etree.ElementTree as ET
P=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(P/'tools'))
out=Path(__file__).resolve().parent/'output';out.mkdir(exist_ok=True)
root=out/('fixture-'+uuid.uuid4().hex);root.mkdir()
spec=importlib.util.spec_from_file_location('service_sync',P/'tools/services_build.py')
service=importlib.util.module_from_spec(spec);spec.loader.exec_module(service)

def copy(rel):
 dest=root/rel;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(P/rel,dest)
def touch(rel):
 dest=root/rel;dest.parent.mkdir(parents=True,exist_ok=True)
 if not dest.exists():dest.write_bytes(b'')
for rel in ('.cproject','FuckNudo_Noodoe_CFW_Project.ioc','Core/Src/freertos.c',
            'Linker/Noodoe_APP.ld','Linker/Noodoe_Product.ld','Graphics/Port/inc/lv_conf.h','tools/lvgl_build.json',
            'tools/build_profile.json','tools/services_build.py','tools/build_optimization.py',
            'Middlewares/Third_Party/LVGL/UPSTREAM.json'):
 copy(rel)
for rel in service.PHOTO_LTO_FILES:touch(rel)
for base in ('App_Logic','Graphics','Drivers/BSP'):
 for source in (P/base).rglob('*.c'):touch(source.relative_to(P))
# Only presence is used by source selection. No vendor implementation is
# imitated/executed; real vendor files are left unchanged in the actual project.
for base in ('App_Logic','Middlewares/Noodoe'):
 for folder in (P/base).glob('*/inc'):(root/folder.relative_to(P)).mkdir(parents=True,exist_ok=True)
 for manifest in (P/base).rglob('module.build.json'):
  copy(manifest.relative_to(P));m=json.loads(manifest.read_text())
  for inc in m.get('include_paths',[]):(root/inc).mkdir(parents=True,exist_ok=True)
  for vendor in m.get('vendor_roots',[]):
   (root/vendor['path']).mkdir(parents=True,exist_ok=True)
   for src in vendor['sources']:touch(vendor['path']+'/'+src)
(root/'Drivers/BSP/inc').mkdir(parents=True,exist_ok=True)
(root/'RecoveryGate/include').mkdir(parents=True,exist_ok=True)
(root/'RecoveryGate/src').mkdir(parents=True,exist_ok=True)
tree=ET.parse(root/'.cproject')
for cfg in tree.findall('.//configuration'):
 if cfg.get('name') not in ('Debug','Release'):continue
 if cfg.get('name')=='Debug':
  cc=service.CC if hasattr(service,'CC') else 'com.st.stm32cube.ide.mcu.gnu.managedbuild.tool.c.compiler'
  compiler=cfg.find(f'folderInfo[@resourcePath=""]/toolChain/tool[@superClass="{cc}"]')
  compiler.find(f'option[@superClass="{cc}.option.optimization.level"]').set('value',cc+'.option.optimization.level.value.o0')
 for entry in list(cfg.find('sourceEntries')):
  if entry.get('name')=='App_Logic':cfg.find('sourceEntries').remove(entry)
 for option in cfg.findall('.//option[@valueType="includePath"]'):
  for item in list(option):
   if item.get('value','').startswith(('../App_Logic/','../Middlewares/Noodoe/')):option.remove(item)
 for option in cfg.findall('.//option[@superClass="com.st.stm32cube.ide.mcu.gnu.managedbuild.tool.c.linker.option.script"]'):
  option.set('value','../STM32F429VETX_FLASH.ld')
tree.write(root/'.cproject',encoding='utf-8',xml_declaration=True)
core=(root/'Core/Src/freertos.c').read_bytes()
ioc=(root/'FuckNudo_Noodoe_CFW_Project.ioc').read_bytes()

def sync(name):
 r=subprocess.run(['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',
     str(P/'tools/sync_project.ps1'),'-ProjectDirectory',str(root)],capture_output=True)
 (out/(name+'.log')).write_bytes(r.stdout+r.stderr)
 assert r.returncode==0,(name,r.stdout.decode(errors='replace'),r.stderr.decode(errors='replace'))
def check(profile):
 tree=ET.parse(root/'.cproject')
 for cfg in tree.findall('.//configuration'):
  if cfg.get('name') not in ('Debug','Release'):continue
  service.validate_debug(cfg)
  roots={e.get('name'):e.get('excluding','') for e in cfg.find('sourceEntries')}
  assert 'App_Logic' in roots and 'Services' not in roots and 'Middlewares/Noodoe' not in roots
  excluded=set(filter(None,roots['App_Logic'].split('|')))
  product_settings={'Settings/src/'+name+'.c' for name in ('app_settings','settings_ui','settings_motion','settings_catalog','settings_display','settings_time','settings_vehicle','settings_maintenance','settings_connections','settings_system','settings_power','settings_device','settings_test_port')}
  assert excluded=={'Bootstrap','UI/src/ui_obd.c'} if profile=='Product' else 'UI' in excluded and product_settings<=excluded
  assert {'Noodoe/USB','Noodoe/OBD'}<=set(roots['Middlewares'].split('|'))
  assert 'Src/usb_otg.c' in roots['Core'].split('|')
  assert 'STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pcd.c' in roots['Drivers'].split('|')
  if profile!='Product':assert not ({'Settings/src/SettingsService.c','Settings/src/settings_record.c'}&excluded)
  graphic_excluded=set(filter(None,roots['Graphics'].split('|')))
  for source in ('settings_view.c','settings_position.c','settings_icon_data.c','scene_transition.c'):
   assert ('UI/src/'+source in graphic_excluded)==(profile!='Product')
  for opt in cfg.findall('.//option[@valueType="includePath"]'):
   paths=[n.get('value') for n in opt]
   for path in ('../App_Logic/UI/inc','../App_Logic/Runtime/inc','../App_Logic/Vehicle/inc',
                '../Middlewares/Noodoe/Vehicle/inc','../Middlewares/Noodoe/Bluetooth/inc','../Middlewares/Noodoe/Clock/inc','../App_Logic/Settings/inc'):
    assert paths.count(path)==1,(profile,cfg.get('name'),path)
   assert not any(p.startswith('../Services') for p in paths)
  linker=cfg.find('.//option[@superClass="com.st.stm32cube.ide.mcu.gnu.managedbuild.tool.c.linker.option.script"]')
  assert linker.get('value')==('../Linker/Noodoe_Product.ld' if profile=='Product' else '../Linker/Noodoe_APP.ld')
  assert ('font/lv_font_montserrat_14.c' in roots['Middlewares/Third_Party/LVGL/src'].split('|'))==(profile=='Product')
  assert not any(name.startswith('RecoveryGate') for name in roots),'Gate code must never be a Product source root'
  boundary={f.get('resourcePath') for f in cfg.findall('fileInfo') if '.noodoe.lto_boundary.' in f.get('id','')}
  assert boundary==(service.PRODUCT_LTO_BOUNDARIES if profile=='Product' and cfg.get('name')=='Release' else set())
  for tool in cfg.findall('.//tool'):
   if tool.get('superClass','').endswith(('tool.c.linker','tool.cpp.linker')):
    flags=[n.get('value') for n in tool.findall('./option[@valueType="stringList"]/listOptionValue')]
    assert flags.count('-Wl,--wrap=lv_eve_scissor')==1,(profile,cfg.get('name'),'clip wrapper missing/duplicate')
  assert (root/'Graphics/Port/src/graphics_eve_clip.c').exists()
  assert (root/'Graphics/Port/src/graphics_eve_transport.c').exists()
 assert (root/'Core/Src/freertos.c').read_bytes()==core
 assert (root/'FuckNudo_Noodoe_CFW_Project.ioc').read_bytes()==ioc

# The fixture always begins in Product, regardless of an unrelated profile
# build running in the real workspace. Only the isolated fixture is changed.
initial=json.loads((root/'tools/build_profile.json').read_text());initial['profile']='Product'
(root/'tools/build_profile.json').write_text(json.dumps(initial))
sync('regenerated');check('Product')
first=(root/'.cproject').read_bytes();sync('repeated');check('Product')
assert (root/'.cproject').read_bytes()==first,'Synchronization is not idempotent'
for profile in ('Integrated','Graphics','Product'):
 policy=json.loads((root/'tools/build_profile.json').read_text());policy['profile']=profile
 (root/'tools/build_profile.json').write_text(json.dumps(policy))
 sync('profile-'+profile);check(profile)
report={'status':'pass','fixture':str(root),'cases':['restore App_Logic root/includes',
 'restore middleware includes','restore APP linker','byte-idempotent repeat',
 'Core/IOC unchanged','Product/Integrated/Graphics source policy'],
 'gui_regeneration_executed':False,'hardware_access':False}
(out/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
