#include "Graphics_Test_Internal.h"

#if 1
/* 위치 값만 바꾸는 animation callback이다. style opacity/transform layer는 생성하지 않는다. */
static void MoveX(void *var,int32_t value) { lv_obj_set_x((lv_obj_t*)var,GT_Px(value)); }

/* 화면 진입 시에만 생성한다. ctx는 이 case 소유 참조를 보관하고 NULL이면
 * 실패0을 돌려 공통 ERROR 진단으로 넘긴다. 하드웨어/RTOS 접근은 없다. */
static uint32_t Build(GT_Context *ctx)
{

    lv_anim_t anim;
    GT_NEW(ctx,0,GT_Box(ctx->body,24,106,58,58));
    lv_obj_set_style_bg_color(ctx->obj[0],lv_color_hex(GT_ACCENT),0);
    lv_anim_init(&anim);
    lv_anim_set_var(&anim,ctx->obj[0]);
    lv_anim_set_exec_cb(&anim,MoveX);
    lv_anim_set_values(&anim,24,374);
    lv_anim_set_duration(&anim,1600U);
    lv_anim_set_reverse_duration(&anim,1600U);
    lv_anim_set_repeat_count(&anim,LV_ANIM_REPEAT_INFINITE);
    GT_ENSURE(lv_anim_start(&anim));
    GT_ENSURE(GT_Label(ctx->body,"Object animation is freed on exit",28,238,410,GT_FONT_SMALL));
    return 1U;
}

const GT_Case gt_animation={{28U,"Animation","Position / reverse / repeat",GRAPHICS_TEST_CAP_RECT|GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_LIMITED,"Position animation only; whole-widget opacity/zoom skipped."},Build,GT_NoTick};
#else
const GT_Case gt_animation={{28U,"Animation","Position / reverse / repeat",GRAPHICS_TEST_CAP_RECT|GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_CONFIG_DISABLED,"LVGL widget is disabled in this build."},GT_Skip,GT_NoTick};
#endif
