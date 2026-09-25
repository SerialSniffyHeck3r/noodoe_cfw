#include "Graphics_Test_Internal.h"

#if LV_USE_CHART
/* 화면 진입 시에만 생성한다. ctx는 이 case 소유 참조를 보관하고 NULL이면
 * 실패0을 돌려 공통 ERROR 진단으로 넘긴다. 하드웨어/RTOS 접근은 없다. */
static uint32_t Build(GT_Context *ctx)
{

    GT_NEW(ctx,0,lv_chart_create(ctx->body));
    GT_Place(ctx->obj[0],36,22,390,228);
    lv_chart_set_type(ctx->obj[0],LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(ctx->obj[0],16);
    lv_chart_set_axis_range(ctx->obj[0],LV_CHART_AXIS_PRIMARY_Y,0,160);
    lv_chart_set_div_line_count(ctx->obj[0],3,3);
    lv_obj_set_style_size(ctx->obj[0],0,0,LV_PART_INDICATOR);
    ctx->extra=lv_chart_add_series(ctx->obj[0],lv_color_hex(GT_ACCENT),LV_CHART_AXIS_PRIMARY_Y);
    GT_ENSURE(ctx->extra);
    lv_chart_set_all_values(ctx->obj[0],ctx->extra,0);
    GT_ENSURE(GT_Label(ctx->body,"Samples: synthetic speed",36,270,390,GT_FONT_SMALL));
    return 1U;
}

/* GRAPHICS_TEST_UPDATE_MS(기본33ms) UI service에서 합성 값만 갱신한다. elapsed_ms는 화면 진입 이후 ms이고
 * speed는0..160 DEMO 값이다. 입력/출력은 이 화면 객체에만 한정된다. */
static void Tick(GT_Context *ctx,uint32_t elapsed_ms,uint32_t speed)
{
    uint32_t step=elapsed_ms/300U;
    if(step==ctx->step) return;
    ctx->step=step;
    lv_chart_set_next_value(ctx->obj[0],ctx->extra,(int32_t)speed);
}

const GT_Case gt_chart={{3U,"Chart","16-point line series",GRAPHICS_TEST_CAP_LINE|GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_LIMITED,"Small line chart; no area fill or custom masks."},Build,Tick};
#else
const GT_Case gt_chart={{3U,"Chart","16-point line series",GRAPHICS_TEST_CAP_LINE|GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_CONFIG_DISABLED,"LVGL widget is disabled in this build."},GT_Skip,GT_NoTick};
#endif

