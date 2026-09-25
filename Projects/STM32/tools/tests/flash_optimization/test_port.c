/* Compare upstream and integer-port draw operations, not a second renderer.
 * Both paths call the same bounded recorder and the real upstream sine table. */
#include <stdint.h>
#include <string.h>
#include "src/draw/eve/lv_eve.h"
#include "src/draw/eve/lv_draw_eve_private.h"
#include "src/draw/lv_draw_private.h"
#include "src/stdlib/lv_sprintf.h"

void ReferenceArc(lv_draw_task_t *, const lv_draw_arc_dsc_t *, const lv_area_t *);
int ReferenceSnprintf(char *, size_t, const char *, ...);
volatile unsigned test_cases, format_cases, failed_case;
static uint32_t trace[512], reference[512], count, overflow;
void lv_log_add(lv_log_level_t level,const char *file,int line,const char *func,const char *format,...)
{(void)level;(void)file;(void)line;(void)func;(void)format;overflow=1;}
void Graphics_AssertFail(const char *file,uint32_t line)
{(void)file;failed_case=line;for(;;)__asm__("bkpt #0");}
static void put(uint32_t v){if(count<512)trace[count++]=v;else overflow=1;}
#define E0(id) put(id)
#define E1(id,a) do{put(id);put(a);}while(0)
#define E2(id,a,b) do{put(id);put(a);put(b);}while(0)
#define E3(id,a,b,c) do{put(id);put(a);put(b);put(c);}while(0)
#define E4(id,a,b,c,d) do{put(id);put(a);put(b);put(c);put(d);}while(0)
void lv_eve_save_context(void){E0(1);}
void lv_eve_restore_context(void){E0(2);}
void lv_eve_scissor(uint16_t a,uint16_t b,uint16_t c,uint16_t d){E4(3,a,b,c,d);}
void lv_eve_primitive(uint8_t a){E1(4,a);}
void lv_eve_color(lv_color_t a){E3(5,a.red,a.green,a.blue);}
void lv_eve_color_opa(lv_opa_t a){E1(6,a);}
void lv_eve_line_width(int32_t a){E1(7,a);}
void lv_eve_vertex_2f(int16_t a,int16_t b){E2(8,a,b);}
void lv_eve_color_mask(uint8_t a,uint8_t b,uint8_t c,uint8_t d){E4(9,a,b,c,d);}
void lv_eve_stencil_func(uint8_t a,uint8_t b,uint8_t c){E3(10,a,b,c);}
void lv_eve_stencil_op(uint8_t a,uint8_t b){E2(11,a,b);}
void lv_eve_blend_func(uint8_t a,uint8_t b){E2(12,a,b);}
void lv_eve_draw_circle_simple(int16_t a,int16_t b,uint16_t c){E3(13,a,b,c);}
void lv_eve_draw_rect_simple(int16_t a,int16_t b,int16_t c,int16_t d,uint16_t e){E4(14,a,b,c,d);put(e);}

/* Check every mask quadrant and near-full arcs, including transparent/zero
 * width/identical-angle exits, rounded ends and normal panel radii. */
int test_arcs(void)
{
    lv_draw_task_t t={0};lv_draw_arc_dsc_t d={0};lv_area_t a={0,0,479,479};
    const unsigned radii[]={1,16,120,240,480};t.clip_area=a;d.center.x=240;d.center.y=240;
    d.color.red=67;d.color.green=133;d.color.blue=201;
    for(unsigned r=0;r<5;r++)for(unsigned start=0;start<360;start+=7)
    for(unsigned end=0;end<360;end+=11)for(unsigned rounded=0;rounded<2;rounded++){
        unsigned k=++test_cases;d.radius=radii[r];d.width=k%17==0?0:1+k%radii[r];
        d.start_angle=start;d.end_angle=end;d.rounded=rounded;d.opa=k%19==0?0:(k%3==0?100:255);
        count=overflow=0;ReferenceArc(&t,&d,&a);unsigned n=count;
        if(overflow){failed_case=k;return __LINE__;}
        memcpy(reference,trace,n*sizeof(*trace));count=0;
        lv_draw_eve_arc(&t,&d,&a);
        if(overflow||count!=n||memcmp(trace,reference,n*sizeof(*trace))){failed_case=k;return __LINE__;}
    }
    return 0;
}

/* Exercise the actual newlib-nano/LVGL adapters on ARM: returned length,
 * truncation, terminators, zero-sized buffers and fixed-width UI formats. */
#define FMT(...) do { \
    for(size_t cap=0;cap<80;cap++){ \
        char old[96],now[96];memset(old,0x5a,sizeof old);memset(now,0x5a,sizeof now); \
        int x=ReferenceSnprintf(old,cap,__VA_ARGS__);int y=lv_snprintf(now,cap,__VA_ARGS__); \
        ++format_cases;if(x!=y||memcmp(old,now,sizeof old)){failed_case=format_cases;return __LINE__;} \
    } \
}while(0)
int test_format(void)
{
    FMT("%s", "Rider / \xe5\x8f\xb0\xe7\x81\xa3");FMT("%.3s", "200");
    FMT("%lu:%02lu:%02lu",123UL,4UL,59UL);FMT("%lu:%02lu",0UL,7UL);
    FMT("%lu.%lu",99999UL,9UL);FMT("%08lX",0xdeadbeefUL);
    FMT("%s%ld", "+",2L);FMT("%ld",-2147483647L-1L);FMT("%lu",4294967295UL);
    FMT("%u / %u",0U,10U);FMT("%c %d %x %%",'O',-3,0x1234U);
    FMT("%6u %-6s", 123U,"km");FMT("%.*s",3,"abcdef");
    FMT("CFW UPDATE\n%s\nStage %lu / 8\nSector %lu / %lu","Checking",7UL,95UL,96UL);
    if(lv_snprintf(0,0,"%lu:%02lu",12UL,4UL)!=5)return __LINE__;
    return 0;
}
