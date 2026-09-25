#include <stdint.h>
#include <string.h>
#define POWER_VIEW_NAME_CAPACITY 49U
#define SUMMARY_SHIFT_Y (-24)
#define LV_COORD_MAX 32767
#define LV_TEXT_FLAG_NONE 0
#define LV_TEXT_ALIGN_LEFT 0
#define LV_TEXT_ALIGN_CENTER 1
#define LV_TEXT_ALIGN_RIGHT 2
typedef struct {int x,y;} lv_point_t;
typedef unsigned lv_font_t;
typedef void lv_layer_t;
typedef void lv_event_t;
static struct {uint32_t kind,alpha;int32_t offset_y;char distance[32],ride[32],unit[4],oil[8],rider_name[49];} model;
static char welcome_line[55];
static uint32_t assertions,calls;
static struct {char text[55];int x,bottom,width,pixels,align;} rows[10];
#include "output/font_metrics.h"
const lv_font_t *Product_TextFont(uint32_t n){static const lv_font_t font=32;return n==32?&font:0;}
static void lv_text_get_size(lv_point_t *size,const char *s,const lv_font_t *f,int a,int b,int c,int d)
{(void)f;(void)a;(void)b;(void)c;(void)d;size->x=0;size->y=32;while(*s)size->x+=advances[(uint8_t)*s++];}
static lv_layer_t *lv_event_get_layer(lv_event_t *e){return e;}
static void Text(lv_layer_t *l,const char *t,int x,int bottom,int width,uint32_t pixels,uint32_t number,uint32_t rgb,int align)
{(void)l;(void)number;(void)rgb;if(calls>=10)return;strcpy(rows[calls].text,t);rows[calls].x=x;rows[calls].bottom=bottom;rows[calls].width=width;rows[calls].pixels=pixels;rows[calls++].align=align;}
#include "output/welcome_code.h"
#define CHECK(x) do{++assertions;if(!(x))return __LINE__;}while(0)
uint32_t test_welcome(void)
{
    model.kind=1;PrepareWelcome();Draw(0);
    CHECK(calls==2&&!strcmp(rows[0].text,"Welcome")&&!strcmp(rows[1].text,"Rider"));
    CHECK(rows[0].bottom==237&&rows[1].bottom==283);
    CHECK(rows[1].x==96&&rows[1].width==288&&rows[1].align==LV_TEXT_ALIGN_CENTER);
    strcpy(model.rider_name,"Alex");PrepareWelcome();CHECK(!strcmp(welcome_line,"Rider Alex"));
    strcpy(model.rider_name,"100% Rider");PrepareWelcome();CHECK(!strcmp(welcome_line,"Rider 100% Rider"));
    strcpy(model.rider_name,"\xED\x99\x8D\xEA\xB8\xB8\xEB\x8F\x99");PrepareWelcome();CHECK(!strcmp(welcome_line,"Rider ???"));
    CHECK(strlen(model.rider_name)==9);
    for(uint32_t n=1;n<=48;++n){
        memset(model.rider_name,'W',n);model.rider_name[n]=0;PrepareWelcome();
        lv_point_t size;lv_text_get_size(&size,welcome_line,Product_TextFont(32),0,0,LV_COORD_MAX,0);
        CHECK(size.x<=288&&!strncmp(welcome_line,"Rider ",6)&&strlen(model.rider_name)==n);
    }
    CHECK(!strcmp(welcome_line+strlen(welcome_line)-3,"..."));
    strcpy(model.rider_name,"Jo");PrepareWelcome();CHECK(!strcmp(welcome_line,"Rider Jo"));
    calls=0;Draw(0);CHECK(rows[1].pixels==32&&rows[1].bottom==283&&calls==2);
    model.rider_name[0]=0;PrepareWelcome();CHECK(!strcmp(welcome_line,"Rider"));
    calls=0;model.kind=2;Draw(0);CHECK(calls==10&&!strcmp(rows[0].text,"Ride Summary"));
    CHECK(rows[0].bottom==161&&rows[1].bottom==222&&rows[4].bottom==278&&rows[7].bottom==334);
    return 0;
}
uint32_t get_assertions(void){return assertions;}
