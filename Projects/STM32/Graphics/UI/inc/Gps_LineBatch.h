#ifndef GPS_LINE_BATCH_H
#define GPS_LINE_BATCH_H
#include "lvgl.h"
/* One immutable copied point array per style; NONE separates clipped legs.
 * No new EVE primitive, framebuffer, or modification of the vendor driver. */
void GpsLineBatch_Draw(lv_layer_t *,lv_draw_line_dsc_t *,lv_point_precise_t *,uint32_t);
uint32_t GpsLineBatch_Evaluate(lv_draw_task_t *);
uint32_t GpsLineBatch_Dispatch(lv_draw_task_t *);
#endif
