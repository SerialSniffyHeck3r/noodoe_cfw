/* Actual renderer, real LVGL descriptors and shipped D-DIN metrics. The EVE
 * transport is intercepted; this does not claim physical panel verification. */
#include "Graphics_LargeNumber.h"
#include "src/draw/lv_draw_private.h"
#include "src/draw/eve/lv_draw_eve_private.h"
#include "src/draw/eve/lv_eve.h"
#include <string.h>
#include <stdio.h>
#include "output/metrics.h"
static lv_draw_label_dsc_t copy;
static lv_area_t area;
static lv_font_t font;
static int xs[3],ys[3],widths[3],heights[3];
static unsigned drawn,upstream,scale;
unsigned assertions;
#define CHECK(x) do{++assertions;if(!(x))return __LINE__;}while(0)
lv_color_t lv_color_hex(uint32_t rgb){return (lv_color_t){.red=rgb>>16,.green=rgb>>8,.blue=rgb};}
void lv_draw_label_dsc_init(lv_draw_label_dsc_t *d){memset(d,0,sizeof(*d));}
void lv_draw_label(lv_layer_t *l,const lv_draw_label_dsc_t *d,const lv_area_t *a){(void)l;copy=*d;area=*a;}
bool lv_font_get_glyph_dsc(const lv_font_t *f,lv_font_glyph_dsc_t *g,uint32_t c,uint32_t next)
{(void)f;(void)next;if(c>=128||!metrics[c].box_w)return false;*g=metrics[c];return true;}
void lv_eve_save_context(void){}
void lv_eve_restore_context(void){}
void lv_eve_scissor(uint16_t a,uint16_t b,uint16_t c,uint16_t d){(void)a;(void)b;(void)c;(void)d;}
void lv_eve_primitive(uint8_t p){(void)p;}
void lv_eve_color(lv_color_t c){(void)c;}
void lv_eve_color_opa(lv_opa_t o){(void)o;}
void EVE_cmd_dl_burst(uint32_t c){(void)c;}
void EVE_cmd_scale_burst(int32_t x,int32_t y){scale=x==163840&&y==163840;}
uint32_t lv_draw_eve_label_upload_glyph(bool b,const lv_font_fmt_txt_dsc_t *f,uint32_t id){(void)b;(void)f;return id*1024;}
void lv_eve_bitmap_source(uint32_t a){(void)a;}
void lv_eve_bitmap_layout(uint8_t f,uint16_t s,uint16_t h){(void)f;(void)s;(void)h;}
void lv_eve_bitmap_size(uint8_t f,uint8_t x,uint8_t y,uint16_t w,uint16_t h)
{(void)f;(void)x;(void)y;if(drawn<3){widths[drawn]=w;heights[drawn]=h;}}
void lv_eve_vertex_2f(int16_t x,int16_t y){if(drawn<3){xs[drawn]=x;ys[drawn]=y;}++drawn;}
void __real_lv_draw_eve_label(lv_draw_task_t *t,const lv_draw_label_dsc_t *d,const lv_area_t *a){(void)t;(void)d;(void)a;++upstream;}
void __wrap_lv_draw_eve_label(lv_draw_task_t*,const lv_draw_label_dsc_t*,const lv_area_t*);
unsigned test_number(void)
{
    lv_layer_t layer={0};lv_draw_task_t task={.type=LV_DRAW_TASK_TYPE_LABEL,.draw_dsc=&copy,.clip_area={72,145,407,375}};
    char text[5];
    for(unsigned value=0;value<=400;++value){
        snprintf(text,sizeof(text),"%u",value);drawn=scale=0;
        Graphics_DrawLargeNumber(&layer,&font,text,78,302,255);
        CHECK(Graphics_LargeNumberEvaluate(&task)&&task.preferred_draw_unit_id==9&&task.preference_score==0);
        __wrap_lv_draw_eve_label(&task,&copy,&area);CHECK(scale&&drawn==strlen(text));
        for(unsigned i=0;i<drawn;++i){
            CHECK(xs[i]>=78&&xs[i]+widths[i]<=326&&ys[i]>=145&&ys[i]+heights[i]<=303);
            unsigned cell=3-drawn+i;CHECK(xs[i]>=78+(cell*165)/2&&xs[i]<78+(cell*165)/2+20);
        }
    }
    drawn=0;Graphics_DrawLargeNumber(&layer,&font,"---",78,302,255);
    CHECK(Graphics_LargeNumberEvaluate(&task));__wrap_lv_draw_eve_label(&task,&copy,&area);CHECK(drawn==3);
    copy.base.user_data=0;CHECK(!Graphics_LargeNumberEvaluate(&task));
    __wrap_lv_draw_eve_label(&task,&copy,&area);CHECK(upstream==1);
    task.type=LV_DRAW_TASK_TYPE_FILL;CHECK(!Graphics_LargeNumberEvaluate(&task));
    return 0;
}
