#ifndef PRODUCT_AUX_ASSETS_H
#define PRODUCT_AUX_ASSETS_H
#include <stdint.h>
/* Called by the UI owner after Resources READY, before LVGL initialization.
 * All six functions reject a missing/wrong-sized resource; no I/O or allocation
 * occurs here. Rebinding the same immutable arena is harmless. */
uint32_t Product_AuxAssetsBind(void);
uint32_t Product_MontserratBind(void);
uint32_t Product_ModeIconsBind(void);
uint32_t Product_TripIconsBind(void);
uint32_t Product_MusicIconsBind(void);
uint32_t Product_SettingsIconsBind(void);
uint32_t Product_FooterIconsBind(void);
uint32_t Product_StatusIconsBind(void);
#endif
