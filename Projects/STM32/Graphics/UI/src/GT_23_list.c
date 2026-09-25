#include "Graphics_Test_Internal.h"

#if LV_USE_LIST
/* 화면 진입 시에만 생성한다. ctx는 이 case 소유 참조를 보관하고 NULL이면
 * 실패0을 돌려 공통 ERROR 진단으로 넘긴다. 하드웨어/RTOS 접근은 없다. */
static uint32_t Build(GT_Context *ctx)
{

    GT_NEW(ctx,0,lv_list_create(ctx->body));
    GT_Place(ctx->obj[0],40,22,384,266);
    GT_NEW(ctx,1,lv_list_add_button(ctx->obj[0],NULL,"Status"));
    GT_NEW(ctx,2,lv_list_add_button(ctx->obj[0],NULL,"Trip"));
    GT_NEW(ctx,3,lv_list_add_button(ctx->obj[0],NULL,"Settings"));
    GT_Focus(ctx->obj[1]); GT_Focus(ctx->obj[2]); GT_Focus(ctx->obj[3]);
    return 1U;
}

/* GRAPHICS_TEST_UPDATE_MS(기본33ms) UI service에서 합성 값만 갱신한다. elapsed_ms는 화면 진입 이후 ms이고
 * speed는0..160 DEMO 값이다. 입력/출력은 이 화면 객체에만 한정된다. */
static void Tick(GT_Context *ctx,uint32_t elapsed_ms,uint32_t speed)
{
    (void)speed;
    if(g_graphics_test.auto_advance && elapsed_ms/1500U!=ctx->step) {
        ctx->step=elapsed_ms/1500U;
        lv_group_focus_obj(ctx->obj[1U+ctx->step%3U]);
    }
}

const GT_Case gt_list={{23U,"List","Three rows / focus",GRAPHICS_TEST_CAP_RECT|GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_BASIC,"ASCII text rows; image icons excluded."},Build,Tick};
#else
const GT_Case gt_list={{23U,"List","Three rows / focus",GRAPHICS_TEST_CAP_RECT|GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_CONFIG_DISABLED,"LVGL widget is disabled in this build."},GT_Skip,GT_NoTick};
#endif

