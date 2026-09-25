#include "Graphics_Test_Internal.h"

#if LV_USE_TEXTAREA && LV_USE_KEYBOARD
/* 화면 진입 시에만 생성한다. ctx는 이 case 소유 참조를 보관하고 NULL이면
 * 실패0을 돌려 공통 ERROR 진단으로 넘긴다. 하드웨어/RTOS 접근은 없다. */
static uint32_t Build(GT_Context *ctx)
{

    static const char *const map[]={"A","B","C","\n","1","2","3",""};
    static const lv_buttonmatrix_ctrl_t ctrl[]={1,1,1,1,1,1};
    GT_NEW(ctx,0,lv_textarea_create(ctx->body));
    GT_Place(ctx->obj[0],24,12,416,58);
    lv_textarea_set_one_line(ctx->obj[0],true);
    lv_textarea_set_max_length(ctx->obj[0],12);
    lv_textarea_set_text(ctx->obj[0],"EVE");
    GT_Focus(ctx->obj[0]);
    GT_NEW(ctx,1,lv_keyboard_create(ctx->body));
    GT_Place(ctx->obj[1],24,98,416,150);
    lv_keyboard_set_map(ctx->obj[1],LV_KEYBOARD_MODE_USER_1,map,ctrl);
    lv_keyboard_set_mode(ctx->obj[1],LV_KEYBOARD_MODE_USER_1);
    lv_keyboard_set_popovers(ctx->obj[1],false);
    lv_keyboard_set_textarea(ctx->obj[1],ctx->obj[0]);
    GT_Dense(ctx->obj[1]); GT_Focus(ctx->obj[1]);
    return 1U;
}

/* GRAPHICS_TEST_UPDATE_MS(기본33ms) UI service에서 합성 값만 갱신한다. elapsed_ms는 화면 진입 이후 ms이고
 * speed는0..160 DEMO 값이다. 입력/출력은 이 화면 객체에만 한정된다. */
static void Tick(GT_Context *ctx,uint32_t elapsed_ms,uint32_t speed)
{
    (void)speed;
    if(g_graphics_test.auto_advance && elapsed_ms/1000U!=ctx->step) {
        ctx->step=elapsed_ms/1000U;
        lv_textarea_set_text(ctx->obj[0],(ctx->step&1U) ? "ABC123" : "EVE");
    }
}

const GT_Case gt_keyboard={{9U,"Text input","Textarea / compact keyboard",GRAPHICS_TEST_CAP_RECT|GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_LIMITED,"Compact ASCII map; no IME/popovers."},Build,Tick};
#else
const GT_Case gt_keyboard={{9U,"Text input","Textarea / compact keyboard",GRAPHICS_TEST_CAP_RECT|GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_CONFIG_DISABLED,"LVGL widget is disabled in this build."},GT_Skip,GT_NoTick};
#endif

