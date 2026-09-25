#ifndef PRODUCT_FUEL_VECTOR_H
#define PRODUCT_FUEL_VECTOR_H
#include "lvgl.h"
/* Material Rounded pump geometry, drawn at the requested output resolution.
 * center refers to the visible outline, rather than the SVG's padded canvas.
 * No bitmap, texture allocation, or driver state is owned by this renderer. */
void Product_FuelVector(lv_layer_t *layer,int cx,int cy,unsigned size,
                        uint32_t color,uint8_t opacity);
#endif
