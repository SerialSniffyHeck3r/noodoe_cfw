#include "Graphics_Test_Internal.h"

#if LV_USE_GRID
/* 화면 진입 시에만 생성한다. ctx는 이 case 소유 참조를 보관하고 NULL이면
 * 실패0을 돌려 공통 ERROR 진단으로 넘긴다. 하드웨어/RTOS 접근은 없다. */
static uint32_t Build(GT_Context *ctx)
{

    static const int32_t columns[]={LV_GRID_FR(1),LV_GRID_FR(1),LV_GRID_TEMPLATE_LAST};
    static const int32_t rows[]={LV_GRID_FR(1),LV_GRID_FR(1),LV_GRID_TEMPLATE_LAST};
    uint32_t i;
    GT_NEW(ctx,0,GT_Box(ctx->body,30,26,404,256));
    lv_obj_set_style_pad_all(ctx->obj[0],10,0);
    lv_obj_set_grid_dsc_array(ctx->obj[0],columns,rows);
    for(i=0;i<4U;++i) {
        GT_NEW(ctx,i+1U,GT_Box(ctx->obj[0],0,0,80,80));
        lv_obj_set_grid_cell(ctx->obj[i+1U],LV_GRID_ALIGN_STRETCH,(int32_t)(i%2U),1,
                            LV_GRID_ALIGN_STRETCH,(int32_t)(i/2U),1);
        lv_obj_set_style_bg_color(ctx->obj[i+1U],lv_color_hex(0x245C6CU+i*0x100A10U),0);
        GT_ENSURE(GT_Label(ctx->obj[i+1U],i==0?"A":i==1?"B":i==2?"C":"D",16,24,80,GT_FONT_TITLE));
    }
    return 1U;
}

const GT_Case gt_grid={{25U,"Grid layout","2 by 2 cells",GRAPHICS_TEST_CAP_RECT|GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_BASIC,"Fractional layout; no clipped rounded corners."},Build,GT_NoTick};
#else
const GT_Case gt_grid={{25U,"Grid layout","2 by 2 cells",GRAPHICS_TEST_CAP_RECT|GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_CONFIG_DISABLED,"LVGL widget is disabled in this build."},GT_Skip,GT_NoTick};
#endif

