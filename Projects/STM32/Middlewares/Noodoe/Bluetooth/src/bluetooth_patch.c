#include "bluetooth_port.h"
#include "btstack_chipset_cc256x.h"
#include <stddef.h>
/* Generated with the pinned upstream converter; source BTS and TI license
 * are retained beside these assets. Classic script only, no BLE/AVPR add-on. */
#if NOODOE_PRODUCT
#include "Resources.h"
/* Pinned BTstack links a default script even when every supported controller
 * supplies a custom script before chipset_init. This empty sentinel is never
 * selected by Bluetooth_SelectPatch; unsupported/missing resources fail first. */
const uint8_t cc256x_init_script[1]={0};
const uint32_t cc256x_init_script_size=0;
#else
#include "cc256xb.inc"
#define cc256x_init_script cc256xc_init_script
#define cc256x_init_script_size cc256xc_init_script_size
#define cc256x_init_script_lmp_subversion cc256xc_init_script_lmp_subversion
#define btstack_chipset_cc256x_lmp_subversion cc256xc_lmp_subversion
#include "cc256xc.inc"
#undef cc256x_init_script
#undef cc256x_init_script_size
#undef cc256x_init_script_lmp_subversion
#undef btstack_chipset_cc256x_lmp_subversion
#endif

/* HCI Read Local Version is emitted before chipset/baud initialization by
 * pinned BTstack. Never infer the silicon from the board's year or package. */
int Bluetooth_SelectPatch(uint16_t manufacturer,uint16_t lmp,uint32_t *bytes)
{
    const uint8_t *script=NULL;
    uint32_t size=0U;
    if (manufacturer!=13U || bytes==NULL) return -1;
#if NOODOE_PRODUCT
    ResourceView asset;uint32_t id=lmp==0x1b90U?RESOURCE_BT_CC256XB:lmp==0x9a1aU?RESOURCE_BT_CC256XC:0;
    if(!Resources_Get(id,&asset))return -1;
    script=asset.data;size=asset.bytes;
#else
    if (lmp==0x1b90U) { script=cc256x_init_script; size=cc256x_init_script_size; }
    if (lmp==0x9a1aU) { script=cc256xc_init_script; size=cc256xc_init_script_size; }
#endif
    if (script==NULL) return -1;
    btstack_chipset_cc256x_set_init_script((uint8_t *)script,size);
    *bytes=size;
    return 0;
}
const btstack_chipset_t *Bluetooth_ChipsetInstance(void)
{ return btstack_chipset_cc256x_instance(); }
