#include "Graphics_Viewport.h"
#include "src/libs/FT800-FT813/EVE_commands.h"

/* LVGL arc/border도stencil을 사용하므로 모든UI 뒤 독립된 stencil을 만든다.
 * mask bitmap/framebuffer 없이30word로 고정 원 바깥을 설정된 단색으로 제외한다.
 * EVE display-list의 외곽pixel 주사 자체를 멈추는 기능은 아니다.
 * raw명령을 context로 감싸graphics속성을 복원한다. BEGIN/END primitive는
 * context 대상이 아니므로 다음draw의 동기화는 EveDispatch가 맡는다. */
void Graphics_EveApplyViewport(void)
{
    EVE_cmd_dl_burst(DL_SAVE_CONTEXT);
    EVE_cmd_dl_burst(SCISSOR_XY(0U,0U));
    EVE_cmd_dl_burst(SCISSOR_SIZE(GRAPHICS_WIDTH,GRAPHICS_HEIGHT));
    EVE_cmd_dl_burst(VERTEX_FORMAT(4U));
    EVE_cmd_dl_burst(VERTEX_TRANSLATE_X(0));
    EVE_cmd_dl_burst(VERTEX_TRANSLATE_Y(0));
    EVE_cmd_dl_burst(ALPHA_FUNC(EVE_ALWAYS,0U));
    EVE_cmd_dl_burst(COLOR_A(255U));
    EVE_cmd_dl_burst(TAG_MASK(0U));
    EVE_cmd_dl_burst(STENCIL_MASK(255U));
    EVE_cmd_dl_burst(CLEAR_STENCIL(0U));
    EVE_cmd_dl_burst(DL_CLEAR|CLR_STN);
    /* RGB/alpha를 쓰지 않고 중심239.5·반경240의stencil만1로 설정한다.
     * 2배좌표에서1/16pixel로 변환하는 계수는8이며 POINT_SIZE는반경이다. */
    EVE_cmd_dl_burst(COLOR_MASK(0U,0U,0U,0U));
    EVE_cmd_dl_burst(STENCIL_FUNC(EVE_ALWAYS,1U,255U));
    EVE_cmd_dl_burst(STENCIL_OP(EVE_REPLACE,EVE_REPLACE));
    EVE_cmd_dl_burst(POINT_SIZE(GRAPHICS_ACTIVE_RADIUS_X2*8U));
    EVE_cmd_dl_burst(DL_BEGIN|EVE_POINTS);
    EVE_cmd_dl_burst(VERTEX2F(GRAPHICS_ACTIVE_CENTER_X2*8U,GRAPHICS_ACTIVE_CENTER_Y2*8U));
    EVE_cmd_dl_burst(DL_END);
    /* stencil!=1의 바깥만 개발용 녹회색/제품용 검정. 이전widget 색·alpha·blend와 무관하게
     * 수행한 뒤scissor와정점정밀도 등context 상태를복원한다. */
    EVE_cmd_dl_burst(COLOR_MASK(1U,1U,1U,1U));
    EVE_cmd_dl_burst(COLOR_RGB((GRAPHICS_OUTSIDE_RGB>>16)&255U,(GRAPHICS_OUTSIDE_RGB>>8)&255U,GRAPHICS_OUTSIDE_RGB&255U));
    EVE_cmd_dl_burst(STENCIL_FUNC(EVE_NOTEQUAL,1U,255U));
    EVE_cmd_dl_burst(STENCIL_OP(EVE_KEEP,EVE_KEEP));
    EVE_cmd_dl_burst(BLEND_FUNC(EVE_ONE,EVE_ZERO));
    EVE_cmd_dl_burst(LINE_WIDTH(16U));
    EVE_cmd_dl_burst(DL_BEGIN|EVE_RECTS);
    EVE_cmd_dl_burst(VERTEX2F(0U,0U));
    EVE_cmd_dl_burst(VERTEX2F(GRAPHICS_WIDTH*16U,GRAPHICS_HEIGHT*16U));
    EVE_cmd_dl_burst(DL_END);
    EVE_cmd_dl_burst(DL_RESTORE_CONTEXT);
}
