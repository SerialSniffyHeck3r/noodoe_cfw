#ifndef GRAPHICS_BACKGROUND_DRAW_H
#define GRAPHICS_BACKGROUND_DRAW_H
#include "lvgl.h"
/* Only the background view queues this private tagged fill; unrelated LVGL
 * fills retain their upstream renderer and task dependency rules. */
void GraphicsBackground_Draw(lv_layer_t *layer);
uint32_t GraphicsBackground_Evaluate(lv_draw_task_t *task);
uint32_t GraphicsBackground_Dispatch(lv_draw_task_t *task);
#endif
