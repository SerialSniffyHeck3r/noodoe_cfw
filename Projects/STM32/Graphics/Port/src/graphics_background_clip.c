#include "Graphics_BackgroundClip.h"
#include "SpeedHome_Layout.h"
#include "src/draw/eve/lv_eve.h"
/* Clock and ODO caps are excluded while sampling the photo and shade, not
 * painted over afterwards. Monotonic-X edge strips follow the SAME four
 * coordinates as the visible separators. No second bitmap/framebuffer or
 * duplicate approximated trapezoid is needed. The shell's clear stays black. */
void GraphicsBackground_ClipBegin(void)
{
    EVE_cmd_dl_burst(STENCIL_MASK(255));EVE_cmd_dl_burst(CLEAR_STENCIL(0));EVE_cmd_dl_burst(DL_CLEAR|CLR_STN);
    lv_eve_color_mask(0,0,0,0);lv_eve_stencil_func(EVE_ALWAYS,1,255);lv_eve_stencil_op(EVE_REPLACE,EVE_REPLACE);
    lv_eve_primitive(LV_EVE_PRIMITIVE_EDGE_STRIP_A);
    for(unsigned i=0;i<SPEED_HOME_TRAPEZOID_POINTS;++i)lv_eve_vertex_2f(speed_home_separator[i][0],speed_home_separator[i][1]);
    lv_eve_primitive(LV_EVE_PRIMITIVE_EDGE_STRIP_B);
    for(unsigned i=0;i<SPEED_HOME_TRAPEZOID_POINTS;++i)lv_eve_vertex_2f(speed_home_footer_separator[i][0],speed_home_footer_separator[i][1]);
    lv_eve_color_mask(1,1,1,1);lv_eve_stencil_func(EVE_EQUAL,0,255);lv_eve_stencil_op(EVE_KEEP,EVE_KEEP);
}
/* Stencil contents are not part of SAVE/RESTORE_CONTEXT. Clear our temporary
 * bits before the normal context restore, so LVGL arcs/triangles start clean. */
void GraphicsBackground_ClipEnd(void)
{EVE_cmd_dl_burst(CLEAR_STENCIL(0));EVE_cmd_dl_burst(DL_CLEAR|CLR_STN);}
