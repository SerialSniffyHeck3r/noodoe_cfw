#ifndef PRODUCT_THEME_H
#define PRODUCT_THEME_H
#include <stdint.h>
#include "lvgl.h"
/* Product-owned palette transform. Pixel images deliberately bypass it;
 * A4 glyphs and semantic UI colors use it at the final EVE draw boundary. */
uint32_t Theme_Color(uint32_t dark_color);
uint32_t Theme_LightAmount(void);
void Theme_SetLight(uint32_t light,uint32_t now);
void Theme_Tick(uint32_t now);
void Graphics_ColorRaw(lv_color_t color);
#endif
