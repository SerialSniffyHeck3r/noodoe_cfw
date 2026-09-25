#include "Graphics_Test_Internal.h"

#if LV_USE_BUTTONMATRIX
/* 화면 진입 시에만 생성한다. ctx는 이 case 소유 참조를 보관하고 NULL이면
 * 실패0을 돌려 공통 ERROR 진단으로 넘긴다. 하드웨어/RTOS 접근은 없다. */
static uint32_t Build(GT_Context *ctx)
{

    static const char *const map[]={"A","B","C","\n","D","E","F",""};
    GT_NEW(ctx,0,lv_buttonmatrix_create(ctx->body));
    GT_Place(ctx->obj[0],28,30,408,208);
    lv_buttonmatrix_set_map(ctx->obj[0],map);
    lv_buttonmatrix_set_button_ctrl_all(ctx->obj[0],LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_one_checked(ctx->obj[0],true);
    GT_Dense(ctx->obj[0]); GT_Focus(ctx->obj[0]);
    return 1U;
}

/* GRAPHICS_TEST_UPDATE_MS(기본33ms) UI service에서 합성 값만 갱신한다. elapsed_ms는 화면 진입 이후 ms이고
 * speed는0..160 DEMO 값이다. 입력/출력은 이 화면 객체에만 한정된다. */
static void Tick(GT_Context *ctx,uint32_t elapsed_ms,uint32_t speed)
{
    (void)speed;
    if(g_graphics_test.auto_advance) lv_buttonmatrix_set_selected_button(ctx->obj[0],(elapsed_ms/900U)%6U);
}

const GT_Case gt_buttonmatrix={{10U,"Button matrix","Six keys / selection",GRAPHICS_TEST_CAP_RECT|GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_BASIC,"Small map keeps display list bounded."},Build,Tick};
#else
const GT_Case gt_buttonmatrix={{10U,"Button matrix","Six keys / selection",GRAPHICS_TEST_CAP_RECT|GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_CONFIG_DISABLED,"LVGL widget is disabled in this build."},GT_Skip,GT_NoTick};
#endif

