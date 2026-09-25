/* Exercise actual port code and pinned sine table; intercept EVE commands. */
#include "Graphics_SubpixelArc.h"
#include "src/draw/lv_draw_private.h"
#include "src/draw/eve/lv_eve.h"
#include <string.h>
static lv_draw_arc_dsc_t copy;
static uint32_t count,last,width,vertex_format,queued;
uint32_t endpoints[10001],max_vertices;
lv_color_t lv_color_hex(uint32_t rgb){return (lv_color_t){.red=rgb>>16,.green=rgb>>8,.blue=rgb};}
void lv_draw_arc_dsc_init(lv_draw_arc_dsc_t *d){memset(d,0,sizeof(*d));d->opa=255;}
void lv_draw_arc(lv_layer_t *layer,const lv_draw_arc_dsc_t *d){(void)layer;copy=*d;++queued;}
void lv_eve_save_context(void){}
void lv_eve_restore_context(void){}
void lv_eve_scissor(uint16_t a,uint16_t b,uint16_t c,uint16_t d){(void)a;(void)b;(void)c;(void)d;}
void lv_eve_primitive(uint8_t p){(void)p;}
void lv_eve_color(lv_color_t c){(void)c;}
void lv_eve_color_opa(lv_opa_t o){(void)o;}
void lv_eve_line_width(int32_t w){width=w;}
void EVE_cmd_dl_burst(uint32_t command){if((command>>30)==1U){last=command;++count;}else if((command>>24)==0x27)vertex_format=command&7;}
#define CHECK(x) do{if(!(x))return __LINE__;}while(0)
unsigned test_arc(void)
{
    lv_layer_t layer={0};lv_draw_task_t task={.type=LV_DRAW_TASK_TYPE_ARC,.draw_dsc=&copy,.clip_area={0,0,479,479}};
    Graphics_DrawSubpixelArc(&layer,240,240,240,18,13500,13500,0xffffff);CHECK(!queued);
    for(uint32_t value=1;value<=10000;++value){
        count=0;Graphics_DrawSubpixelArc(&layer,240,240,240,18,13500,13500+value*27000U/10000U,0xffffff);
        CHECK(Graphics_EvaluateSubpixelArc(&task)==1&&task.preferred_draw_unit_id==9&&task.preference_score==0);
        CHECK(Graphics_DispatchSubpixelArc(&task)==1&&width==144&&vertex_format==4);
        CHECK(count>=2&&count<=91);if(count>max_vertices)max_vertices=count;
        endpoints[value]=last;
    }
    copy.base.user_data=NULL;CHECK(!Graphics_EvaluateSubpixelArc(&task)&&!Graphics_DispatchSubpixelArc(&task));
    task.type=LV_DRAW_TASK_TYPE_LABEL;CHECK(!Graphics_DispatchSubpixelArc(&task));
    return 0;
}
