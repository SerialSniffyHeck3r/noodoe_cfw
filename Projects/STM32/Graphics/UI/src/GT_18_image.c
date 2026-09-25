#include "Graphics_Test_Internal.h"

#if LV_USE_IMAGE
/* 화면 진입 시에만 생성한다. ctx는 이 case 소유 참조를 보관하고 NULL이면
 * 실패0을 돌려 공통 ERROR 진단으로 넘긴다. 하드웨어/RTOS 접근은 없다. */
static uint32_t Build(GT_Context *ctx)
{

    GT_NEW(ctx,0,lv_image_create(ctx->body));
    lv_image_set_src(ctx->obj[0],&gt_image_a);
    lv_image_set_scale(ctx->obj[0],768U);
    GT_Position(ctx->obj[0],84,80);
    GT_NEW(ctx,1,lv_image_create(ctx->body));
    lv_image_set_src(ctx->obj[1],&gt_image_alpha);
    lv_image_set_scale(ctx->obj[1],768U);
    GT_Position(ctx->obj[1],304,80);
    GT_ENSURE(GT_Label(ctx->body,"RGB565            ARGB8888",40,214,420,GT_FONT_BODY));
    GT_ENSURE(GT_Label(ctx->body,"Fixed 32x32 assets / RAM_G reused",36,256,420,GT_FONT_SMALL));
    return 1U;
}

/* GRAPHICS_TEST_UPDATE_MS(기본33ms) UI service에서 합성 값만 갱신한다. elapsed_ms는 화면 진입 이후 ms이고
 * speed는0..160 DEMO 값이다. 입력/출력은 이 화면 객체에만 한정된다. */
static void Tick(GT_Context *ctx,uint32_t elapsed_ms,uint32_t speed)
{
    (void)speed;
    lv_image_set_rotation(ctx->obj[0],(int32_t)((elapsed_ms/10U)%360U)*10);
}

const GT_Case gt_image={{18U,"Images","RGB565 / alpha / transform",GRAPHICS_TEST_CAP_IMAGE|GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_LIMITED,"Static variable images only; nearest scaling."},Build,Tick};
#else
const GT_Case gt_image={{18U,"Images","RGB565 / alpha / transform",GRAPHICS_TEST_CAP_IMAGE|GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_CONFIG_DISABLED,"LVGL widget is disabled in this build."},GT_Skip,GT_NoTick};
#endif

