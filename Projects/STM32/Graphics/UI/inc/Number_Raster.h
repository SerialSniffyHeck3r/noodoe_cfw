#ifndef NUMBER_RASTER_H
#define NUMBER_RASTER_H
#include "lvgl.h"
/* Compose up to three native glyphs into an existing A4 page bank. The bank
 * must have retired from EVE scanout before its owner calls this function. */
uint32_t NumberRaster_Compose(uint8_t *pixels,uint32_t bytes,const lv_font_t *font,const char *text);
#endif
