#include "Graphics_Test_Internal.h"

#if LV_USE_IMAGEBUTTON
/* 화면 진입 시에만 생성한다. ctx는 이 case 소유 참조를 보관하고 NULL이면
 * 실패0을 돌려 공통 ERROR 진단으로 넘긴다. 하드웨어/RTOS 접근은 없다. */
static uint32_t Build(GT_Context *ctx)
{

    GT_NEW(ctx,0,lv_imagebutton_create(ctx->body));
    lv_imagebutton_set_src(ctx->obj[0],LV_IMAGEBUTTON_STATE_RELEASED,NULL,&gt_image_a,NULL);
    lv_imagebutton_set_src(ctx->obj[0],LV_IMAGEBUTTON_STATE_PRESSED,NULL,&gt_image_b,NULL);
    GT_Place(ctx->obj[0],214,100,32,32);
    /* 원형 레이아웃에서도 raw bitmap 한 장의 크기는32px 그대로 시험한다. */
    lv_obj_set_size(ctx->obj[0],32,32);
    GT_Focus(ctx->obj[0]);
    GT_ENSURE(GT_Label(ctx->body,"32x32 source switches by state",34,208,410,GT_FONT_BODY));
    return 1U;
}

/* GRAPHICS_TEST_UPDATE_MS(기본33ms) UI service에서 합성 값만 갱신한다. elapsed_ms는 화면 진입 이후 ms이고
 * speed는0..160 DEMO 값이다. 입력/출력은 이 화면 객체에만 한정된다. */
static void Tick(GT_Context *ctx,uint32_t elapsed_ms,uint32_t speed)
{
    (void)speed;
    if(g_graphics_test.auto_advance)
        lv_obj_set_state(ctx->obj[0],LV_STATE_PRESSED,((elapsed_ms/800U)&1U)!=0U);
}

const GT_Case gt_imagebutton={{20U,"Image button","Released / pressed sources",GRAPHICS_TEST_CAP_IMAGE|GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_LIMITED,"Native image size; no tiled nine-slice."},Build,Tick};
#else
const GT_Case gt_imagebutton={{20U,"Image button","Released / pressed sources",GRAPHICS_TEST_CAP_IMAGE|GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_CONFIG_DISABLED,"LVGL widget is disabled in this build."},GT_Skip,GT_NoTick};
#endif
