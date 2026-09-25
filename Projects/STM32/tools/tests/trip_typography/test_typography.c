#include <stdint.h>
#include <stdio.h>
#include "../../../Graphics/UI/inc/Trip_Layout.h"
typedef struct {int box_h,ofs_y;} lv_font_glyph_dsc_t;
typedef struct {int line_height,base_line;lv_font_glyph_dsc_t glyph[128];} lv_font_t;
typedef int lv_text_align_t;
typedef int PageSlot;
#define SPEED_HOME_CONTENT_Y 105
#define WHITE 0xF2F5F7U
#define LV_TEXT_ALIGN_LEFT 0
#define LV_TEXT_ALIGN_CENTER 1
#include "output/font_metrics.h"
static const lv_font_t *Product_NumberFont(uint32_t px){return px==40?&number_40:&number_36;}
static const lv_font_t *Product_TextFont(uint32_t px){(void)px;return &text_20;}
static int lv_font_get_glyph_dsc(const lv_font_t *f,lv_font_glyph_dsc_t *g,uint8_t c,uint32_t next){(void)next;*g=f->glyph[c];return 1;}
static int actual_x,actual_y,actual_w;
static struct {int x,y,w;uint32_t color,calls;} recorded[12];
static void Label(PageSlot *s,uint32_t id,const char *text,int x,int y,int w,int h,uint32_t px,uint32_t numeric,uint32_t color,lv_text_align_t align)
{(void)s;(void)text;(void)h;(void)px;(void)numeric;(void)align;actual_x=x;actual_y=y;actual_w=w;
recorded[id].x=x;recorded[id].y=y;recorded[id].w=w;recorded[id].color=color;++recorded[id].calls;}
#include "output/trip_label.h"
static uint32_t assertions;
#define CHECK(x) do {++assertions;if(!(x))return __LINE__;} while(0)
uint32_t get_assertions(void){return assertions;}
uint32_t test_fixed_rows(void)
{
    char text[20];PageSlot slot=0;
    /* Includes every tenth between95 and100, decimal carries, width changes,
     * and the reported97.6/97.7/97.8 regression. Numeric baseline is constant. */
    for(unsigned i=0;i<10000;++i){
        snprintf(text,sizeof(text),"%u.%u",i/10,i%10);
        TripLabel(&slot,9,text,196,376,83,36,1,0,0);
        CHECK(actual_y+105==348&&actual_x==196&&actual_w==83);
    }
    for(unsigned i=0;i<=200;++i){
        snprintf(text,sizeof(text),"%u",i);TripLabel(&slot,7,text,40,376,64,36,1,0,0);
        CHECK(actual_y+105==348&&actual_x==40&&actual_w==64);
    }
    TripLabel(&slot,2,"0:00",102,215,132,40,1,0,0);int time_y=actual_y;
    for(unsigned i=0;i<600;++i){snprintf(text,sizeof(text),"%u:%02u",i/60,i%60);
        TripLabel(&slot,2,text,102,215,132,40,1,0,0);CHECK(actual_y==time_y);}
    return 0;
}
uint32_t test_time_padding(void)
{
    PageSlot slot=0;char text[20];
    /* Every displayed minute up to99:59: only the inserted hour0 is gray.
     * 09:59 ->10:00 uses the same five20/8px cells and identical Y. */
    for(unsigned i=0;i<6000;++i){
        recorded[4].calls=0;snprintf(text,sizeof(text),"%u:%02u",i/60,i%60);TripTime(&slot,text);
        CHECK(recorded[2].color==WHITE);
        if(i<600){
            CHECK(recorded[4].calls==1&&recorded[4].color==0x647077U);
            CHECK(recorded[4].x==124&&recorded[4].w==20);
            CHECK(recorded[2].x==144&&recorded[2].w==68);
            CHECK(recorded[4].y==recorded[2].y);
        }else CHECK(recorded[4].calls==0&&recorded[2].x==102&&recorded[2].w==132);
    }
    /* No truncation or invented gray prefix on multi-day totals/unknowns. */
    recorded[4].calls=0;TripTime(&slot,"100:00");CHECK(recorded[4].calls==0&&actual_w==132);
    TripTime(&slot,"--:--");CHECK(recorded[4].calls==0&&actual_w==132);
    return 0;
}
