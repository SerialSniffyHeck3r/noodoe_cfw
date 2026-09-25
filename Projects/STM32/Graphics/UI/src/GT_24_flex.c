#include "Graphics_Test_Internal.h"

#if LV_USE_FLEX
/* 화면 진입 시에만 생성한다. ctx는 이 case 소유 참조를 보관하고 NULL이면
 * 실패0을 돌려 공통 ERROR 진단으로 넘긴다. 하드웨어/RTOS 접근은 없다. */
static uint32_t Build(GT_Context *ctx)
{

    uint32_t i;
    GT_NEW(ctx,0,GT_Box(ctx->body,30,28,404,252));
    lv_obj_set_style_pad_all(ctx->obj[0],16,0);
    lv_obj_set_flex_flow(ctx->obj[0],LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(ctx->obj[0],LV_FLEX_ALIGN_SPACE_EVENLY,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    for(i=0;i<3U;++i) {
        GT_NEW(ctx,i+1U,GT_Box(ctx->obj[0],0,0,104,80));
        lv_obj_set_style_bg_color(ctx->obj[i+1U],lv_color_hex(0x245C6CU+i*0x100A10U),0);
        GT_ENSURE(GT_Label(ctx->obj[i+1U],i==0U?"ONE":i==1U?"TWO":"THREE",8,24,96,GT_FONT_SMALL));
    }
    return 1U;
}

/* GRAPHICS_TEST_UPDATE_MS(기본33ms) UI service에서 합성 값만 갱신한다. elapsed_ms는 화면 진입 이후 ms이고
 * speed는0..160 DEMO 값이다. 입력/출력은 이 화면 객체에만 한정된다. */
static void Tick(GT_Context *ctx,uint32_t elapsed_ms,uint32_t speed)
{
    (void)speed;
    if(g_graphics_test.auto_advance && elapsed_ms/2000U!=ctx->step) {
        ctx->step=elapsed_ms/2000U;
        lv_obj_set_width(ctx->obj[0],GT_Px((ctx->step&1U)?300:404));
    }
}

const GT_Case gt_flex={{24U,"Flex layout","Three cells / wrap",GRAPHICS_TEST_CAP_RECT|GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_BASIC,"Layout only; no layer transform."},Build,Tick};
#else
const GT_Case gt_flex={{24U,"Flex layout","Three cells / wrap",GRAPHICS_TEST_CAP_RECT|GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_CONFIG_DISABLED,"LVGL widget is disabled in this build."},GT_Skip,GT_NoTick};
#endif
