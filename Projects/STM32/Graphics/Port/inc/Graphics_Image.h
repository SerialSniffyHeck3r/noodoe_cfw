#ifndef GRAPHICS_IMAGE_H
#define GRAPHICS_IMAGE_H
#include "lvgl.h"
/* Graphics-owner only, outside an active frame. Refresh a cached mutable
 * RGB565 image in place. Reuses its existing RAM_G block; no cache leak. */
void Graphics_ImageChanged(const lv_image_dsc_t *image);
#endif
