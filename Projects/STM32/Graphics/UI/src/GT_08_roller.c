#include "Graphics_Test_Internal.h"

#if LV_USE_ROLLER
/* 화면 진입 시에만 생성한다. ctx는 이 case 소유 참조를 보관하고 NULL이면
 * 실패0을 돌려 공통 ERROR 진단으로 넘긴다. 하드웨어/RTOS 접근은 없다. */
static uint32_t Build(GT_Context *ctx)
{

    GT_NEW(ctx,0,lv_roller_create(ctx->body));
    lv_roller_set_options(ctx->obj[0],"ONE\nTWO\nTHREE",LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(ctx->obj[0],3);
    lv_obj_set_width(ctx->obj[0],GT_Px(260));
    GT_Position(ctx->obj[0],100,32);
    lv_obj_set_style_text_font(ctx->obj[0],GT_FONT_BODY,0);
    lv_obj_set_style_bg_grad_dir(ctx->obj[0],LV_GRAD_DIR_NONE,0);
    GT_Focus(ctx->obj[0]);
    return 1U;
}

/* GRAPHICS_TEST_UPDATE_MS(기본33ms) UI service에서 합성 값만 갱신한다. elapsed_ms는 화면 진입 이후 ms이고
 * speed는0..160 DEMO 값이다. 입력/출력은 이 화면 객체에만 한정된다. */
static void Tick(GT_Context *ctx,uint32_t elapsed_ms,uint32_t speed)
{
    (void)speed;
    if(g_graphics_test.auto_advance) lv_roller_set_selected(ctx->obj[0],(elapsed_ms/1600U)%3U,LV_ANIM_OFF);
}

const GT_Case gt_roller={{8U,"Roller","Finite options / selection",GRAPHICS_TEST_CAP_TEXT|GRAPHICS_TEST_CAP_CLIP,GRAPHICS_TEST_SUPPORT_LIMITED,"Finite list, flat background; no gradient fade."},Build,Tick};
#else
const GT_Case gt_roller={{8U,"Roller","Finite options / selection",GRAPHICS_TEST_CAP_TEXT|GRAPHICS_TEST_CAP_CLIP,GRAPHICS_TEST_SUPPORT_CONFIG_DISABLED,"LVGL widget is disabled in this build."},GT_Skip,GT_NoTick};
#endif
