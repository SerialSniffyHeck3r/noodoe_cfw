#include "Graphics_Test_Internal.h"
#include "Graphics_ArcSweep.h"

#if LV_USE_ARC && LV_USE_LABEL
#if defined(GRAPHICS_TEST_DASHBOARD_LEGACY) && GRAPHICS_TEST_DASHBOARD_LEGACY
/* 숫자/속도 ring과 중앙 오디오 정보를 함께 만드는 축소 대시보드다. 제목/가수는
 * 합성 ASCII 문자열이며 BT metadata를 읽지 않는다. 바깥 chrome의 아래96px는
 * 공통 time/ODO 영역으로 보존한다. 각 생성 실패는0으로 전파한다. */
static uint32_t Build(GT_Context *ctx)
{

    GT_NEW(ctx,0,lv_arc_create(ctx->body));
    GT_Place(ctx->obj[0],118,8,228,228);
    lv_arc_set_range(ctx->obj[0],0,160);
    lv_arc_set_bg_angles(ctx->obj[0],135,405);
    lv_obj_remove_flag(ctx->obj[0],LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(ctx->obj[0],12,LV_PART_MAIN);
    lv_obj_set_style_arc_width(ctx->obj[0],12,LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ctx->obj[0],lv_color_hex(GT_ACCENT),LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(ctx->obj[0],LV_OPA_TRANSP,LV_PART_KNOB);
    GT_NEW(ctx,1,GT_Label(ctx->body,"0",166,51,132,GT_FONT_SPEED));
    lv_obj_set_style_text_align(ctx->obj[1],LV_TEXT_ALIGN_CENTER,0);
    GT_NEW(ctx,2,GT_Label(ctx->body,"km/h  DEMO",164,105,136,GT_FONT_SMALL));
    lv_obj_set_style_text_align(ctx->obj[2],LV_TEXT_ALIGN_CENTER,0);
    /* 오디오 text도 ring 내부에 둔다. 고정 길이 문자열이라 긴 marquee/layer를
     * 만들지 않고 ring/숫자와 함께 한 frame에 표시할 수 있다. */
    GT_NEW(ctx,3,GT_Label(ctx->body,"Midnight Ride",142,140,180,GT_FONT_BODY));
    GT_NEW(ctx,4,GT_Label(ctx->body,"Noodoe Demo",142,170,180,GT_FONT_SMALL));
    lv_obj_set_style_text_align(ctx->obj[3],LV_TEXT_ALIGN_CENTER,0);
    lv_obj_set_style_text_align(ctx->obj[4],LV_TEXT_ALIGN_CENTER,0);
    GT_ENSURE(GT_Label(ctx->body,"DEMO AUDIO / PALETTE",120,254,300,GT_FONT_SMALL));
    lv_obj_set_style_bg_color(ctx->body,lv_color_hex(0x132C3AU),0);
    return 1U;
}

/* GRAPHICS_TEST_UPDATE_MS(기본33ms) UI service에서 합성 값만 갱신한다. elapsed_ms는 화면 진입 이후 ms이고
 * speed는0..160 DEMO 값이다. 입력/출력은 이 화면 객체에만 한정된다. */
static void Tick(GT_Context *ctx,uint32_t elapsed_ms,uint32_t speed)
{
    uint32_t step=elapsed_ms/2500U;
    lv_arc_set_value(ctx->obj[0],(int32_t)speed);
    lv_label_set_text_fmt(ctx->obj[1],"%lu",(unsigned long)speed);
    /* 2.5초마다 색과 오디오 샘플을 함께 바꾼다. 대형 bitmap 재업로드가 없고
     * gradient/whole-screen opacity도 쓰지 않아 RAM_G/DL 부담을 작게 유지한다. */
    if(step!=ctx->step) {
        ctx->step=step;
        lv_obj_set_style_bg_color(ctx->body,lv_color_hex((step&1U)?0x211C38U:0x132C3AU),0);
        lv_label_set_text(ctx->obj[3],(step&1U)?"City Lights":"Midnight Ride");
        lv_label_set_text(ctx->obj[4],(step&1U)?"Night Drive":"Noodoe Demo");
    }
}

const GT_Case gt_dashboard={{0U,"Dashboard","Speed / audio / palette",GRAPHICS_TEST_CAP_ARC|GRAPHICS_TEST_CAP_TEXT|GRAPHICS_TEST_CAP_RECT,GRAPHICS_TEST_SUPPORT_BASIC,"Synthetic speed/audio; alternating solid palette."},Build,Tick};
#else
/* 첫 case는 큰 호를 채우고 비우는 전용 시험이다. 전체480px 부모는 투명하고
 * 호는480x480 테두리까지 사용하고 최종 활성 원 mask가 경계를 자른다. 이전 audio/palette 소스는
 * 위 legacy define으로 보존하며 현재 화면에서는 차량/BT 값을 사용하지 않는다. */
static uint32_t Build(GT_Context *ctx)
{
    return GraphicsArcSweep_InitViewport(ctx->body);
}

/* 실제 경과시간으로4초 채움/4초 비움을 계산한다. 정수km/h는 표시용일 뿐이며
 * 호의 고해상도 값을 정수 속도로 양자화하지 않는다. 별도 task/timer는 없다. */
static void Tick(GT_Context *ctx,uint32_t elapsed_ms,uint32_t speed)
{
    (void)ctx; (void)speed;
    GraphicsArcSweep_Process(elapsed_ms);
}
const GT_Case gt_dashboard={{0U,"Arc sweep","270 degree speed ring",GRAPHICS_TEST_CAP_ARC|GRAPHICS_TEST_CAP_TEXT,
    GRAPHICS_TEST_SUPPORT_BASIC,"4 seconds fill / 4 seconds drain; actual FPS and CPU."},Build,Tick};
#endif
#else
const GT_Case gt_dashboard={{0U,"Dashboard","Speed / audio / palette",GRAPHICS_TEST_CAP_ARC|GRAPHICS_TEST_CAP_TEXT|GRAPHICS_TEST_CAP_RECT,GRAPHICS_TEST_SUPPORT_CONFIG_DISABLED,"LVGL widget is disabled in this build."},GT_Skip,GT_NoTick};
#endif
