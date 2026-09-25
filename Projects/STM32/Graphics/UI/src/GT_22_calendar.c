#include "Graphics_Test_Internal.h"

#if LV_USE_CALENDAR
/* 화면 진입 시에만 생성한다. ctx는 이 case 소유 참조를 보관하고 NULL이면
 * 실패0을 돌려 공통 ERROR 진단으로 넘긴다. 하드웨어/RTOS 접근은 없다. */
static uint32_t Build(GT_Context *ctx)
{

    static lv_calendar_date_t dates[]={{2026,9,12}};
    static const char *days[]={"S","M","T","W","T","F","S"};
    GT_NEW(ctx,0,lv_calendar_create(ctx->body));
    GT_Place(ctx->obj[0],36,22,392,260);
    lv_calendar_set_today_date(ctx->obj[0],2026,9,12);
    lv_calendar_set_month_shown(ctx->obj[0],2026,9);
    lv_calendar_set_highlighted_dates(ctx->obj[0],dates,1U);
    lv_calendar_set_day_names(ctx->obj[0],days);
    GT_Dense(ctx->obj[0]);
    GT_Dense(lv_calendar_get_btnmatrix(ctx->obj[0]));
    GT_Focus(lv_calendar_get_btnmatrix(ctx->obj[0]));
    return 1U;
}

const GT_Case gt_calendar={{22U,"Calendar","Month grid / highlight",GRAPHICS_TEST_CAP_RECT|GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_LIMITED,"Small month only; no dropdown/arrow images."},Build,GT_NoTick};
#else
const GT_Case gt_calendar={{22U,"Calendar","Month grid / highlight",GRAPHICS_TEST_CAP_RECT|GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_CONFIG_DISABLED,"LVGL widget is disabled in this build."},GT_Skip,GT_NoTick};
#endif

