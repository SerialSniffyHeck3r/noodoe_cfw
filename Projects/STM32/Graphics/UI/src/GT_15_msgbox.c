#include "Graphics_Test_Internal.h"

#if LV_USE_MSGBOX
/* 화면 진입 시에만 생성한다. ctx는 이 case 소유 참조를 보관하고 NULL이면
 * 실패0을 돌려 공통 ERROR 진단으로 넘긴다. 하드웨어/RTOS 접근은 없다. */
static uint32_t Build(GT_Context *ctx)
{

    GT_NEW(ctx,0,lv_msgbox_create(ctx->body));
    GT_Place(ctx->obj[0],36,30,392,244);
    GT_ENSURE(lv_msgbox_add_title(ctx->obj[0],"Message"));
    GT_ENSURE(lv_msgbox_add_text(ctx->obj[0],"EVE renders a small\nmessage box."));
    GT_NEW(ctx,1,lv_msgbox_add_footer_button(ctx->obj[0],"OK"));
    GT_Focus(ctx->obj[1]);
    return 1U;
}

const GT_Case gt_msgbox={{15U,"Message box","Title / text / footer button",GRAPHICS_TEST_CAP_RECT|GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_LIMITED,"Embedded box; no modal overlay or symbol image."},Build,GT_NoTick};
#else
const GT_Case gt_msgbox={{15U,"Message box","Title / text / footer button",GRAPHICS_TEST_CAP_RECT|GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_CONFIG_DISABLED,"LVGL widget is disabled in this build."},GT_Skip,GT_NoTick};
#endif

