#include "Graphics_Test_Internal.h"

#if LV_USE_ANIMIMG
/* 화면 진입 시에만 생성한다. ctx는 이 case 소유 참조를 보관하고 NULL이면
 * 실패0을 돌려 공통 ERROR 진단으로 넘긴다. 하드웨어/RTOS 접근은 없다. */
static uint32_t Build(GT_Context *ctx)
{

    static const void *frames[]={&gt_image_a,&gt_image_b};
    GT_NEW(ctx,0,lv_animimg_create(ctx->body));
    lv_animimg_set_src(ctx->obj[0],frames,2U);
    lv_animimg_set_duration(ctx->obj[0],800U);
    lv_animimg_set_repeat_count(ctx->obj[0],LV_ANIM_REPEAT_INFINITE);
    lv_image_set_scale(ctx->obj[0],1024U);
    GT_Position(ctx->obj[0],206,98);
    lv_animimg_start(ctx->obj[0]);
    GT_ENSURE(GT_Label(ctx->body,"Two frames / 800ms loop",58,244,380,GT_FONT_BODY));
    return 1U;
}

const GT_Case gt_animimage={{19U,"Animated image","Two immutable RGB565 frames",GRAPHICS_TEST_CAP_IMAGE|GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_LIMITED,"Same two cache keys reused on every visit."},Build,GT_NoTick};
#else
const GT_Case gt_animimage={{19U,"Animated image","Two immutable RGB565 frames",GRAPHICS_TEST_CAP_IMAGE|GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_CONFIG_DISABLED,"LVGL widget is disabled in this build."},GT_Skip,GT_NoTick};
#endif

