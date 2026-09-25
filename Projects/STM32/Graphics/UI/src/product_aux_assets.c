#include "Product_AuxAssets.h"
/* Preserve initialization ordering: default theme/font lookup may happen in
 * lv_init, so every descriptor must be bound before Graphics_Init is called. */
uint32_t Product_AuxAssetsBind(void)
{
#if NOODOE_PRODUCT
    return Product_ModeIconsBind()&&
        Product_TripIconsBind()&&Product_MusicIconsBind()&&
        Product_SettingsIconsBind()&&Product_FooterIconsBind()&&Product_StatusIconsBind();
#else
    return 1;
#endif
}
