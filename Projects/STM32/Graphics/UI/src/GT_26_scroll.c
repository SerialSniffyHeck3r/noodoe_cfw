#include "Graphics_Test_Internal.h"

#if LV_USE_LABEL
/* 화면 진입 시에만 생성한다. ctx는 이 case 소유 참조를 보관하고 NULL이면
 * 실패0을 돌려 공통 ERROR 진단으로 넘긴다. 하드웨어/RTOS 접근은 없다. */
static uint32_t Build(GT_Context *ctx)
{

    uint32_t i;
    GT_NEW(ctx,0,GT_Box(ctx->body,54,26,356,254));
    lv_obj_add_flag(ctx->obj[0],LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(ctx->obj[0],LV_DIR_VER);
    for(i=0;i<5U;++i) {
        lv_obj_t *label=GT_Label(ctx->obj[0],"Scrollable row",24,(int32_t)i*82,280,GT_FONT_BODY);
        GT_ENSURE(label);
        lv_label_set_text_fmt(label,"ROW %lu",(unsigned long)i+1UL);
    }
    return 1U;
}

/* GRAPHICS_TEST_UPDATE_MS(기본33ms) UI service에서 합성 값만 갱신한다. elapsed_ms는 화면 진입 이후 ms이고
 * speed는0..160 DEMO 값이다. 입력/출력은 이 화면 객체에만 한정된다. */
static void Tick(GT_Context *ctx,uint32_t elapsed_ms,uint32_t speed)
{
    (void)speed;
    if(g_graphics_test.auto_advance)
        lv_obj_scroll_to_y(ctx->obj[0],GT_Px((int32_t)((elapsed_ms/20U)%200U)),LV_ANIM_OFF);
}

const GT_Case gt_scroll={{26U,"Scrolling","Rectangular viewport",GRAPHICS_TEST_CAP_RECT|GRAPHICS_TEST_CAP_TEXT|GRAPHICS_TEST_CAP_CLIP,GRAPHICS_TEST_SUPPORT_LIMITED,"Rectangular scissor only; corner masking skipped."},Build,Tick};
#else
const GT_Case gt_scroll={{26U,"Scrolling","Rectangular viewport",GRAPHICS_TEST_CAP_RECT|GRAPHICS_TEST_CAP_TEXT|GRAPHICS_TEST_CAP_CLIP,GRAPHICS_TEST_SUPPORT_CONFIG_DISABLED,"LVGL widget is disabled in this build."},GT_Skip,GT_NoTick};
#endif
