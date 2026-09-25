#ifndef GRAPHICS_SUBPIXEL_ARC_H
#define GRAPHICS_SUBPIXEL_ARC_H
#include "lvgl.h"
/* Owner-task draw API. Angles are hundredths of a degree; radius is the outer
 * edge and width is the full stroke, in pixels. Queues an ordinary LVGL task
 * with an explicit private tag for our EVE dispatcher, not the vendor arc.
 * The task owns its copied values: later speed packets cannot change it. */
void Graphics_DrawSubpixelArc(lv_layer_t *layer,int32_t x,int32_t y,uint32_t radius,
                             uint32_t width,uint32_t start100,uint32_t end100,uint32_t rgb);
/* EVE owner only, inside its open command burst. Returns1 only when it drew
 * our tagged task; caller then marks it finished. No framebuffer or SPI owner
 * changes. Unrelated LVGL arcs return0 and retain the upstream renderer. */
uint32_t Graphics_DispatchSubpixelArc(lv_draw_task_t *task);
/* Claim the tagged task during LVGL evaluation, before dispatch. The upstream
 * EVE evaluator deliberately rejects every non-NULL user_data descriptor. */
uint32_t Graphics_EvaluateSubpixelArc(lv_draw_task_t *task);
void Graphics_DrawSubpixelArcOpacity(lv_layer_t *layer,int32_t x,int32_t y,uint32_t radius,uint32_t width,uint32_t start100,uint32_t end100,uint32_t rgb,uint32_t opacity);
#endif
