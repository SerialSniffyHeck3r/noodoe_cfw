#include "Graphics_Test_Internal.h"

#if LV_USE_BUTTON
/* 화면 진입 시에만 생성한다. ctx는 이 case 소유 참조를 보관하고 NULL이면
 * 실패0을 돌려 공통 ERROR 진단으로 넘긴다. 하드웨어/RTOS 접근은 없다. */
static uint32_t Build(GT_Context *ctx)
{

    uint32_t i;
    for(i=0;i<2U;++i) {
        GT_NEW(ctx,i,lv_button_create(ctx->body));
        GT_Place(ctx->obj[i],48+(int32_t)i*192,62,164,64);
        GT_ENSURE(GT_Label(ctx->obj[i],i?"BUTTON B":"BUTTON A",12,18,144,GT_FONT_SMALL));
        GT_Focus(ctx->obj[i]);
    }
    GT_NEW(ctx,2,GT_Label(ctx->body,"Events: 0",48,196,370,GT_FONT_BODY));
    return 1U;
}

/* GRAPHICS_TEST_UPDATE_MS(기본33ms) UI service에서 합성 값만 갱신한다. elapsed_ms는 화면 진입 이후 ms이고
 * speed는0..160 DEMO 값이다. 입력/출력은 이 화면 객체에만 한정된다. */
static void Tick(GT_Context *ctx,uint32_t elapsed_ms,uint32_t speed)
{
    uint32_t step=elapsed_ms/1200U;
    (void)speed;
    if(g_graphics_test.auto_advance && step!=ctx->step) {
        ctx->step=step;
        lv_group_focus_obj(ctx->obj[step%2U]);
        lv_obj_send_event(ctx->obj[step%2U],LV_EVENT_CLICKED,NULL);
    }
    lv_label_set_text_fmt(ctx->obj[2],"Events: %lu",(unsigned long)g_graphics_test.ui_events);
}

const GT_Case gt_events={{27U,"Events and focus","Click / state / focus border",GRAPHICS_TEST_CAP_RECT|GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_BASIC,"Auto click events are synthetic, not physical proof."},Build,Tick};
#else
const GT_Case gt_events={{27U,"Events and focus","Click / state / focus border",GRAPHICS_TEST_CAP_RECT|GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_CONFIG_DISABLED,"LVGL widget is disabled in this build."},GT_Skip,GT_NoTick};
#endif

