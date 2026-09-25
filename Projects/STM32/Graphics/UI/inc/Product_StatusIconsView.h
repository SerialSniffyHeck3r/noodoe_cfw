#ifndef PRODUCT_STATUS_ICONS_VIEW_H
#define PRODUCT_STATUS_ICONS_VIEW_H
#include "lvgl.h"
uint32_t ProductStatusIcons_Create(lv_obj_t*);
void ProductStatusIcons_SetCall(uint32_t active);
/* dash_mode is the current physical switch position, independent of button
 * presses. The caller excludes power/install/recovery overlays. */
void ProductStatusIcons_Render(uint32_t visible,uint32_t bt,uint32_t gps,uint32_t dash_mode);
#endif
