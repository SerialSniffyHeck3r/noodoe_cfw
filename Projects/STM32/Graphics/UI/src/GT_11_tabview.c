#include "Graphics_Test_Internal.h"

#if LV_USE_TABVIEW
/* 화면 진입 시에만 생성한다. ctx는 이 case 소유 참조를 보관하고 NULL이면
 * 실패0을 돌려 공통 ERROR 진단으로 넘긴다. 하드웨어/RTOS 접근은 없다. */
static uint32_t Build(GT_Context *ctx)
{

    GT_NEW(ctx,0,lv_tabview_create(ctx->body));
    GT_Place(ctx->obj[0],16,14,432,276);
    lv_tabview_set_tab_bar_size(ctx->obj[0],42);
    GT_NEW(ctx,1,lv_tabview_add_tab(ctx->obj[0],"STATUS"));
    GT_NEW(ctx,2,lv_tabview_add_tab(ctx->obj[0],"TRIP"));
    GT_ENSURE(GT_Label(ctx->obj[1],"Status page",18,30,340,GT_FONT_TITLE));
    GT_ENSURE(GT_Label(ctx->obj[2],"Trip page / DEMO",18,30,340,GT_FONT_TITLE));
    GT_Focus(lv_tabview_get_tab_button(ctx->obj[0],0));
    GT_Focus(lv_tabview_get_tab_button(ctx->obj[0],1));
    return 1U;
}

/* GRAPHICS_TEST_UPDATE_MS(기본33ms) UI service에서 합성 값만 갱신한다. elapsed_ms는 화면 진입 이후 ms이고
 * speed는0..160 DEMO 값이다. 입력/출력은 이 화면 객체에만 한정된다. */
static void Tick(GT_Context *ctx,uint32_t elapsed_ms,uint32_t speed)
{
    (void)speed;
    if(g_graphics_test.auto_advance) lv_tabview_set_active(ctx->obj[0],(elapsed_ms/2000U)%2U,LV_ANIM_OFF);
}

const GT_Case gt_tabview={{11U,"Tab view","Two tabs / page switch",GRAPHICS_TEST_CAP_RECT|GRAPHICS_TEST_CAP_TEXT|GRAPHICS_TEST_CAP_CLIP,GRAPHICS_TEST_SUPPORT_LIMITED,"Two small pages; animated slide disabled."},Build,Tick};
#else
const GT_Case gt_tabview={{11U,"Tab view","Two tabs / page switch",GRAPHICS_TEST_CAP_RECT|GRAPHICS_TEST_CAP_TEXT|GRAPHICS_TEST_CAP_CLIP,GRAPHICS_TEST_SUPPORT_CONFIG_DISABLED,"LVGL widget is disabled in this build."},GT_Skip,GT_NoTick};
#endif

