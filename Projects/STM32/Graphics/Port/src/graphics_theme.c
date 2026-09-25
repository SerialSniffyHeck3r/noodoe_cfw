#include "Product_Theme.h"
void __real_lv_eve_color(lv_color_t color);
/* Link interposition keeps the pinned EVE driver unchanged. The defining
 * vendor TU remains outside LTO; cache comparison happens after mapping. */
void __wrap_lv_eve_color(lv_color_t color)
{__real_lv_eve_color(lv_color_hex(Theme_Color(lv_color_to_u32(color)&0xffffffU)));}
void Graphics_ColorRaw(lv_color_t color){__real_lv_eve_color(color);}
