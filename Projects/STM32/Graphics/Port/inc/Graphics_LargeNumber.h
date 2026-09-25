#ifndef GRAPHICS_LARGE_NUMBER_H
#define GRAPHICS_LARGE_NUMBER_H
#include "lvgl.h"
/* Exactly three fixed 82.5px cells, right aligned without leading zeroes.
 * text must remain alive until this frame is submitted. */
void Graphics_DrawLargeNumber(lv_layer_t*,const lv_font_t*,const char*,int x,int bottom,uint32_t opacity);
uint32_t Graphics_LargeNumberEvaluate(lv_draw_task_t *task);
#endif
