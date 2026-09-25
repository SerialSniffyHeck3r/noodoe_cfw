#include "Graphics_Test_Internal.h"

#if LV_USE_SCALE && LV_USE_BAR
/* 화면 진입 시에만 생성한다. ctx는 이 case 소유 참조를 보관하고 NULL이면
 * 실패0을 돌려 공통 ERROR 진단으로 넘긴다. 하드웨어/RTOS 접근은 없다. */
static uint32_t Build(GT_Context *ctx)
{

    GT_NEW(ctx,0,lv_scale_create(ctx->body));
    GT_Place(ctx->obj[0],42,72,378,86);
    lv_scale_set_mode(ctx->obj[0],LV_SCALE_MODE_HORIZONTAL_BOTTOM);
    lv_scale_set_range(ctx->obj[0],0,160);
    lv_scale_set_total_tick_count(ctx->obj[0],9);
    lv_scale_set_major_tick_every(ctx->obj[0],2);
    lv_scale_set_label_show(ctx->obj[0],true);
    lv_obj_set_style_text_font(ctx->obj[0],GT_FONT_SMALL,LV_PART_MAIN);
    GT_NEW(ctx,1,lv_bar_create(ctx->body));
    GT_Place(ctx->obj[1],42,44,378,16);
    lv_bar_set_range(ctx->obj[1],0,160);
    return 1U;
}

/* GRAPHICS_TEST_UPDATE_MS(기본33ms) UI service에서 합성 값만 갱신한다. elapsed_ms는 화면 진입 이후 ms이고
 * speed는0..160 DEMO 값이다. 입력/출력은 이 화면 객체에만 한정된다. */
static void Tick(GT_Context *ctx,uint32_t elapsed_ms,uint32_t speed)
{
    (void)elapsed_ms;
    lv_bar_set_value(ctx->obj[1],(int32_t)speed,LV_ANIM_OFF);
}

const GT_Case gt_scale={{5U,"Scale","Linear ticks / labels",GRAPHICS_TEST_CAP_LINE|GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_LIMITED,"Linear scale only; no rotated text or round mask."},Build,Tick};
#else
const GT_Case gt_scale={{5U,"Scale","Linear ticks / labels",GRAPHICS_TEST_CAP_LINE|GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_CONFIG_DISABLED,"LVGL widget is disabled in this build."},GT_Skip,GT_NoTick};
#endif

