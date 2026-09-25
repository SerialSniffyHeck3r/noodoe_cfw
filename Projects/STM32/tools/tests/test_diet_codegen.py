"""Read-only generation policy tests; no Cube, target or generated-file edits."""
import json
from pathlib import Path
import sys
import unittest

P=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(P/'tools'))
import check_generated as subject


class DietGenerationTests(unittest.TestCase):
    def setUp(self):
        self.contract=json.loads((P/'tools/generated_contract.json').read_text(encoding='utf-8'))
        paths={'Core/Src/main.c','Core/Inc/stm32f4xx_hal_conf.h'}
        paths.update(self.contract['forbidden_generated_symbols'])
        for stem in self.contract['peripheral_files']:
            paths.update((f'Core/Src/{stem}.c',f'Core/Inc/{stem}.h'))
        self.files={p:(P/p).read_text(encoding='utf-8-sig') for p in paths}
        # In-memory policy fixture only. This is deliberately not compiled or
        # presented as a Cube regeneration or an installable firmware build.
        for p, names in self.contract['forbidden_generated_symbols'].items():
            for name in names:self.files[p]=self.files[p].replace(name,'test_retired_symbol_removed')
        p='Core/Inc/stm32f4xx_hal_conf.h'
        self.files[p]=self.files[p].replace('#define HAL_PCD_MODULE_ENABLED','/* #define HAL_PCD_MODULE_ENABLED */')

    def test_remaining_peripheral_contract(self):
        self.assertEqual(subject.validate_sources(self.contract,self.files),[])

    def test_usb_irq_rejected_before_link(self):
        self.files['Core/Src/stm32f4xx_it.c']+='\nvoid retired(void){HAL_PCD_IRQHandler(&hpcd_USB_OTG_HS);}\n'
        errors=subject.validate_sources(self.contract,self.files)
        self.assertEqual(sum('retired USB symbol' in e for e in errors),2)

    def test_disabled_hal_conditional_is_allowed(self):
        self.files['Core/Inc/stm32f4xx_hal_conf.h']+='\n#ifdef HAL_PCD_MODULE_ENABLED\n#endif\n'
        self.assertEqual(subject.validate_sources(self.contract,self.files),[])
        self.files['Core/Inc/stm32f4xx_hal_conf.h']+='\n#define HAL_PCD_MODULE_ENABLED\n'
        self.assertTrue(any('must remain disabled' in e for e in subject.validate_sources(self.contract,self.files)))

    def test_ioc_pins_and_disabled_usb(self):
        ioc=(P/(P.name+'.ioc')).read_text(encoding='utf-8-sig')
        self.assertEqual(subject.validate_ioc(self.contract,ioc),[])
        self.assertTrue(any('retired peripheral' in e for e in subject.validate_ioc(self.contract,ioc+'\nUSB_OTG_HS.IPParameters=VirtualMode\n')))
        bad=ioc.replace('PB14.Signal=GPIO_Analog','PB14.Signal=USB_OTG_HS_DM')
        self.assertTrue(any('unused PB14' in e for e in subject.validate_ioc(self.contract,bad)))

    def test_ioc_requires_explicit_hse_pll_source(self):
        ioc=(P/(P.name+'.ioc')).read_text(encoding='utf-8-sig')
        for bad in (ioc.replace('RCC.PLLSourceVirtual=RCC_PLLSOURCE_HSE',''),
                    ioc.replace('RCC_PLLSOURCE_HSE','RCC_PLLSOURCE_HSI')):
            self.assertTrue(subject.validate_ioc(self.contract,bad))


if __name__=='__main__':unittest.main()
