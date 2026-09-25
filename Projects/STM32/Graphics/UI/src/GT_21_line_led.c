#include "Graphics_Test_Internal.h"

#if LV_USE_LINE && LV_USE_LED
/* 화면 진입 시에만 생성한다. ctx는 이 case 소유 참조를 보관하고 NULL이면
 * 실패0을 돌려 공통 ERROR 진단으로 넘긴다. 하드웨어/RTOS 접근은 없다. */
static uint32_t Build(GT_Context *ctx)
{

    static const lv_point_precise_t pts[]={{0,75},{45,15},{105,54},{180,0},{270,67}};
    GT_NEW(ctx,0,lv_line_create(ctx->body));
    lv_line_set_points(ctx->obj[0],pts,5U);
    GT_Position(ctx->obj[0],48,36);
    lv_obj_set_style_line_color(ctx->obj[0],lv_color_hex(GT_ACCENT),0);
    lv_obj_set_style_line_width(ctx->obj[0],4,0);
    GT_NEW(ctx,1,lv_led_create(ctx->body));
    GT_Place(ctx->obj[1],210,202,40,40);
    lv_led_set_color(ctx->obj[1],lv_color_hex(GT_ACCENT));
    return 1U;
}

/* GRAPHICS_TEST_UPDATE_MS(기본33ms) UI service에서 합성 값만 갱신한다. elapsed_ms는 화면 진입 이후 ms이고
 * speed는0..160 DEMO 값이다. 입력/출력은 이 화면 객체에만 한정된다. */
static void Tick(GT_Context *ctx,uint32_t elapsed_ms,uint32_t speed)
{
    (void)elapsed_ms;
    lv_led_set_brightness(ctx->obj[1],(uint8_t)(speed*255U/160U));
}

const GT_Case gt_line_led={{21U,"Line and LED","Polyline / indicator brightness",GRAPHICS_TEST_CAP_LINE|GRAPHICS_TEST_CAP_RECT,GRAPHICS_TEST_SUPPORT_LIMITED,"LED shadow/gradient deliberately disabled."},Build,Tick};
#else
const GT_Case gt_line_led={{21U,"Line and LED","Polyline / indicator brightness",GRAPHICS_TEST_CAP_LINE|GRAPHICS_TEST_CAP_RECT,GRAPHICS_TEST_SUPPORT_CONFIG_DISABLED,"LVGL widget is disabled in this build."},GT_Skip,GT_NoTick};
#endif
