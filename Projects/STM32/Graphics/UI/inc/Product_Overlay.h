#ifndef PRODUCT_OVERLAY_H
#define PRODUCT_OVERLAY_H
#include "lvgl.h"
/* Shared construction only. Each caller owns its visibility, text and input. */
typedef struct {int16_t x,y,w,h,lx,ly,lw,lh;uint8_t radius,alpha;uint32_t color;} ProductOverlayLayout;
uint32_t ProductOverlay_Create(lv_obj_t *screen,lv_obj_t **panel,lv_obj_t **label,const ProductOverlayLayout *layout);
#endif
