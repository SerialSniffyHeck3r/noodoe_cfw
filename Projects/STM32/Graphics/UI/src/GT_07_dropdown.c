#include "Graphics_Test_Internal.h"

#if LV_USE_DROPDOWN
/* 화면 진입 시에만 생성한다. ctx는 이 case 소유 참조를 보관하고 NULL이면
 * 실패0을 돌려 공통 ERROR 진단으로 넘긴다. 하드웨어/RTOS 접근은 없다. */
static uint32_t Build(GT_Context *ctx)
{

    GT_NEW(ctx,0,lv_dropdown_create(ctx->body));
    GT_Place(ctx->obj[0],72,48,320,52);
    lv_dropdown_set_options(ctx->obj[0],"Urban\nTouring\nSport");
    lv_dropdown_set_symbol(ctx->obj[0],NULL);
    /* list는 body 자식이 아니라 screen 아래에 미리 생성되므로 공통 body 순회에
     * 포함되지 않는다. 수동으로 먼저 열어도 대비가 맞도록 생성 직후 별도 적용한다. */
    GT_ENSURE(lv_dropdown_get_list(ctx->obj[0]));
    GT_FlatTree(lv_dropdown_get_list(ctx->obj[0]));
    GT_Focus(ctx->obj[0]);
    GT_ENSURE(GT_Label(ctx->body,"Popup opens after 2 seconds",30,238,400,GT_FONT_SMALL));
    return 1U;
}

/* GRAPHICS_TEST_UPDATE_MS(기본33ms) UI service에서 합성 값만 갱신한다. elapsed_ms는 화면 진입 이후 ms이고
 * speed는0..160 DEMO 값이다. 입력/출력은 이 화면 객체에만 한정된다. */
static void Tick(GT_Context *ctx,uint32_t elapsed_ms,uint32_t speed)
{
    (void)speed;
    if(!g_graphics_test.auto_advance) return;
    if(elapsed_ms>=2000U && ctx->step==0U) {
        lv_dropdown_open(ctx->obj[0]);
        ctx->step=1U;
    }
    if(elapsed_ms>=5000U && ctx->step==1U) {
        lv_dropdown_set_selected(ctx->obj[0],2U);
        lv_dropdown_close(ctx->obj[0]); ctx->step=2U;
    }
}

const GT_Case gt_dropdown={{7U,"Dropdown","Selection / popup",GRAPHICS_TEST_CAP_RECT|GRAPHICS_TEST_CAP_TEXT|GRAPHICS_TEST_CAP_CLIP,GRAPHICS_TEST_SUPPORT_LIMITED,"Three fixed options; no symbol image or fade."},Build,Tick};
#else
const GT_Case gt_dropdown={{7U,"Dropdown","Selection / popup",GRAPHICS_TEST_CAP_RECT|GRAPHICS_TEST_CAP_TEXT|GRAPHICS_TEST_CAP_CLIP,GRAPHICS_TEST_SUPPORT_CONFIG_DISABLED,"LVGL widget is disabled in this build."},GT_Skip,GT_NoTick};
#endif
