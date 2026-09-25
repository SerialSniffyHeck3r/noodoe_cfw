#include "Graphics_Test_Internal.h"

#if LV_USE_SPINNER
/* 화면 진입 시에만 생성한다. ctx는 이 case 소유 참조를 보관하고 NULL이면
 * 실패0을 돌려 공통 ERROR 진단으로 넘긴다. 하드웨어/RTOS 접근은 없다. */
static uint32_t Build(GT_Context *ctx)
{

    GT_NEW(ctx,0,lv_spinner_create(ctx->body));
    GT_Place(ctx->obj[0],158,38,148,148);
    lv_spinner_set_anim_params(ctx->obj[0],1400U,70U);
    lv_obj_set_style_arc_width(ctx->obj[0],10,LV_PART_MAIN);
    lv_obj_set_style_arc_width(ctx->obj[0],10,LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ctx->obj[0],lv_color_hex(GT_ACCENT),LV_PART_INDICATOR);
    GT_ENSURE(GT_Label(ctx->body,"Continuous arc animation",68,230,360,GT_FONT_BODY));
    return 1U;
}

const GT_Case gt_spinner={{16U,"Spinner","Animated arc",GRAPHICS_TEST_CAP_ARC|GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_BASIC,"Object-owned animation is deleted on case exit."},Build,GT_NoTick};
#else
const GT_Case gt_spinner={{16U,"Spinner","Animated arc",GRAPHICS_TEST_CAP_ARC|GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_CONFIG_DISABLED,"LVGL widget is disabled in this build."},GT_Skip,GT_NoTick};
#endif

