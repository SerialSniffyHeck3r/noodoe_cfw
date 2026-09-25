#ifndef PRODUCT_ICONS_H
#define PRODUCT_ICONS_H
#include "lvgl.h"
#include <stdint.h>
/* All new product icons use Google's Material Icons Round family. Numeric
 * and text fonts remain independent; no generic LV_SYMBOL fallback. */
typedef enum {PRODUCT_ICON_BUILD=0,PRODUCT_ICON_LOCAL_GAS_STATION,PRODUCT_ICON_COUNT} ProductIcon;
const lv_font_t *Product_IconFont(uint32_t pixels);
const char *Product_IconText(ProductIcon icon);
#endif
