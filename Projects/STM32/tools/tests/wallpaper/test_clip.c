#include "Graphics_BackgroundClip.h"
#include "SpeedHome_Layout.h"
#include "src/draw/eve/lv_eve.h"
static uint32_t clears,primitive,count_a,count_b,bad,write_color,compare_zero,keep;
void EVE_cmd_dl_burst(uint32_t command)
{
    if(command==(DL_CLEAR|CLR_STN))++clears;
    else if(command!=STENCIL_MASK(255)&&command!=CLEAR_STENCIL(0))bad=1;
}
void lv_eve_color_mask(uint8_t r,uint8_t g,uint8_t b,uint8_t a)
{if(r!=g||g!=b||b!=a)bad=1;write_color=r;}
void lv_eve_stencil_func(uint8_t func,uint8_t ref,uint8_t mask)
{if(mask!=255||!((func==EVE_ALWAYS&&ref==1)||(func==EVE_EQUAL&&!ref)))bad=1;compare_zero=func==EVE_EQUAL;}
void lv_eve_stencil_op(uint8_t fail,uint8_t pass)
{if(fail!=pass||(fail!=EVE_REPLACE&&fail!=EVE_KEEP))bad=1;keep=fail==EVE_KEEP;}
void lv_eve_primitive(uint8_t p){primitive=p;}
void lv_eve_vertex_2f(int16_t x,int16_t y)
{
    if(write_color||compare_zero||keep)bad=1;
    const int16_t (*points)[2];uint32_t n;
    if(primitive==LV_EVE_PRIMITIVE_EDGE_STRIP_A){points=speed_home_separator;n=count_a++;}
    else if(primitive==LV_EVE_PRIMITIVE_EDGE_STRIP_B){points=speed_home_footer_separator;n=count_b++;}
    else{bad=1;return;}
    if(n>=4||x!=points[n][0]||y!=points[n][1])bad=1;
}
/* Inspect actual C command emission; no software pixel renderer substitutes
 * for the separate real EVE snapshot verification. */
uint32_t test_separator_clip(void)
{
    clears=primitive=count_a=count_b=bad=write_color=compare_zero=keep=0;
    GraphicsBackground_ClipBegin();
    if(bad||clears!=1||count_a!=4||count_b!=4||!write_color||!compare_zero||!keep)return __LINE__;
    GraphicsBackground_ClipEnd();if(bad||clears!=2)return __LINE__;
    return 0;
}
