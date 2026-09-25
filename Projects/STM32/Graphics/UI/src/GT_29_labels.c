#include "Graphics_Test_Internal.h"

#if LV_USE_LABEL
/* 화면 진입 시에만 생성한다. ctx는 이 case 소유 참조를 보관하고 NULL이면
 * 실패0을 돌려 공통 ERROR 진단으로 넘긴다. 하드웨어/RTOS 접근은 없다. */
static uint32_t Build(GT_Context *ctx)
{

    GT_ENSURE(GT_Label(ctx->body,"14 px - compact telemetry",24,20,410,GT_FONT_SMALL));
    GT_ENSURE(GT_Label(ctx->body,"20 px - readable text",24,56,410,GT_FONT_BODY));
    GT_ENSURE(GT_Label(ctx->body,"28 px - title",24,99,410,GT_FONT_TITLE));
    GT_ENSURE(GT_Label(ctx->body,"123 456",24,149,410,GT_FONT_SPEED));
    GT_NEW(ctx,0,GT_Label(ctx->body,"Wrapped text remains inside the rectangular label width.",24,224,410,GT_FONT_SMALL));
    return 1U;
}

const GT_Case gt_labels={{29U,"Labels","Fonts / wrap / alignment",GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_BASIC,"14/20/28/40 4bpp fonts; ASCII only."},Build,GT_NoTick};
#else
const GT_Case gt_labels={{29U,"Labels","Fonts / wrap / alignment",GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_CONFIG_DISABLED,"LVGL widget is disabled in this build."},GT_Skip,GT_NoTick};
#endif

