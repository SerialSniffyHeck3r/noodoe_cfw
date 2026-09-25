#ifndef PRODUCT_NUMBER_FONTS_H
#define PRODUCT_NUMBER_FONTS_H
#include "lvgl.h"
/* D-DIN Regular for clock/distance/metrics. 0..9 occupy stable centered slots;
 * punctuation and spaces keep their own widths. Glyph shapes are unscaled.
 * Units/labels must use Product_TextFont, not this numeric subset. */
LV_FONT_DECLARE(product_number_32);
LV_FONT_DECLARE(product_number_36);
LV_FONT_DECLARE(product_number_48);
LV_FONT_DECLARE(product_number_64);
LV_FONT_DECLARE(product_number_160);
/* Return native numeric metrics; 160px is composed into an existing A4 page
 * bank, never uploaded as a complete GPU font. NULL if unsupported. */
const lv_font_t *Product_NumberFont(uint32_t pixels);
uint32_t Product_NumberFontsBind(void);
#endif
