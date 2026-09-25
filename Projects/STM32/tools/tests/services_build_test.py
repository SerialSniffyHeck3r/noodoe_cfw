"""Policy migration, regeneration and profile isolation on in-memory CDT XML."""
import copy
from pathlib import Path
import sys
import unittest
import xml.etree.ElementTree as ET
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import services_build as subject
from build_optimization import CC, CPP, sync_debug, validate_debug

class PolicyTests(unittest.TestCase):
    def setUp(self):
        self.path=subject.ROOT/'.cproject';self.raw=self.path.read_bytes()
        self.configs={c.get('name'):c for c in ET.fromstring(self.raw).findall('.//configuration')}

    def apply(self,config,product=True):
        subject._sync_release_files(config,product)
        sync_debug(config)
        subject._sync_product_lto(config,product)
        subject._sync_lto_artifacts(config,product)
        validate_debug(config)

    def test_uniform_defaults_idempotent_all_profiles(self):
        config=self.configs['Debug']
        for product in (True,False,False,True):
            self.apply(config,product)
            self.assertEqual(config.findall('fileInfo'),[])
            self.assertEqual([x.get('resourcePath') for x in config.findall('folderInfo')],[''])
            first=ET.tostring(config);self.apply(config,product)
            self.assertEqual(first,ET.tostring(config))
            for kind in (CC,CPP):
                root=config.find(f'folderInfo[@resourcePath=""]/toolChain/tool[@superClass="{kind}"]')
                for name,value in (('optimization.level','os'),('debuglevel','g3')):
                    cls=kind+'.option.'+name
                    self.assertEqual(root.find(f'option[@superClass="{cls}"]').get('value'),cls+'.value.'+value)
        self.assertEqual(self.path.read_bytes(),self.raw)

    def test_regenerated_defaults_and_legacy_overrides_migrate(self):
        config=self.configs['Debug'];prefix=config.get('id')+'.noodoe.'
        root=config.find(f'folderInfo[@resourcePath=""]/toolChain/tool[@superClass="{CC}"]')
        root.find(f'option[@superClass="{CC}.option.optimization.level"]').set('value',CC+'.option.optimization.level.value.o0')
        ET.SubElement(config,'folderInfo',id=prefix+'lvgl',resourcePath='Middlewares/Third_Party/LVGL/src')
        ET.SubElement(config,'folderInfo',id=prefix+'Middlewares.Third_Party.BTstack',resourcePath='Middlewares/Third_Party/BTstack')
        ET.SubElement(config,'fileInfo',id=prefix+'product.old',resourcePath='App_Logic/old.c')
        self.apply(config)
        self.assertEqual(len(config.findall('folderInfo')),1)
        self.assertFalse(config.findall('fileInfo'))

    def test_deliberate_user_file_override_preserved(self):
        config=self.configs['Debug'];self.apply(config)
        root=config.find(f'folderInfo[@resourcePath=""]/toolChain/tool[@superClass="{CC}"]')
        node=ET.SubElement(config,'fileInfo',id='user.debug',resourcePath='Drivers/BSP/src/BSP_Ambient.c')
        tool=ET.SubElement(node,'tool',id='user.tool',superClass=root.get('id'))
        cls=CC+'.option.optimization.level'
        ET.SubElement(tool,'option',id='user.optimization',superClass=cls,value=cls+'.value.og',valueType='enumerated')
        before=ET.tostring(node);self.apply(config)
        self.assertEqual(before,ET.tostring(node))

    def test_hidden_optimization_and_lto_rejected(self):
        for flag in ('-flto','-flto=auto','-Oz','-Ofast','-O','-O0 -fno-lto'):
            config=copy.deepcopy(self.configs['Debug']);self.apply(config)
            root=config.find(f'folderInfo[@resourcePath=""]/toolChain/tool[@superClass="{CC}"]')
            option=root.find(f'option[@superClass="{CC}.option.otherflags"]')
            ET.SubElement(option,'listOptionValue',value=flag)
            with self.assertRaises(ValueError):validate_debug(config)

    def test_stale_owned_exception_rejected(self):
        config=self.configs['Debug'];self.apply(config)
        ET.SubElement(config,'fileInfo',id=config.get('id')+'.noodoe.product.stale',
                      resourcePath='App_Logic/stale.c')
        with self.assertRaisesRegex(ValueError,'Obsolete owned'):validate_debug(config)
        self.apply(config)

    def test_release_not_touched_by_debug_policy(self):
        config=self.configs['Release'];before=ET.tostring(config)
        sync_debug(config);validate_debug(config)
        self.assertEqual(before,ET.tostring(config))

    def test_product_release_lto_boundaries_and_profile_rollback(self):
        config=self.configs['Release'];self.apply(config)
        first=ET.tostring(config);self.apply(config)
        self.assertEqual(first,ET.tostring(config))
        self.assertEqual({x.get('resourcePath') for x in config.findall('fileInfo')},
                         subject.PHOTO_LTO_FILES|subject.PRODUCT_LTO_BOUNDARIES)
        for path in subject.PRODUCT_LTO_BOUNDARIES:
            tool=config.find(f'fileInfo[@resourcePath="{path}"]/tool')
            flags=[x.get('value') for x in tool.findall('.//listOptionValue')]
            self.assertIn('-Os -fno-lto',flags)
            self.assertNotIn(subject.PRODUCT_LTO_FLAGS,flags)
        self.apply(config,False)
        self.assertFalse(config.findall('fileInfo'))
        self.assertFalse(any(x.get('value')==subject.PRODUCT_LTO_FLAGS for x in config.findall('.//listOptionValue')))
        self.apply(config,True);self.assertEqual(first,ET.tostring(config))

    def test_release_user_conflict_not_deleted(self):
        config=self.configs['Release'];self.apply(config,False)
        path=sorted(subject.PHOTO_LTO_FILES)[0]
        node=ET.SubElement(config,'fileInfo',id='user.conflict',resourcePath=path)
        with self.assertRaisesRegex(ValueError,'Conflicting user'):self.apply(config)
        self.assertIn(node,list(config))

    def test_stack_reports_belong_to_correct_codegen_stage(self):
        for name,config in self.configs.items():
            self.apply(config)
            if name=='Debug':
                self.assertFalse(config.findall('.//outputType'))
                self.assertFalse(any('.noodoe.lto_artifact.' in x.get('id','') for x in config.findall('.//option')))
            else:
                root=config.find(f'folderInfo[@resourcePath=""]/toolChain/tool[@superClass="{CC}"]')
                for suffix in ('fstackusage','cyclomaticcomplexity'):
                    self.assertEqual(root.find(f'option[@superClass="{CC}.option.{suffix}"]').get('value'),'false')
                for path in subject.PRODUCT_LTO_BOUNDARIES:
                    tool=config.find(f'fileInfo[@resourcePath="{path}"]/tool')
                    self.assertEqual({x.get('outputs') for x in tool.findall('outputType')},{'su','cyclo'})

if __name__=='__main__':unittest.main(verbosity=2)
