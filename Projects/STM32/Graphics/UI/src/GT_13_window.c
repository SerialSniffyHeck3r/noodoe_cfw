#include "Graphics_Test_Internal.h"

#if LV_USE_WIN
/* 화면 진입 시에만 생성한다. ctx는 이 case 소유 참조를 보관하고 NULL이면
 * 실패0을 돌려 공통 ERROR 진단으로 넘긴다. 하드웨어/RTOS 접근은 없다. */
static uint32_t Build(GT_Context *ctx)
{

    lv_obj_t *content;
    GT_NEW(ctx,0,lv_win_create(ctx->body));
    GT_Place(ctx->obj[0],20,18,424,272);
    lv_obj_set_height(lv_win_get_header(ctx->obj[0]),GT_Px(48));
    GT_ENSURE(lv_win_add_title(ctx->obj[0],"Window"));
    content=lv_win_get_content(ctx->obj[0]); GT_ENSURE(content);
    GT_ENSURE(GT_Label(content,"Independent content\narea inside window",12,28,350,GT_FONT_BODY));
    return 1U;
}

const GT_Case gt_window={{13U,"Window","Header / content",GRAPHICS_TEST_CAP_RECT|GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_BASIC,"Embedded window; no whole-window opacity."},Build,GT_NoTick};
#else
const GT_Case gt_window={{13U,"Window","Header / content",GRAPHICS_TEST_CAP_RECT|GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_CONFIG_DISABLED,"LVGL widget is disabled in this build."},GT_Skip,GT_NoTick};
#endif
