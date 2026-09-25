#include "Graphics_Test_Internal.h"

#if LV_USE_SPAN
/* 화면 진입 시에만 생성한다. ctx는 이 case 소유 참조를 보관하고 NULL이면
 * 실패0을 돌려 공통 ERROR 진단으로 넘긴다. 하드웨어/RTOS 접근은 없다. */
static uint32_t Build(GT_Context *ctx)
{

    lv_span_t *a,*b;
    GT_NEW(ctx,0,lv_spangroup_create(ctx->body));
    GT_Place(ctx->obj[0],28,56,408,164);
    a=lv_spangroup_add_span(ctx->obj[0]); GT_ENSURE(a);
    b=lv_spangroup_add_span(ctx->obj[0]); GT_ENSURE(b);
    lv_span_set_text(a,"EVE ");
    lv_style_set_text_font(lv_span_get_style(a),GT_FONT_TITLE);
    lv_style_set_text_color(lv_span_get_style(a),lv_color_hex(GT_ACCENT));
    lv_span_set_text(b,"rich text\nwith separate spans");
    lv_style_set_text_font(lv_span_get_style(b),GT_FONT_BODY);
    lv_style_set_text_color(lv_span_get_style(b),lv_color_hex(GT_TEXT_COLOR));
    lv_spangroup_refresh(ctx->obj[0]);
    return 1U;
}

const GT_Case gt_span={{17U,"Rich text spans","Multiple text colors / fonts",GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_LIMITED,"ASCII styled text; no per-span transforms."},Build,GT_NoTick};
#else
const GT_Case gt_span={{17U,"Rich text spans","Multiple text colors / fonts",GRAPHICS_TEST_CAP_TEXT,GRAPHICS_TEST_SUPPORT_CONFIG_DISABLED,"LVGL widget is disabled in this build."},GT_Skip,GT_NoTick};
#endif

