#include "Graphics_Test_Internal.h"

#if LV_USE_MENU
/* 화면 진입 시에만 생성한다. ctx는 이 case 소유 참조를 보관하고 NULL이면
 * 실패0을 돌려 공통 ERROR 진단으로 넘긴다. 하드웨어/RTOS 접근은 없다. */
static uint32_t Build(GT_Context *ctx)
{

    lv_obj_t *row,*back;
    GT_NEW(ctx,0,lv_menu_create(ctx->body));
    GT_Place(ctx->obj[0],20,16,424,280);
    /* 기본 메뉴는 symbol을 lv_image source로 사용한다. EVE는 이 형식을 받지
     * 않으므로 내부 아이콘만 삭제하고 동일 back button 안에 ASCII label을 넣는다. */
    back=lv_menu_get_main_header_back_button(ctx->obj[0]); GT_ENSURE(back);
    lv_obj_clean(back);
    GT_ENSURE(GT_Label(back,"<",0,0,20,GT_FONT_BODY));
    GT_NEW(ctx,1,lv_menu_page_create(ctx->obj[0],"Menu"));
    GT_NEW(ctx,2,lv_menu_page_create(ctx->obj[0],"Details"));
    row=lv_menu_cont_create(ctx->obj[1]); GT_ENSURE(row);
    GT_ENSURE(GT_Label(row,"Open details",0,0,340,GT_FONT_BODY));
    lv_menu_set_load_page_event(ctx->obj[0],row,ctx->obj[2]);
    GT_Focus(row);
    GT_ENSURE(GT_Label(ctx->obj[2],"Second menu page",12,30,350,GT_FONT_BODY));
    lv_menu_set_page(ctx->obj[0],ctx->obj[1]);
    return 1U;
}

/* GRAPHICS_TEST_UPDATE_MS(기본33ms) UI service에서 합성 값만 갱신한다. elapsed_ms는 화면 진입 이후 ms이고
 * speed는0..160 DEMO 값이다. 입력/출력은 이 화면 객체에만 한정된다. */
static void Tick(GT_Context *ctx,uint32_t elapsed_ms,uint32_t speed)
{
    (void)speed;
    if(g_graphics_test.auto_advance && elapsed_ms>3500U && !ctx->step) {
        lv_menu_set_page(ctx->obj[0],ctx->obj[2]); ctx->step=1U;
    }
}

const GT_Case gt_menu={{14U,"Menu","Page / load-page event",GRAPHICS_TEST_CAP_RECT|GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_LIMITED,"One subpage; no sidebar transitions."},Build,Tick};
#else
const GT_Case gt_menu={{14U,"Menu","Page / load-page event",GRAPHICS_TEST_CAP_RECT|GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_CONFIG_DISABLED,"LVGL widget is disabled in this build."},GT_Skip,GT_NoTick};
#endif
