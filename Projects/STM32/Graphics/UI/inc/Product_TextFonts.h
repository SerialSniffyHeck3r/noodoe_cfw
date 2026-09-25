#ifndef PRODUCT_TEXT_FONTS_H
#define PRODUCT_TEXT_FONTS_H
#include "lvgl.h"
/* Lato Regular: natural letter/space advances. Fixed UI boxes belong to the
 * view, never to this font. Current English UI subset is ASCII 32..126. */
LV_FONT_DECLARE(product_text_20);
LV_FONT_DECLARE(product_text_24);
LV_FONT_DECLARE(product_text_32);
/* Return a static font for 20/24/32px; NULL for an unsupported size. */
const lv_font_t *Product_TextFont(uint32_t pixels);
uint32_t Product_TextFontsBind(void);
#endif
