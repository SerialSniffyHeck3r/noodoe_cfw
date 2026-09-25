#include "Graphics_Test_Internal.h"

#if LV_USE_TABLE
/* 화면 진입 시에만 생성한다. ctx는 이 case 소유 참조를 보관하고 NULL이면
 * 실패0을 돌려 공통 ERROR 진단으로 넘긴다. 하드웨어/RTOS 접근은 없다. */
static uint32_t Build(GT_Context *ctx)
{

    GT_NEW(ctx,0,lv_table_create(ctx->body));
    GT_Place(ctx->obj[0],32,26,400,236);
    lv_table_set_row_count(ctx->obj[0],3);
    lv_table_set_column_count(ctx->obj[0],2);
    lv_table_set_column_width(ctx->obj[0],0,GT_Px(200));
    lv_table_set_column_width(ctx->obj[0],1,GT_Px(190));
    lv_table_set_cell_value(ctx->obj[0],0,0,"Signal");
    lv_table_set_cell_value(ctx->obj[0],0,1,"DEMO");
    lv_table_set_cell_value(ctx->obj[0],1,0,"Speed");
    lv_table_set_cell_value(ctx->obj[0],2,0,"Mode");
    lv_table_set_cell_value(ctx->obj[0],2,1,"AUTO");
    GT_Focus(ctx->obj[0]);
    return 1U;
}

/* GRAPHICS_TEST_UPDATE_MS(기본33ms) UI service에서 합성 값만 갱신한다. elapsed_ms는 화면 진입 이후 ms이고
 * speed는0..160 DEMO 값이다. 입력/출력은 이 화면 객체에만 한정된다. */
static void Tick(GT_Context *ctx,uint32_t elapsed_ms,uint32_t speed)
{
    (void)elapsed_ms;
    lv_table_set_cell_value_fmt(ctx->obj[0],1,1,"%lu km/h",(unsigned long)speed);
}

const GT_Case gt_table={{4U,"Table","3 rows / 2 columns",GRAPHICS_TEST_CAP_RECT|GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_BASIC,"Small cells and changing numeric value."},Build,Tick};
#else
const GT_Case gt_table={{4U,"Table","3 rows / 2 columns",GRAPHICS_TEST_CAP_RECT|GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_CONFIG_DISABLED,"LVGL widget is disabled in this build."},GT_Skip,GT_NoTick};
#endif
