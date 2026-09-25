#include "Graphics_Test_Internal.h"

#if LV_USE_SLIDER && LV_USE_BAR
/* 화면 진입 시에만 생성한다. ctx는 이 case 소유 참조를 보관하고 NULL이면
 * 실패0을 돌려 공통 ERROR 진단으로 넘긴다. 하드웨어/RTOS 접근은 없다. */
static uint32_t Build(GT_Context *ctx)
{

    GT_NEW(ctx,0,lv_slider_create(ctx->body));
    GT_Place(ctx->obj[0],40,56,380,24);
    lv_slider_set_range(ctx->obj[0],0,160);
    GT_Focus(ctx->obj[0]);
    GT_NEW(ctx,1,lv_bar_create(ctx->body));
    GT_Place(ctx->obj[1],40,152,380,30);
    lv_bar_set_range(ctx->obj[1],0,160);
    GT_NEW(ctx,2,GT_Label(ctx->body,"Value 0",40,225,360,GT_FONT_TITLE));
    return 1U;
}

/* GRAPHICS_TEST_UPDATE_MS(기본33ms) UI service에서 합성 값만 갱신한다. elapsed_ms는 화면 진입 이후 ms이고
 * speed는0..160 DEMO 값이다. 입력/출력은 이 화면 객체에만 한정된다. */
static void Tick(GT_Context *ctx,uint32_t elapsed_ms,uint32_t speed)
{
    (void)elapsed_ms;
    if(g_graphics_test.auto_advance) lv_slider_set_value(ctx->obj[0],(int32_t)speed,LV_ANIM_OFF);
    lv_bar_set_value(ctx->obj[1],lv_slider_get_value(ctx->obj[0]),LV_ANIM_OFF);
    lv_label_set_text_fmt(ctx->obj[2],"Value %ld",(long)lv_slider_get_value(ctx->obj[0]));
}

const GT_Case gt_meters={{2U,"Value meters","Slider / bar",GRAPHICS_TEST_CAP_RECT|GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_BASIC,"0..160 sweep; manual slider editing."},Build,Tick};
#else
const GT_Case gt_meters={{2U,"Value meters","Slider / bar",GRAPHICS_TEST_CAP_RECT|GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_CONFIG_DISABLED,"LVGL widget is disabled in this build."},GT_Skip,GT_NoTick};
#endif

