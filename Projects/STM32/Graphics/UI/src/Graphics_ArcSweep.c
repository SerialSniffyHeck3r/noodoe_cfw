#include "Graphics_ArcSweep.h"
#include "Graphics.h"
#include "Graphics_BringupHUD.h"
#include <string.h>

/* UI 소유 task 한 곳에서만 변경한다. LVGL 객체 수명을 따라 삭제 callback이
 * 모든 포인터를 지우므로 다른 화면으로 전환한 뒤 stale object를 쓰지 않는다. */
typedef struct {
    lv_obj_t *root, *arc, *speed, *direction, *performance;
    uint32_t last_elapsed_ms, phase_ms, last_hud_ms;
    uint32_t last_value, last_speed, last_direction;
    int32_t center_x, center_y, radius;
} ArcSweep_Context;

static ArcSweep_Context sweep;
volatile GraphicsArcSweep_Diagnostics g_graphics_arc_sweep;

/* 초기화 시에만 사용하는 floor(sqrt(value))다. 제한된240px 반경 때문에
 * 정수 곱셈은 overflow하지 않는다. frame 경로에는 기하 계산을 넣지 않는다. */
static int32_t RootFloor(int32_t value, int32_t limit)
{
    int32_t lo = 0, hi = limit;
    while(lo < hi) {
        int32_t mid = (lo + hi + 1) / 2;
        if(mid * mid <= value) lo = mid;
        else hi = mid - 1;
    }
    return lo;
}

/* 원 전체의 정수 scanline을 현재 보드 표시 계약으로 검사한다. 중심이 보드의
 * 원점과 다른 경우에도 네 극점만 검사하는 불충분한 판정을 하지 않는다.
 * 실제 mask/검은 테두리 근거가 바뀌어도 Graphics_IsAreaVisible을 그대로 따른다. */
static uint32_t CircleVisible(int32_t cx, int32_t cy, int32_t radius)
{
    for(int32_t y = -radius; y <= radius; ++y) {
        int32_t half = RootFloor(radius * radius - y * y, radius);
        lv_area_t row = {cx - half, cy + y, cx + half, cy + y};
        if(!Graphics_IsAreaVisible(&row)) return 0U;
    }
    return 1U;
}

/* 부모 삭제 또는 Destroy 경로 모두 이 callback을 지난다. DELETE의 target은
 * 등록한 root여야 하며 child 이벤트 전파가 들어오더라도 참조를 건드리지 않는다.
 * 마지막 관측값은 유지하고 initialized만0으로 내려 종료 사실을 남긴다. */
static void RootDeleted(lv_event_t *event)
{
    if(lv_event_get_target(event) != sweep.root) return;
    memset(&sweep, 0, sizeof(sweep));
    g_graphics_arc_sweep.initialized = 0U;
}

/* label의 네 모서리가 시험 원과 보드 표시 영역 둘 다 안에 있는지 확인한다.
 * 고정 높이/폭과 한 줄 DOT를 써서 긴 숫자/상태가 원 밖으로 확장되지 않는다. */
static uint32_t LabelVisible(const lv_area_t *area)
{
    int32_t x[2] = {area->x1 - sweep.center_x, area->x2 - sweep.center_x};
    int32_t y[2] = {area->y1 - sweep.center_y, area->y2 - sweep.center_y};
    if(!Graphics_IsAreaVisible(area)) return 0U;
    for(uint32_t i = 0; i < 2U; ++i)
        for(uint32_t j = 0; j < 2U; ++j)
            if(x[i] * x[i] + y[j] * y[j] > sweep.radius * sweep.radius) return 0U;
    return 1U;
}

/* radius에 비례한 상대 중심/폭과 실제4bpp font 높이로 label을 만든다.
 * local styles를 명시하고 theme의 shadow/outline/opacity layer를 상속하지 않는다.
 * NULL은 기하/할당 error를 이미 기록한 상태이며 Init이 전체 자식을 회수한다. */
static lv_obj_t *Label(const char *text, int32_t y_offset_percent,
                       int32_t width_percent, const lv_font_t *font, uint32_t color)
{
    int32_t width = sweep.radius * width_percent / 100;
    int32_t height = (int32_t)font->line_height + 2;
    int32_t x = sweep.center_x - width / 2;
    int32_t y = sweep.center_y + sweep.radius * y_offset_percent / 100 - height / 2;
    lv_area_t area = {x, y, x + width - 1, y + height - 1};
    if(!LabelVisible(&area)) {
        g_graphics_arc_sweep.error = 2U;
        return NULL;
    }
    lv_obj_t *label = lv_label_create(sweep.root);
    if(!label) {
        g_graphics_arc_sweep.error = 3U;
        return NULL;
    }
    lv_obj_remove_style_all(label);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_size(label, width, height);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_opa(label, LV_OPA_COVER, LV_PART_MAIN);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_label_set_text(label, text);
    lv_obj_remove_flag(label, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return label;
}

/* 불필요한 theme style을 모두 제거한270도 arc를 만든다. 135도(좌하단)에서
 * 405도(우하단)까지 시계방향으로 채워 하단90도 gap을 유지한다. 두 arc의
 * round cap을 명시하며 knob의 배경/테두리/그림자는 전부 숨긴다.
 * width는반경200px에서18px, 작은 반경에서는 비례 축소한다. */
static uint32_t CreateArc(void)
{
    int32_t width = sweep.radius < 200 ? sweep.radius * 18 / 200 : 18;
    sweep.arc = lv_arc_create(sweep.root);
    if(!sweep.arc) return 0U;
    lv_obj_remove_style_all(sweep.arc);
    lv_obj_set_pos(sweep.arc, sweep.center_x - sweep.radius, sweep.center_y - sweep.radius);
    lv_obj_set_size(sweep.arc, sweep.radius * 2, sweep.radius * 2);
    lv_obj_set_style_pad_all(sweep.arc, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(sweep.arc, 0, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(sweep.arc, width, LV_PART_MAIN);
    lv_obj_set_style_arc_width(sweep.arc, width, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(sweep.arc, lv_color_hex(0x183440U), LV_PART_MAIN);
    lv_obj_set_style_arc_color(sweep.arc, lv_color_hex(0x45DFD0U), LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(sweep.arc, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(sweep.arc, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(sweep.arc, true, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(sweep.arc, true, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(sweep.arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_border_width(sweep.arc, 0, LV_PART_KNOB);
    lv_obj_set_style_shadow_width(sweep.arc, 0, LV_PART_KNOB);
    lv_obj_set_style_outline_width(sweep.arc, 0, LV_PART_KNOB);
    lv_obj_remove_flag(sweep.arc, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CLICK_FOCUSABLE |
                                  LV_OBJ_FLAG_SCROLLABLE);
    lv_arc_set_mode(sweep.arc, LV_ARC_MODE_NORMAL);
    lv_arc_set_range(sweep.arc, 0, GRAPHICS_ARC_SWEEP_VALUE_MAX);
    lv_arc_set_bg_angles(sweep.arc, 135, 405);
    lv_arc_set_value(sweep.arc, 0);
    return 1U;
}

/* 같은 UI task에서 기존 화면 자식만 회수한다. root DELETE callback에서 참조를
 * 초기화하므로 외부 parent 삭제와 명시적인 Destroy 둘 다 동일한 종료 상태다. */
void GraphicsArcSweep_Destroy(void)
{
    if(sweep.root) lv_obj_delete(sweep.root);
    else g_graphics_arc_sweep.initialized = 0U;
}

/* 표시 계약과 parent 원점을 먼저 확인하고 자식 tree를 생성한다. 자식에는
 * opaque 배경을 만들지 않아 부모의 검은 외곽을 유지한다. lv_obj_update_layout은
 * parent의 좌표 확정만 위한 것이며 refresh/timer/task 처리기를 재진입하지 않는다. */
static uint32_t Init(lv_obj_t *parent, int32_t center_x,
                     int32_t center_y, int32_t outer_radius, uint32_t full_viewport)
{
    lv_area_t content;
    GraphicsArcSweep_Destroy();
    memset((void *)&g_graphics_arc_sweep, 0, sizeof(g_graphics_arc_sweep));
    g_graphics_arc_sweep.magic = GRAPHICS_ARC_SWEEP_DIAGNOSTIC_MAGIC;
    g_graphics_arc_sweep.version = 1U;
    if(!parent || outer_radius < 128 || outer_radius > (full_viewport ? 240 : 239) ||
       center_x < 0 || center_x >= 480 || center_y < 0 || center_y >= 480) {
        g_graphics_arc_sweep.error = 1U;
        return 0U;
    }
    lv_obj_update_layout(parent);
    lv_obj_get_content_coords(parent, &content);
    if(content.x1 != 0 || content.y1 != 0 || content.x2 < 479 || content.y2 < 479) {
        g_graphics_arc_sweep.error = 1U;
        return 0U;
    }
    /* 보통 도형은 원 전체가 들어와야 한다. 화면 테두리 전용 경로만 정확한
     * 480x480 widget을 허용한다. LVGL의 정수 중심240과 활성 원 중심239.5의
     * 반 pixel 차이는 EVE 최종 고정 mask가 자른다. 임의 위치/크기의 원이나
     * label에 같은 예외를 적용하지 않으며 화면 바깥 drawing 영역도 늘리지 않는다. */
    if(full_viewport ? (center_x != 240 || center_y != 240 || outer_radius != 240 ||
                       GRAPHICS_WIDTH != 480U || GRAPHICS_HEIGHT != 480U ||
                       GRAPHICS_ACTIVE_CENTER_X2 != 479 || GRAPHICS_ACTIVE_CENTER_Y2 != 479 ||
                       GRAPHICS_ACTIVE_RADIUS_X2 != 480)
                     : !CircleVisible(center_x, center_y, outer_radius)) {
        g_graphics_arc_sweep.error = 2U;
        return 0U;
    }
    sweep.center_x = center_x;
    sweep.center_y = center_y;
    sweep.radius = outer_radius;
    sweep.last_value = UINT32_MAX;
    sweep.last_speed = UINT32_MAX;
    sweep.last_direction = UINT32_MAX;
    sweep.root = lv_obj_create(parent);
    if(!sweep.root) {
        g_graphics_arc_sweep.error = 3U;
        return 0U;
    }
    lv_obj_add_event_cb(sweep.root, RootDeleted, LV_EVENT_DELETE, NULL);
    lv_obj_remove_style_all(sweep.root);
    lv_obj_set_pos(sweep.root, 0, 0);
    lv_obj_set_size(sweep.root, GRAPHICS_WIDTH, GRAPHICS_HEIGHT);
    lv_obj_remove_flag(sweep.root, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    if(!CreateArc()) {
        g_graphics_arc_sweep.error = 3U;
        goto failed;
    }
    /* 모든 label은 font의 실제 line height로 높이를 고정한다. 화면 자체 확대나
     * opacity animation 없이4bpp glyph만 사용한다. 작은 반경에도 겹치지 않는다. */
#if defined(NOODOE_INTEGRATED) && NOODOE_INTEGRATED
    /* At the higher integrated title position the circle chord is narrower.
     * Keep the complete label rectangle inside the arc's validated radius. */
    if(!Label("DEMO SPEED", -68, 120, &lv_font_montserrat_14, 0x94B6C1U)) goto failed;
    sweep.speed = Label("0", -46, 130, &lv_font_montserrat_40, 0xF1FBFFU);
    if(!sweep.speed) goto failed;
    if(!Label("km/h", -29, 120, &lv_font_montserrat_14, 0x94B6C1U)) goto failed;
    sweep.direction = Label("FILL", -20, 140, &lv_font_montserrat_14, 0x45DFD0U);
    if(!sweep.direction || !GraphicsBringupHUD_Init(sweep.root)) goto failed;
#else
    if(!Label("DEMO SPEED", -49, 140, &lv_font_montserrat_14, 0x94B6C1U)) goto failed;
    sweep.speed = Label("0", -11, 130, &lv_font_montserrat_40, 0xF1FBFFU);
    if(!sweep.speed) goto failed;
    if(!Label("km/h", 17, 120, &lv_font_montserrat_14, 0x94B6C1U)) goto failed;
    sweep.direction = Label("FILL", 38, 140, &lv_font_montserrat_14, 0x45DFD0U);
    if(!sweep.direction) goto failed;
    if(!Label("4s UP / 4s DOWN", 55, 140, &lv_font_montserrat_14, 0x94B6C1U)) goto failed;
#endif
    sweep.performance = Label("FPS -- CPU --", 72, 120, &lv_font_montserrat_14, 0xF1FBFFU);
    if(!sweep.performance) goto failed;
    g_graphics_arc_sweep.initialized = 1U;
    GraphicsArcSweep_Process(0U);
    return 1U;
failed:
    GraphicsArcSweep_Destroy();
    return 0U;
}

/* 기존의 완전 포함 도형 API다. 화면 테두리 전용 경로 때문에 반경/위치 검사를
 * 느슨하게 바꾸지 않는다. 실패 정리와 재초기화 수명은 공통 Init이 소유한다. */
uint32_t GraphicsArcSweep_Init(lv_obj_t *parent, int32_t center_x,
                               int32_t center_y, int32_t outer_radius)
{
    return Init(parent, center_x, center_y, outer_radius, 0U);
}

/* 480px 원형 계기판 테두리 전용 진입점. pad0인480x480 arc의 외곽 반경은240,
 * stroke18은 그 안쪽으로 들어가므로 추가20px 여백을 만들지 않는다. arc와
 * round cap의 최종 raster 경계는 Graphics_EveApplyViewport가 제한한다.
 * 일반 label의 네 모서리 검사는 그대로 유지하여 텍스트가 잘리지 않게 한다. */
uint32_t GraphicsArcSweep_InitViewport(lv_obj_t *parent)
{
    return Init(parent, 240, 240, 240, 1U);
}

/* smoothstep(t)=t*t*(3-2*t). t는0..4000ms이고 출력은0..10000이다.
 * 분자를64bit에서 계산해 곱셈 overflow와 float 비용을 피하며 반올림한다.
 * 양 끝 미분이0이어서 채움/비움 방향 전환 때 속도가 갑자기 바뀌지 않는다. */
static uint32_t EasedValue(uint32_t t)
{
    uint64_t numerator = (uint64_t)t * t * (12000U - 2U * t) * GRAPHICS_ARC_SWEEP_VALUE_MAX;
    return (uint32_t)((numerator + 32000000000ULL) / 64000000000ULL);
}

/* UI 값은 경과 ms로 결정한다. 숫자 문자열과 방향 문자열은 정수 상태가 바뀔 때만
 * 재할당하고 HUD는1초마다 실제 Graphics 측정을 복사한다. 미측정 상태를0fps나
 * 0%로 꾸미지 않는다. 이 모듈에서 Graphics_Process를 재귀 호출하지 않는다. */
void GraphicsArcSweep_Process(uint32_t elapsed_ms)
{
    if(!g_graphics_arc_sweep.initialized) return;
    GraphicsBringupHUD_Process(elapsed_ms);
    uint32_t delta = elapsed_ms - sweep.last_elapsed_ms;
    uint64_t phase = (uint64_t)sweep.phase_ms + delta;
    g_graphics_arc_sweep.cycles += (uint32_t)(phase / GRAPHICS_ARC_SWEEP_PERIOD_MS);
    sweep.phase_ms = (uint32_t)(phase % GRAPHICS_ARC_SWEEP_PERIOD_MS);
    sweep.last_elapsed_ms = elapsed_ms;
    uint32_t direction = sweep.phase_ms >= GRAPHICS_ARC_SWEEP_HALF_PERIOD_MS;
    uint32_t t = direction ? GRAPHICS_ARC_SWEEP_PERIOD_MS - sweep.phase_ms : sweep.phase_ms;
    uint32_t value = EasedValue(t);
    uint32_t speed = (value * GRAPHICS_ARC_SWEEP_SPEED_MAX + 5000U) / GRAPHICS_ARC_SWEEP_VALUE_MAX;
    g_graphics_arc_sweep.phase_ms = sweep.phase_ms;
    g_graphics_arc_sweep.direction = direction;
    g_graphics_arc_sweep.value = value;
    g_graphics_arc_sweep.speed = speed;
    ++g_graphics_arc_sweep.process_count;
    if(value != sweep.last_value) {
        lv_arc_set_value(sweep.arc, (int32_t)value);
        sweep.last_value = value;
        ++g_graphics_arc_sweep.updates;
    }
    if(speed != sweep.last_speed) {
        lv_label_set_text_fmt(sweep.speed, "%lu", (unsigned long)speed);
        sweep.last_speed = speed;
    }
    if(direction != sweep.last_direction) {
        lv_label_set_text(sweep.direction, direction ? "DRAIN" : "FILL");
        sweep.last_direction = direction;
    }
    if(elapsed_ms - sweep.last_hud_ms >= 1000U) {
        const volatile Graphics_Performance *p = Graphics_GetPerformance();
        if(p && p->valid) {
            uint32_t fps = p->fps_tenths, cpu = p->cpu_tenths;
            lv_label_set_text_fmt(sweep.performance, "%lu.%lu FPS CPU %lu.%lu%%",
                                 (unsigned long)(fps / 10U), (unsigned long)(fps % 10U),
                                 (unsigned long)(cpu / 10U), (unsigned long)(cpu % 10U));
        }
        else lv_label_set_text(sweep.performance, "FPS -- CPU --");
        sweep.last_hud_ms = elapsed_ms;
        ++g_graphics_arc_sweep.hud_updates;
    }
}

/* 반환 포인터는 firmware 수명 동안 유효하다. 현재 생성/삭제 상태와 합성 값을
 * 조회하며 LVGL 객체의 내부 구조를 상위 코드에 노출하지 않는다. */
const volatile GraphicsArcSweep_Diagnostics *GraphicsArcSweep_GetDiagnostics(void)
{
    return &g_graphics_arc_sweep;
}
