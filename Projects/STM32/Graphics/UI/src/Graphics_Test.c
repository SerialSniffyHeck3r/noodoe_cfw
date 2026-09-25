#include "Graphics_Test_Internal.h"
#include "Graphics_ArcSweep.h"
#include <string.h>

/* 이 배열이 public case 번호/순차 실행 순서를 함께 정의한다. 미지원 항목도
 * 건너뛴 사실을 화면과 진단에서 확인할 수 있도록 번호를 보존한다. */
static const GT_Case *const cases[] = {
#if defined(NOODOE_INTEGRATED) && NOODOE_INTEGRATED
    /* 통합 구성의 코드 공간을 확보한다. 전체 시험은 Graphics 구성에 보존한다. */
    &gt_dashboard
#else
    &gt_dashboard, &gt_controls, &gt_meters, &gt_chart, &gt_table,
    &gt_scale, &gt_spinbox, &gt_dropdown, &gt_roller, &gt_keyboard,
    &gt_buttonmatrix, &gt_tabview, &gt_tileview, &gt_window, &gt_menu,
    &gt_msgbox, &gt_spinner, &gt_span, &gt_image, &gt_animimage,
    &gt_imagebutton, &gt_line_led, &gt_calendar, &gt_list, &gt_flex,
    &gt_grid, &gt_scroll, &gt_events, &gt_animation, &gt_labels,
    &gt_skip_canvas, &gt_skip_layer, &gt_skip_shadow, &gt_skip_gradient,
    &gt_skip_mask, &gt_skip_svg, &gt_skip_lottie, &gt_skip_file_image,
    &gt_skip_arclabel, &gt_skip_texture3d, &gt_skip_ime
#endif
};
#define GT_COUNT ((uint32_t)(sizeof(cases)/sizeof(cases[0])))

volatile GraphicsTest_Diagnostics g_graphics_test;
GraphicsTest_Result g_graphics_test_results[GT_COUNT];
static GT_Context context;
static lv_obj_t *root_obj, *title_obj, *status_obj, *feature_obj, *footer_obj, *help_obj, *performance_obj;
static lv_style_t flat_style, palette_style, selection_style;
static uint32_t style_ready, last_ui_ms, boot_ms, armed_mask, short_mask;
/* 고정 문자열을 매 frame마다 재할당/레이아웃하지 않도록 chrome의 마지막 값을
 * 기억한다. 진단 구조의 ABI는 바꾸지 않으며 실제 진단 값 갱신과 화면 갱신을 분리한다. */
static uint32_t chrome_case, chrome_mode, chrome_state, chrome_seconds, chrome_status_ms;
static uint32_t performance_ms;
static uint32_t case_pinned;
#define GT_STATUS_UPDATE_MS 250U

/* 같은 task에서만 호출한다. NULL은 즉시 관측 가능한 ERROR로 기록하며 이후
 * 그 객체를 사용하지 않는다. LVGL 내부 allocation assert는 포트 fault 진단 대상이다. */
uint32_t GT_Check(const void *ptr)
{
    if(ptr != NULL) return 1U;
    ++g_graphics_test.allocation_failures;
    g_graphics_test.state = GRAPHICS_TEST_ERROR;
    if(g_graphics_test.current_case < GT_COUNT) {
        ++g_graphics_test_results[g_graphics_test.current_case].allocation_failures;
        g_graphics_test_results[g_graphics_test.current_case].state = GRAPHICS_TEST_ERROR;
    }
    return 0U;
}

/* 공통 스타일은 한 번 할당하여 모든 화면이 공유한다. 개별 객체에 큰 스타일을
 * 복제하지 않고 GPU가 처리하지 않는 layer/shadow/gradient/mask 유발 속성을 끈다. */
static void InitFlatStyle(void)
{
    if(style_ready) return;
    lv_style_init(&flat_style);
    lv_style_set_shadow_width(&flat_style, 0);
    lv_style_set_outline_width(&flat_style, 0);
    lv_style_set_bg_grad_dir(&flat_style, LV_GRAD_DIR_NONE);
    lv_style_set_clip_corner(&flat_style, false);
    lv_style_set_opa(&flat_style, LV_OPA_COVER);
    lv_style_set_transform_rotation(&flat_style, 0);
    lv_style_set_transform_scale_x(&flat_style, LV_SCALE_NONE);
    lv_style_set_transform_scale_y(&flat_style, LV_SCALE_NONE);
    /* simple theme의 WHITE/LIGHT 배경 위에 흰 글자가 겹치지 않게 색만 바꾼다.
     * bg_opa는 설정하지 않아 label/arc의 투명 배경을 불투명하게 만들지 않는다.
     * 일반 shared style이므로 같은 selector의 builder local color가 우선한다. */
    lv_style_init(&palette_style);
    lv_style_set_bg_color(&palette_style,lv_color_hex(GT_PANEL));
    lv_style_set_text_color(&palette_style,lv_color_hex(GT_TEXT_COLOR));
    lv_style_init(&selection_style);
    lv_style_set_bg_color(&selection_style,lv_color_hex(0x245C6CU));
    lv_style_set_text_color(&selection_style,lv_color_hex(GT_TEXT_COLOR));
    style_ready = 1U;
}

/* 생성 완료된 자식까지 단순 스타일을 적용한다. 해당 화면이 소유한 트리만
 * 방문하며, 동적 dropdown popup에는 생성 직후 다시 호출한다. */
void GT_FlatTree(lv_obj_t *obj)
{
    uint32_t i;
    if(obj == NULL) return;
    lv_obj_add_style(obj, &flat_style, LV_PART_MAIN);
    lv_obj_add_style(obj, &flat_style, LV_PART_INDICATOR);
    lv_obj_add_style(obj, &flat_style, LV_PART_ITEMS);
    lv_obj_add_style(obj, &flat_style, LV_PART_KNOB);
    /* 배경 palette를 flat style과 분리한다. INDICATOR/KNOB에는 적용하지 않아
     * slider·switch·checkbox의 기능상 색 대비와 local LED/box 색을 보존한다. */
    lv_obj_add_style(obj, &palette_style, LV_PART_MAIN);
    lv_obj_add_style(obj, &palette_style, LV_PART_ITEMS);
    /* 상태 selector가 더 구체적이면 theme의 CHECKED/LIGHT가 기본 palette보다
     * 우선한다. keyboard checked 및 dropdown/roller 선택줄은 별도로 어둡게 맞춘다. */
    lv_obj_add_style(obj, &selection_style, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_add_style(obj, &selection_style, LV_PART_SELECTED);
    lv_obj_add_style(obj, &selection_style, LV_PART_SELECTED | LV_STATE_CHECKED);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
    for(i=0; i<lv_obj_get_child_count(obj); ++i) GT_FlatTree(lv_obj_get_child(obj, i));
}

/* 시험 내용에만 적용하는 좌표 변환. root/chrome은 물리 pixel 좌표를 쓴다. */
int32_t GT_Px(int32_t logical) { return logical*3/4; }
static uint32_t IsContent(lv_obj_t *parent)
{
    for(; parent && parent!=root_obj; parent=lv_obj_get_parent(parent))
        if(parent==context.body) return 1U;
    return 0U;
}
void GT_Position(lv_obj_t *obj, int32_t x, int32_t y)
{
    if(IsContent(lv_obj_get_parent(obj))) { x=GT_Px(x); y=GT_Px(y); }
    lv_obj_set_pos(obj,x,y);
}
/* 원형 내부 content 크기에 맞추되 글꼴/이미지 자체를 GPU transform하지 않는다. */
void GT_Place(lv_obj_t *obj, int32_t x, int32_t y, int32_t w, int32_t h)
{
    if(IsContent(lv_obj_get_parent(obj))) { w=GT_Px(w); h=GT_Px(h); }
    GT_Position(obj, x, y);
    lv_obj_set_size(obj, w, h);
}

/* 배경 상자를 만들어 반환한다. 부모가 삭제되면 함께 반환되고 NULL은 위로 전파한다. */
lv_obj_t *GT_Box(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h)
{
    lv_obj_t *obj = lv_obj_create(parent);
    if(!GT_Check(obj)) return NULL;
    GT_Place(obj, x, y, w, h);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(GT_PANEL), 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

/* ASCII 시험 문자열을 복사하는 label 생성기다. 길이는 화면 폭으로 제한하고
 * 자동 circular scrolling을 사용하지 않아 불필요한 animation/cache 증가를 막는다. */
lv_obj_t *GT_Label(lv_obj_t *parent, const char *text, int32_t x, int32_t y,
                   int32_t width, const lv_font_t *font)
{
    lv_obj_t *obj = lv_label_create(parent);
    if(!GT_Check(obj)) return NULL;
    if(IsContent(parent)) width=GT_Px(width);
    GT_Position(obj, x, y);
    lv_obj_set_width(obj, width);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(GT_TEXT_COLOR), 0);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_WRAP);
    lv_label_set_text(obj, text);
    return obj;
}

/* 키가 많은 위젯은 항목 경계/여백을 줄여 EVE 2048-word display list를 아낀다. */
void GT_Dense(lv_obj_t *obj)
{
    lv_obj_set_style_text_font(obj, GT_FONT_SMALL, LV_PART_MAIN);
    lv_obj_set_style_text_font(obj, GT_FONT_SMALL, LV_PART_ITEMS);
    lv_obj_set_style_border_width(obj, 0, LV_PART_ITEMS);
    lv_obj_set_style_pad_all(obj, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_all(obj, 1, LV_PART_ITEMS);
    lv_obj_set_style_radius(obj, 0, LV_PART_ITEMS);
}

/* 위젯 이벤트는 UI task에서만 누적한다. API로 만든 synthetic VALUE_CHANGED도
 * 포함되므로 사용자 물리 입력 횟수 또는 PASS로 해석하지 않는다. */
void GT_Event(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    if(code == LV_EVENT_CLICKED || code == LV_EVENT_VALUE_CHANGED || code == LV_EVENT_FOCUSED) {
        ++g_graphics_test.ui_events;
        if(g_graphics_test.current_case < GT_COUNT) ++g_graphics_test_results[g_graphics_test.current_case].ui_events;
    }
}

/* 이미 생성된 focus 대상만 포트의 그룹에 추가한다. 소멸 시 LVGL이 그룹에서도
 * 제거한다. 수동 모드의 encoder/ENTER 처리 자체는 Graphics 포트가 담당한다. */
void GT_Focus(lv_obj_t *obj)
{
    lv_group_t *group = Graphics_GetFocusGroup();
    if(!GT_Check(obj)) return;
    if(group != NULL) lv_group_add_obj(group, obj);
    lv_obj_add_event_cb(obj, GT_Event, LV_EVENT_ALL, NULL);
    lv_obj_set_style_border_color(obj, lv_color_hex(GT_ACCENT), LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(obj, 2, LV_STATE_FOCUSED);
}

/* 위험 경로는 생성하지 않고 현재 case의 명시적 사유만 보여 준다. */
uint32_t GT_Skip(GT_Context *ctx)
{
    const GraphicsTest_CaseInfo *info = &cases[g_graphics_test.current_case]->info;
    GT_ENSURE(GT_Label(ctx->body, "SKIPPED / UNSUPPORTED", 20, 42, 420, GT_FONT_BODY));
    GT_ENSURE(GT_Label(ctx->body, info->note, 20, 92, 420, GT_FONT_BODY));
    GT_ENSURE(GT_Label(ctx->body, "No risky draw task submitted.", 20, 238, 420, GT_FONT_SMALL));
    return 1U;
}

/* 정적인 화면은 추가 작업 없이 유지한다. 모든 case가 같은 callback 형식을 사용한다. */
void GT_NoTick(GT_Context *ctx, uint32_t elapsed_ms, uint32_t speed)
{
    (void)ctx; (void)elapsed_ms; (void)speed;
}

/* 객체에 연결된 UI animation을 부모부터 취소한다. 전체 LVGL animation 삭제는
 * 다른 포트 객체에 영향을 주므로 사용하지 않는다. widget destructor가 내부 timer를 정리한다. */
static void CancelTree(lv_obj_t *obj)
{
    uint32_t i;
    if(obj == NULL) return;
    lv_anim_delete(obj, NULL);
    for(i=0; i<lv_obj_get_child_count(obj); ++i) CancelTree(lv_obj_get_child(obj, i));
}

/* chrome의 의미상 변화만 LVGL에 전달한다. title/features/help는 전환 시, clock은
 * 초 변경 시, frame/event 상태줄은250ms 또는 상태 변경 시 갱신한다. LVGL label
 * setter는 같은 문자열도 재할당할 수 있으므로 단순히 호출 횟수를 늘리지 않는다.
 * force는 새 case가 같은 id여도 모든 chrome을 새로 표시할 때만1이다. */
static void UpdateChrome(uint32_t now, uint32_t force)
{
    /* 호 시험은 자체 큰 원/숫자/HUD를 소유한다. 숨긴 suite 문자열을 계속
     * 재할당하여 부하를 만들지 않는다. 다른 case의 chrome은 기존 경로다. */
    if(g_graphics_test.current_case==0U) return;
    const char *state = "RUNNING";
    uint32_t seconds = (now-boot_ms)/1000U;
    if(g_graphics_test.state == GRAPHICS_TEST_RENDERED) state="RENDERED";
    if(g_graphics_test.state == GRAPHICS_TEST_SKIPPED) state="SKIPPED";
    if(g_graphics_test.state == GRAPHICS_TEST_ERROR) state="ERROR";
    if(force || chrome_case!=g_graphics_test.current_case) {
        lv_label_set_text_fmt(title_obj, "%02lu/%02lu  %s", (unsigned long)g_graphics_test.current_case+1UL,
                          (unsigned long)GT_COUNT, cases[g_graphics_test.current_case]->info.title);
        lv_label_set_text(feature_obj,cases[g_graphics_test.current_case]->info.features);
        chrome_case=g_graphics_test.current_case;
    }
    if(force || chrome_state!=g_graphics_test.state || chrome_mode!=g_graphics_test.auto_advance ||
       now-chrome_status_ms>=GT_STATUS_UPDATE_MS) {
        lv_label_set_text_fmt(status_obj, "%s %s F:%lu E:%lu", state,
                          g_graphics_test.auto_advance ? "AUTO" : "MANUAL",
                          (unsigned long)g_graphics_test_results[g_graphics_test.current_case].frames,
                          (unsigned long)g_graphics_test_results[g_graphics_test.current_case].ui_events);
        chrome_state=g_graphics_test.state;
        chrome_status_ms=now;
    }
    if(force || chrome_seconds!=seconds) {
        lv_label_set_text_fmt(footer_obj, "DEMO %02lu:%02lu:%02lu\nODO %lu.%lu km",
                          (unsigned long)((seconds/3600U)%24U), (unsigned long)((seconds/60U)%60U),
                          (unsigned long)(seconds%60U), (unsigned long)(36475U+seconds/3600U),
                          (unsigned long)((seconds/360U)%10U));
        chrome_seconds=seconds;
    }
    if(force || chrome_mode!=g_graphics_test.auto_advance) {
        lv_label_set_text(help_obj, g_graphics_test.auto_advance ?
        "Hold ENTER: manual" : "Hold ENTER: auto");
        chrome_mode=g_graphics_test.auto_advance;
    }
    if(force || now-performance_ms>=1000U) {
        const volatile Graphics_Performance *p=Graphics_GetPerformance();
        if(p->valid) lv_label_set_text_fmt(performance_obj,"%lu.%lu FPS CPU %lu.%lu%%",
            (unsigned long)(p->fps_tenths/10U),(unsigned long)(p->fps_tenths%10U),
            (unsigned long)(p->cpu_tenths/10U),(unsigned long)(p->cpu_tenths%10U));
        else lv_label_set_text(performance_obj,"FPS --  CPU --");
        performance_ms=now;
    }
}

/* 생성된 포트 화면은 소유하지 않는다. 그 아래 UI 전용 root만 만들고 재초기화 시
 * 그 root만 삭제한다. lv_timer_handler/하드웨어를 여기서 호출하지 않는다. */
uint32_t GraphicsTest_Init(void)
{
    uint32_t i;
    lv_obj_t *screen = Graphics_GetScreen();
    if(screen == NULL || Graphics_GetDisplay() == NULL) return 0U;
    if(root_obj != NULL) { CancelTree(root_obj); lv_obj_delete(root_obj); root_obj=NULL; }
    memset((void*)&g_graphics_test, 0, sizeof(g_graphics_test));
    memset(g_graphics_test_results, 0, sizeof(g_graphics_test_results));
    memset(&context, 0, sizeof(context));
    g_graphics_test.magic=GRAPHICS_TEST_DIAGNOSTIC_MAGIC;
    g_graphics_test.version=GRAPHICS_TEST_DIAGNOSTIC_VERSION;
    g_graphics_test.case_count=GT_COUNT;
    g_graphics_test.auto_advance=1U;
    armed_mask=short_mask=0U;
    for(i=0; i<GT_COUNT; ++i) g_graphics_test_results[i].id=i;
    InitFlatStyle();
#if LV_USE_THEME_SIMPLE
    lv_display_set_theme(Graphics_GetDisplay(), lv_theme_simple_init(Graphics_GetDisplay()));
#endif
    root_obj=GT_Box(screen,0,0,480,480);
    if(root_obj == NULL) return 0U;
    lv_obj_set_style_bg_color(root_obj,lv_color_black(),0);
    title_obj=GT_Label(root_obj,"UI test",112,44,256,GT_FONT_BODY);
    status_obj=GT_Label(root_obj,"RUNNING",100,350,280,GT_FONT_SMALL);
    feature_obj=GT_Label(root_obj,"",86,77,308,GT_FONT_SMALL);
    context.body=GT_Box(root_obj,66,112,348,225);
    footer_obj=GT_Label(root_obj,"DEMO",100,378,280,GT_FONT_SMALL);
    help_obj=GT_Label(root_obj,"",102,415,276,GT_FONT_SMALL);
    performance_obj=GT_Label(root_obj,"FPS -- CPU --",150,438,180,GT_FONT_SMALL);
    if(title_obj==NULL || status_obj==NULL || feature_obj==NULL || context.body==NULL || footer_obj==NULL || help_obj==NULL || performance_obj==NULL) return 0U;
    /* 모든 chrome의 고정 높이와 긴 문장 생략을 함께 설정한다. 아래 영역의 폭을
     * 다시 넓히면 원형 창 바깥으로 나가므로 생성 시 네 꼭짓점을 검사한다. */
    lv_obj_t *labels[]={title_obj,status_obj,feature_obj,footer_obj,help_obj,performance_obj};
    const int32_t heights[]={26,18,18,34,18,18};
    for(i=0; i<6; ++i) {
        lv_obj_set_height(labels[i],heights[i]);
        lv_obj_set_style_text_align(labels[i],LV_TEXT_ALIGN_CENTER,0);
        if(labels[i]!=footer_obj) lv_label_set_long_mode(labels[i],LV_LABEL_LONG_DOT);
    }
    lv_obj_update_layout(root_obj);
    for(i=0; i<6; ++i) {
        lv_area_t area; lv_obj_get_coords(labels[i],&area);
        if(!Graphics_IsAreaVisible(&area)) return 0U;
    }
    lv_area_t body_area; lv_obj_get_coords(context.body,&body_area);
    if(!Graphics_IsAreaVisible(&body_area)) return 0U;
    GT_FlatTree(root_obj);
    boot_ms=last_ui_ms=lv_tick_get();
    chrome_case=chrome_mode=chrome_state=chrome_seconds=UINT32_MAX;
    chrome_status_ms=boot_ms;
    g_graphics_test.initialized=1U;
    Graphics_SetInputEnabled(0U);
    Graphics_SetTargetFPS(30U);
    Graphics_SetContinuousRendering(1U);
    return GraphicsTest_Select(0U);
}

/* LVGL allocator가 살아 있는 동안 UI 소유 할당부터 반환한다. 부모 포트 화면과
 * focus group은 소유하지 않으므로 삭제하지 않는다. context/정적 포인터까지 비워
 * 뒤이은 lv_deinit 및 다음 Graphics_Init에서 이전 heap 주소를 재사용하지 않게 한다. */
void GraphicsTest_Shutdown(void)
{
    g_graphics_test.initialized=0U;
    GraphicsArcSweep_Destroy();
    if(root_obj != NULL) {
        CancelTree(root_obj);
        lv_obj_delete(root_obj);
    }
    root_obj=title_obj=status_obj=feature_obj=footer_obj=help_obj=performance_obj=NULL;
    memset(&context,0,sizeof(context));
    if(style_ready) {
        lv_style_reset(&flat_style);
        lv_style_reset(&palette_style);
        lv_style_reset(&selection_style);
        style_ready=0U;
    }
    armed_mask=short_mask=last_ui_ms=boot_ms=0U;
    chrome_case=chrome_mode=chrome_state=chrome_seconds=UINT32_MAX;
    chrome_status_ms=0U;
    g_graphics_test.boot_held_mask=0U;
    case_pinned=0U;
}

/* 한 화면의 객체/animation을 모두 반환한 뒤 다음 화면을 만든다. 화면 번호는
 * 항상 유효 범위 내에서만 공개하고, build 실패 후에는 더 이상 tick을 호출하지 않는다. */
uint32_t GraphicsTest_Select(uint32_t index)
{
    uint32_t okay;
    lv_obj_t *body;
    if(!g_graphics_test.initialized || index>=GT_COUNT) return 0U;
    GraphicsArcSweep_Destroy();
    case_pinned=0U;
    body=context.body;
    CancelTree(body);
    lv_obj_clean(body);
    /* body는 재사용하지만 이전 case의 palette/local style/layout은 재사용하지
     * 않는다. 배경·불투명도·여백을 기준값으로 복원한 후 새 case를 생성한다. */
    lv_obj_remove_style_all(body);
    lv_obj_set_layout(body,LV_LAYOUT_NONE);
    GT_Place(body,66,112,348,225);
    lv_obj_set_style_pad_all(body,0,0);
    lv_obj_set_style_border_width(body,0,0);
    lv_obj_set_style_radius(body,0,0);
    lv_obj_set_style_bg_color(body,lv_color_hex(GT_PANEL),0);
    lv_obj_set_style_bg_opa(body,LV_OPA_COVER,0);
    lv_obj_remove_flag(body,LV_OBJ_FLAG_SCROLLABLE);
    memset(&context,0,sizeof(context));
    context.body=body;
    /* 같은41개 번호를 유지한다. 첫 화면만 디스플레이 원점을 사용하는
     * 전용 호 시험이고, 나머지는 안전 사각 viewport를 계속 사용한다. */
    lv_obj_t *chrome[]={title_obj,status_obj,feature_obj,footer_obj,help_obj,performance_obj};
    for(uint32_t i=0;i<6U;++i) {
        if(index==0U) lv_obj_add_flag(chrome[i],LV_OBJ_FLAG_HIDDEN);
        else lv_obj_remove_flag(chrome[i],LV_OBJ_FLAG_HIDDEN);
    }
    if(index==0U) {
        lv_obj_set_pos(body,0,0);
        lv_obj_set_size(body,GRAPHICS_WIDTH,GRAPHICS_HEIGHT);
        lv_obj_set_style_bg_opa(body,LV_OPA_TRANSP,0);
    }
    g_graphics_test.current_case=index;
    g_graphics_test.state=GRAPHICS_TEST_RUNNING;
    g_graphics_test.case_started_ms=lv_tick_get();
    ++g_graphics_test.transition_count;
    ++g_graphics_test_results[index].visits;
    g_graphics_test_results[index].state=GRAPHICS_TEST_RUNNING;
    okay=cases[index]->build(&context);
    if(g_graphics_test.state==GRAPHICS_TEST_ERROR) okay=0U;
    if(okay && cases[index]->info.support>=GRAPHICS_TEST_SUPPORT_UNSUPPORTED) {
        g_graphics_test.state=GRAPHICS_TEST_SKIPPED;
        g_graphics_test_results[index].state=GRAPHICS_TEST_SKIPPED;
    }
    if(!okay) { g_graphics_test.state=GRAPHICS_TEST_ERROR; g_graphics_test_results[index].state=GRAPHICS_TEST_ERROR; }
    /* 호 컴포넌트는 theme를 제거한 자체 스타일이다. suite palette를 덧씌워
     * 투명 container/둥근 arc 끝의 색과 상태를 바꾸지 않는다. */
    if(index!=0U) GT_FlatTree(body);
    UpdateChrome(lv_tick_get(),1U);
    Graphics_Invalidate();
    return okay;
}

/* 순환은 unsigned wrap을 이용하지 않고 배열 경계를 명시한다. */
uint32_t GraphicsTest_Next(void) { return GraphicsTest_Select((g_graphics_test.current_case+1U)%GT_COUNT); }
uint32_t GraphicsTest_Previous(void) { return GraphicsTest_Select(g_graphics_test.current_case ? g_graphics_test.current_case-1U : GT_COUNT-1U); }

/* 재개 후 곧바로 전환되지 않도록 8초 시계를 새로 잡는다. 실제 encoder 전달은
 * 포트가 계속 처리하며 UI는 수동 모드에서 navigation 명령을 추가하지 않는다. */
void GraphicsTest_SetAutoAdvance(uint32_t enabled)
{
    g_graphics_test.auto_advance=enabled ? 1U : 0U;
    g_graphics_test.case_started_ms=lv_tick_get();
    /* 포트 observer는 계속 수신하고 LVGL encoder 전달만 막는다. 자동 순환 중
     * ENTER나 DOWN이 시험 위젯도 동시에 조작하는 것을 방지한다. */
    Graphics_SetInputEnabled(enabled ? 0U : 1U);
}

void GraphicsTest_SetCasePinned(uint32_t enabled) { case_pinned=enabled ? 1U : 0U; }
uint32_t GraphicsTest_IsCasePinned(void) { return case_pinned; }

/* 숫자/arc는 기존6.4초에0→160→0을 유지한다. 표본 간격만 기본33ms로 줄여
 * 약10fps였던 합성 움직임에 약30fps의 변경 기회를 준다. 느린 frame 뒤 밀린
 * 표본을 연속 재생하지 않고 현재 시각의 값 한 번만 사용한다. unsigned 차이와
 * 정수 주기 누적으로 tick wrap/서비스 지연을 처리하며 task delay는 호출하지 않는다. */
void GraphicsTest_Process(void)
{
    uint32_t now, phase, speed, elapsed;
    if(!g_graphics_test.initialized) return;
    ++g_graphics_test.process_count;
    now=lv_tick_get();
    if(g_graphics_test.auto_advance && !case_pinned && g_graphics_test.state!=GRAPHICS_TEST_ERROR &&
       now-g_graphics_test.case_started_ms>=GRAPHICS_TEST_CASE_MS) GraphicsTest_Next();
    elapsed=now-last_ui_ms;
    if(elapsed<GRAPHICS_TEST_UPDATE_MS) return;
    last_ui_ms+=(elapsed/GRAPHICS_TEST_UPDATE_MS)*GRAPHICS_TEST_UPDATE_MS;
    phase=((now-boot_ms)/20U)%320U;
    speed=phase<=160U ? phase : 320U-phase;
    g_graphics_test.demo_speed=speed;
    /* 수동 조작 중 사용자가 바꾼 slider/selection 값을 합성 tick이 덮지 않는다.
     * LVGL 소유 spinner/animation은 Graphics_Process에서 계속 동작한다. */
    if(g_graphics_test.auto_advance &&
       (g_graphics_test.state==GRAPHICS_TEST_RUNNING || g_graphics_test.state==GRAPHICS_TEST_RENDERED))
        cases[g_graphics_test.current_case]->tick(&context,now-g_graphics_test.case_started_ms,speed);
    UpdateChrome(now,0U);
}

/* 정적 descriptor와 현재 세션 결과를 읽기 전용으로 노출한다. */
uint32_t GraphicsTest_GetCaseCount(void) { return GT_COUNT; }
const GraphicsTest_CaseInfo *GraphicsTest_GetCaseInfo(uint32_t i) { return i<GT_COUNT ? &cases[i]->info : NULL; }
const GraphicsTest_Result *GraphicsTest_GetResults(uint32_t *count) { if(count) *count=GT_COUNT; return g_graphics_test_results; }
const volatile GraphicsTest_Diagnostics *GraphicsTest_GetDiagnostics(void) { return &g_graphics_test; }

/* 부팅부터 LOW였던 입력은 release로 block만 풀고 같은 누름의 SHORT는 버린다.
 * 정상 새 PRESS부터 arm하여 고장난 UP의 boot-held 이벤트가 UI를 지배하지 않게 한다. */
void GraphicsTest_SetBootHeldMask(uint32_t mask)
{
    g_graphics_test.boot_held_mask=mask&7U;
    armed_mask &= ~mask;
    short_mask &= ~mask;
}

/* PRESS/RELEASE/SHORT는 BSP 이벤트 순서를 따라 같은 UI task에서 전달한다.
 * 2초 초과 ENTER는 release 시 auto/manual 전환, SHORT는 자동 모드 UP/DOWN만
 * 페이지 전환한다. 수동 위젯 focus/edit/click은 포트 encoder가 단독 소유한다. */
void GraphicsTest_ObserveButton(GraphicsTest_Button button, GraphicsTest_ButtonEvent event, uint32_t duration_ms)
{
    uint32_t bit;
    if((uint32_t)button>=3U || !g_graphics_test.initialized) return;
    bit=1UL<<(uint32_t)button;
    ++g_graphics_test.button_events;
    if(g_graphics_test.boot_held_mask&bit) {
        if(event==GRAPHICS_TEST_BUTTON_RELEASE) g_graphics_test.boot_held_mask &= ~bit;
        armed_mask &= ~bit; short_mask &= ~bit;
        if(event==GRAPHICS_TEST_BUTTON_SHORT || event==GRAPHICS_TEST_BUTTON_RELEASE) ++g_graphics_test.blocked_button_commands;
        return;
    }
    if(event==GRAPHICS_TEST_BUTTON_PRESS) { armed_mask|=bit; short_mask &= ~bit; }
    if(event==GRAPHICS_TEST_BUTTON_RELEASE) {
        if(armed_mask&bit) {
            if(button==GRAPHICS_TEST_BUTTON_ENTER && duration_ms>=2001U) {
                GraphicsTest_SetAutoAdvance(!g_graphics_test.auto_advance);
                short_mask &= ~bit;
            } else short_mask |= bit;
        }
        armed_mask &= ~bit;
    }
    if(event==GRAPHICS_TEST_BUTTON_SHORT) {
        if(!(short_mask&bit)) { ++g_graphics_test.blocked_button_commands; return; }
        short_mask &= ~bit;
        if(g_graphics_test.auto_advance) {
            if(button==GRAPHICS_TEST_BUTTON_UP) GraphicsTest_Previous();
            if(button==GRAPHICS_TEST_BUTTON_DOWN) GraphicsTest_Next();
        }
    }
}

/* 포트가 실제 frame 제출에 성공한 경우만 결과를 올린다. SKIPPED/ERROR는
 * 텍스트가 그려져도 상태를 바꾸지 않으며 RENDERED는 육안 검증 PASS가 아니다. */
void GraphicsTest_NotifyRendered(uint32_t frame_sequence)
{
    GraphicsTest_Result *result;
    if(!g_graphics_test.initialized || frame_sequence==g_graphics_test.last_frame_sequence) return;
    g_graphics_test.last_frame_sequence=frame_sequence;
    ++g_graphics_test.rendered_frames;
    result=&g_graphics_test_results[g_graphics_test.current_case];
    ++result->frames;
    if(g_graphics_test.state==GRAPHICS_TEST_RUNNING) {
        g_graphics_test.state=GRAPHICS_TEST_RENDERED;
        result->state=GRAPHICS_TEST_RENDERED;
        ++result->rendered_visits;
    }
}
