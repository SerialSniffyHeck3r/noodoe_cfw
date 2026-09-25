#ifndef PRODUCT_FONT_BINDING_H
#define PRODUCT_FONT_BINDING_H
#include "lvgl.h"
typedef struct {uint32_t resource;lv_font_fmt_txt_dsc_t *descriptor;} ProductFontBinding;
/* Bind already validated SDRAM bitmaps once, before LVGL starts drawing. */
uint32_t Product_FontsBindTable(const ProductFontBinding *table,uint32_t count);
#endif
