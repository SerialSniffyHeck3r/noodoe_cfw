#ifndef PRODUCT_CALL_ICON_H
#define PRODUCT_CALL_ICON_H
#include "lvgl.h"
const lv_image_dsc_t *Product_CallIcon(void);
void ProductCall_Hints(uint32_t state,uint32_t scope,uint32_t alpha,uint32_t now);
#endif
