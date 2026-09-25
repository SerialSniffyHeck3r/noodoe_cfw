#include "Gps_LineBatch.h"
#include "src/draw/lv_draw_private.h"
#include "src/draw/eve/lv_eve.h"
static const uint8_t tag;
static uint32_t IsBatch(lv_draw_task_t *t)
{return t->type==LV_DRAW_TASK_TYPE_LINE&&((lv_draw_line_dsc_t*)t->draw_dsc)->base.user_data==&tag;}
void GpsLineBatch_Draw(lv_layer_t *l,lv_draw_line_dsc_t *d,lv_point_precise_t *points,uint32_t count)
{
 if(count<2)return;
 d->base.user_data=(void*)&tag;d->points=points;d->point_cnt=count;
 lv_draw_line(l,d); /* LVGL copies all points before this stack buffer expires. */
 d->base.user_data=NULL;d->points=NULL;d->point_cnt=0;
}
uint32_t GpsLineBatch_Evaluate(lv_draw_task_t *t)
{if(!IsBatch(t))return 0;t->preference_score=0;t->preferred_draw_unit_id=9;return 1;}
/* Color/clip/width are shared by the entire immutable batch. Starting a fresh
 * LINE_STRIP for each pair preserves rounded caps and never joins gap endpoints.
 * At most three DL words per segment, instead of a complete state prologue and
 * epilogue per segment. All48 route samples and all grid fade bands remain. */
uint32_t GpsLineBatch_Dispatch(lv_draw_task_t *t)
{
 if(!IsBatch(t))return 0;
 const lv_draw_line_dsc_t *d=t->draw_dsc;
 lv_eve_scissor(t->clip_area.x1,t->clip_area.y1,t->clip_area.x2,t->clip_area.y2);
 lv_eve_save_context();lv_eve_color(d->color);lv_eve_color_opa(d->opa);lv_eve_line_width(d->width*8U);
 for(uint32_t i=0;i+1<d->point_cnt;i++){
  const lv_point_precise_t *a=&d->points[i],*b=&d->points[i+1];
  if(a->x==LV_DRAW_LINE_POINT_NONE||b->x==LV_DRAW_LINE_POINT_NONE)continue;
  EVE_cmd_dl_burst(DL_BEGIN|EVE_LINE_STRIP);
  lv_eve_vertex_2f(a->x,a->y);lv_eve_vertex_2f(b->x,b->y);
 }
 lv_eve_restore_context();return 1;
}
