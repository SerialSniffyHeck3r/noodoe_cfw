#include "Graphics_SubpixelArc.h"
#include "src/draw/lv_draw_private.h"
#include "src/draw/eve/lv_eve.h"

static const uint8_t arc_tag;

/* A tag is insufficient without an explicit unit preference: LVGL finalizes
 * unclaimed tasks as skipped before the dispatch callback can see them. */
uint32_t Graphics_EvaluateSubpixelArc(lv_draw_task_t *task)
{
    if(task->type!=LV_DRAW_TASK_TYPE_ARC||((lv_draw_arc_dsc_t*)task->draw_dsc)->base.user_data!=&arc_tag)return 0;
    task->preference_score=0;task->preferred_draw_unit_id=9;return 1;
}

/* lv_draw_arc copies the full descriptor and always queues the outer-circle
 * bounding box. The tag changes angle units for this project-owned dispatch
 * path only. No pointers to mutable UI values and no vendor modifications. */
void Graphics_DrawSubpixelArcOpacity(lv_layer_t *layer,int32_t x,int32_t y,uint32_t radius,
                             uint32_t width,uint32_t start100,uint32_t end100,uint32_t rgb,uint32_t opacity)
{
    if(!layer||!width||width>radius||radius>512U||start100>=end100||end100-start100>36000U||end100>72000U)return;
    lv_draw_arc_dsc_t d;lv_draw_arc_dsc_init(&d);
    d.base.user_data=(void*)&arc_tag;d.center.x=x;d.center.y=y;
    d.opa=opacity>255?255:opacity;d.radius=radius;d.width=width;d.rounded=1;d.color=lv_color_hex(rgb);
    d.start_angle=start100;d.end_angle=end100;lv_draw_arc(layer,&d);
}

/* Interpolate the pinned LVGL1-degree Q15 sine table before rounding to EVE
 * 1/16px vertices. All products fit signed32bits for the API's512px bound.
 * Angles wrap in geometry only; a135..405degree sweep remains continuous. */
static int32_t Sine(uint32_t angle100)
{
    uint32_t degree=(angle100/100U)%360U,fraction=angle100%100U;
    int32_t a=lv_trigo_sin(degree),b=lv_trigo_sin((degree+1U)%360U);
    return a+(b-a)*(int32_t)fraction/100;
}

/* Rounded EVE line-strip segments approximate the centerline every3degrees.
 * At radius231 the maximum chord sag is0.080px; endpoints retain0.01degree
 * input and1/16px output precision. The ring's width/caps and circle stencil
 * stay unchanged. One complete270degree ring costs91 vertices, bounded. */
uint32_t Graphics_DispatchSubpixelArc(lv_draw_task_t *task)
{
    if(task->type!=LV_DRAW_TASK_TYPE_ARC)return 0;
    const lv_draw_arc_dsc_t *d=task->draw_dsc;
    if(d->base.user_data!=&arc_tag)return 0;
    lv_eve_scissor(task->clip_area.x1,task->clip_area.y1,task->clip_area.x2,task->clip_area.y2);
    lv_eve_save_context();lv_eve_color(d->color);lv_eve_color_opa(d->opa);
    EVE_cmd_dl_burst(VERTEX_FORMAT(4));
    lv_eve_line_width(d->width*8U);lv_eve_primitive(LV_EVE_PRIMITIVE_LINE_STRIP);
    int32_t radius16=(int32_t)d->radius*16-(int32_t)d->width*8;
    uint32_t angle=d->start_angle,end=d->end_angle;
    for(;;){
        int32_t x=d->center.x*16+(radius16*Sine(angle+9000U)+16384)/32768;
        int32_t y=d->center.y*16+(radius16*Sine(angle)+16384)/32768;
        EVE_cmd_dl_burst(VERTEX2F(x,y));
        if(angle==end)break;
        angle=end-angle>300U?angle+300U:end;
    }
    lv_eve_restore_context();return 1;
}

/* Existing callers retain opaque behavior. */
void Graphics_DrawSubpixelArc(lv_layer_t *layer,int32_t x,int32_t y,uint32_t radius,uint32_t width,uint32_t start100,uint32_t end100,uint32_t rgb)
{Graphics_DrawSubpixelArcOpacity(layer,x,y,radius,width,start100,end100,rgb,255);}
