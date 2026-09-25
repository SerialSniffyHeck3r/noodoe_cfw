#include "Graphics_Test_Internal.h"

#if LV_USE_BUTTON && LV_USE_CHECKBOX && LV_USE_SWITCH
/* 화면 진입 시에만 생성한다. ctx는 이 case 소유 참조를 보관하고 NULL이면
 * 실패0을 돌려 공통 ERROR 진단으로 넘긴다. 하드웨어/RTOS 접근은 없다. */
static uint32_t Build(GT_Context *ctx)
{

    GT_NEW(ctx,0,lv_button_create(ctx->body));
    GT_Place(ctx->obj[0],22,24,180,52);
    GT_ENSURE(GT_Label(ctx->obj[0],"Button",18,10,140,GT_FONT_BODY));
    GT_Focus(ctx->obj[0]);
    GT_NEW(ctx,1,lv_checkbox_create(ctx->body));
    GT_Position(ctx->obj[1],24,112);
    lv_checkbox_set_text(ctx->obj[1],"Enabled");
    GT_Focus(ctx->obj[1]);
    GT_NEW(ctx,2,lv_switch_create(ctx->body));
    GT_Place(ctx->obj[2],290,110,92,44);
    GT_Focus(ctx->obj[2]);
    GT_ENSURE(GT_Label(ctx->body,"Pause to operate controls",22,236,420,GT_FONT_SMALL));
    return 1U;
}

/* GRAPHICS_TEST_UPDATE_MS(기본33ms) UI service에서 합성 값만 갱신한다. elapsed_ms는 화면 진입 이후 ms이고
 * speed는0..160 DEMO 값이다. 입력/출력은 이 화면 객체에만 한정된다. */
static void Tick(GT_Context *ctx,uint32_t elapsed_ms,uint32_t speed)
{
    uint32_t step=elapsed_ms/1000U;
    (void)speed;
    if(!g_graphics_test.auto_advance || step==ctx->step) return;
    ctx->step=step;
    lv_obj_set_state(ctx->obj[1],LV_STATE_CHECKED,(step&1U)!=0U);
    lv_obj_set_state(ctx->obj[2],LV_STATE_CHECKED,(step&1U)==0U);
}

const GT_Case gt_controls={{1U,"Controls","Button / checkbox / switch",GRAPHICS_TEST_CAP_RECT|GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_BASIC,"Auto toggles; manual focus/click when paused."},Build,Tick};
#else
const GT_Case gt_controls={{1U,"Controls","Button / checkbox / switch",GRAPHICS_TEST_CAP_RECT|GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_CONFIG_DISABLED,"LVGL widget is disabled in this build."},GT_Skip,GT_NoTick};
#endif

