#ifndef NOODOE_GRAPHICS_H
#define NOODOE_GRAPHICS_H

#include <stdint.h>
#include "lvgl.h"
#include "Graphics_Viewport.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Graphics-owner only, nonblocking: a newer submitted DL has reached scanout.
 * Submission/FIFO completion alone is not evidence of a visible frame. */
uint32_t Graphics_FramePresentedAfter(uint32_t frame);
/* Owner-only diagnostic mailbox servicing, without LVGL invalidation/render.
 * Call only while the EVE is awake; sleeping hardware is never woken here. */
void Graphics_ServiceCapture(void);
/* A fatal LVGL assertion cannot return into invalid object code. The Product
 * owner may enter its allocation-free error loop; IRQ/stack faults bypass it. */
void Graphics_SetFatalHandler(void (*handler)(uint32_t));

/* LVGL 원본 API를 그대로 공개한다. 이 API는 lifecycle/장치/진단/입력 연결을
 * 보충하며 모든 widget마다 중복 wrapper를 만드는 제한된 GUI 언어가 아니다.
 * Init을 호출한 단일 RTOS 태스크가 모든 lv_* 및 변경 API를 소유한다.
 * ISR/다른 태스크는 메시지를 소유 태스크로 전달한 뒤 그 태스크에서 UI를 변경한다. */
/* 고정 활성 원: AreaVisible은 사각형 전체 포함, Intersects는 최소 한 pixel
 * 포함이다. y와 사각 좌표는 양 끝을 포함하며 모든 UI가 같은 기준을 쓴다. */
uint32_t Graphics_IsPointVisible(int32_t x, int32_t y);
uint32_t Graphics_IsAreaVisible(const lv_area_t *area);
uint32_t Graphics_AreaIntersectsVisible(const lv_area_t *area);
uint32_t Graphics_IsCircleVisible(int32_t x, int32_t y, int32_t radius);
uint32_t Graphics_GetSafeArea(int32_t y_top, int32_t y_bottom, lv_area_t *area);
#define GRAPHICS_DIAGNOSTIC_MAGIC 0x47524131UL /* GRA1 */
#define GRAPHICS_DIAGNOSTIC_VERSION 1U
#define GRAPHICS_DEFAULT_BRIGHTNESS 25U

typedef enum {
    GRAPHICS_OK = 0,
    GRAPHICS_ERROR_ARGUMENT = 1,
    GRAPHICS_ERROR_CONTEXT = 2,
    GRAPHICS_ERROR_STATE = 3,
    GRAPHICS_ERROR_DISPLAY = 4,
    GRAPHICS_ERROR_MEMORY = 5,
    GRAPHICS_ERROR_IO = 6,
    GRAPHICS_ERROR_GPU = 7,
    GRAPHICS_ERROR_UNSUPPORTED = 8
} Graphics_Status;

typedef enum {
    GRAPHICS_STAGE_OFF = 0,
    GRAPHICS_STAGE_BSP = 1,
    GRAPHICS_STAGE_LVGL = 2,
    GRAPHICS_STAGE_DISPLAY = 3,
    GRAPHICS_STAGE_READY = 4,
    GRAPHICS_STAGE_FAULT = 0x80000000U
} Graphics_Stage;

enum {
    GRAPHICS_CAP_TEXT_4BPP = 1U << 0,
    GRAPHICS_CAP_PRIMITIVES = 1U << 1,
    GRAPHICS_CAP_ARC = 1U << 2,
    GRAPHICS_CAP_IMAGE_RGB565 = 1U << 3,
    GRAPHICS_CAP_IMAGE_ALPHA4 = 1U << 4,
    GRAPHICS_CAP_IMAGE_TRANSFORM = 1U << 5,
    GRAPHICS_CAP_BUTTON_FOCUS = 1U << 6,
    GRAPHICS_CAP_FLEX_GRID = 1U << 7,
    GRAPHICS_CAP_ANIMATION = 1U << 8,
    GRAPHICS_CAP_LAYER = 1U << 9,
    GRAPHICS_CAP_STYLE_GRADIENT = 1U << 10,
    GRAPHICS_CAP_FILE_IMAGE = 1U << 11
};

/* 정확히 40개 uint32_t word이다. sample_seq 홀수는 갱신 도중, 짝수는 완료.
 * SWD는 같은 짝수 sequence의 snapshot을 사용해야 하며 마지막 GPU 표본은
 * hardware_sample_ms 시각의 값이다. 렌더 중 FIFO read/write 차이는 오류가 아니다.
 * frame_count는 LVGL refresh 완료, eve_frames는 EVE scanout 횟수로 서로 다르다. */
typedef struct {
    uint32_t magic, version, sample_seq, stage, last_error, initialized;
    uint32_t process_count, last_process_ms, last_process_duration_ms, max_process_duration_ms;
    uint32_t render_count, render_busy, render_started_ms, last_render_ms;
    uint32_t last_render_duration_ms, max_render_duration_ms;
    uint32_t heap_total, heap_free, heap_largest, heap_used, heap_peak, heap_fragmentation;
    uint32_t eve_frames, eve_cmd_read, eve_cmd_write, eve_cmd_dl, eve_cpu_reset, eve_pclk;
    uint32_t hardware_sample_ms, ramg_used, ramg_entries, cache_resets;
    uint32_t input_pressed_mask, input_boot_held_mask, input_events, input_dropped;
    uint32_t spi_tx_bytes, spi_rx_bytes, spi_failures, log_warnings;
} Graphics_Diagnostics;

extern volatile Graphics_Diagnostics g_graphics;

/* 1초 실제 제출 FPS와 RTOS 비유휴 시간 추정. FPS는 EVE scanout Hz가 아니다.
 * cpu_tenths는0..1000이며 valid=0이면 아직 표본이 없다. SWD ABI:12 words. */
typedef struct {
    uint32_t magic, version, valid, fps_tenths, cpu_tenths, target_fps_milli;
    uint32_t continuous, window_ms, window_frames, frame_slots_missed;
    uint32_t cpu_window_cycles, idle_window_cycles;
} Graphics_Performance;
extern volatile Graphics_Performance g_graphics_performance;
const volatile Graphics_Performance *Graphics_GetPerformance(void);
/* target1..60Hz. 분수 주기를 누적해30Hz를33ms 고정 주기로 근사하지 않는다.
 * continuous=1은 정적인 화면도 목표 주기에 다시 그리는 성능 시험 모드다. */
Graphics_Status Graphics_SetTargetFPS(uint32_t fps);
Graphics_Status Graphics_SetContinuousRendering(uint32_t enabled);

/* HAL tick/RTOS 초기화 후 한 번 호출. 기존 BSP의 패널/광원/EVE 전원 순서를
 * 사용하고 검증된 EVE에 renderer를 인계한다. 실패는 상태값+진단에 남긴다.
 * LVGL 자체는 비휘발 메모리에 쓰지 않는다. 성공 후 밝기는25%다. */
Graphics_Status Graphics_Init(void);
/* 소유 태스크에서 약5ms마다 호출. 버튼서비스, LVGL timer/animation, 주기
 * 건강진단을 처리한다. 반환 뒤 위젯 및 시험 API를 호출할 수 있다. */
Graphics_Status Graphics_Process(void);
/* 소유 태스크에서 LVGL object/input/display를 해제하고 패널·광원을 종료한다. */
Graphics_Status Graphics_Shutdown(void);
uint32_t Graphics_IsReady(void);
uint32_t Graphics_GetCapabilities(void);

/* 반환 object는 Graphics 소유. caller가 display/input/group 자체를 삭제하면
 * 안 된다. 화면 아래의 위젯은 일반 LVGL 수명 규칙을 따르고 하드웨어를 몰라도 된다. */
lv_display_t *Graphics_GetDisplay(void);
lv_indev_t *Graphics_GetInput(void);
lv_group_t *Graphics_GetFocusGroup(void);
lv_obj_t *Graphics_GetScreen(void);
Graphics_Status Graphics_LoadScreen(lv_obj_t *screen);
Graphics_Status Graphics_Invalidate(void);
Graphics_Status Graphics_SetBrightnessPercent(uint32_t percent);
uint32_t Graphics_GetBrightnessPercent(void);
uint32_t Graphics_SetDisplaySleeping(uint32_t sleeping);
uint32_t Graphics_DisplaySleeping(void);
Graphics_Status Graphics_SetRefreshPeriodMs(uint32_t milliseconds);
Graphics_Status Graphics_SetInputEnabled(uint32_t enabled);
Graphics_Status Graphics_FocusNext(void);
Graphics_Status Graphics_FocusPrevious(void);
Graphics_Status Graphics_SetEditing(uint32_t editing);

/* 이미지와폰트는renderer지원형식만 허용한다. 원본데이터는GPU캐시정리/재업로드
 * 시까지유효해야하며같은주소의내용을바꾼뒤에는ClearAssetCache가필요하다.
 * Clear는완료된프레임사이GPUFIFO가비었을때만전체캐시를비우며화면을다시그린다.
 * 캐시정리는모든이미지/글리프에적용되며LRU/파일JPEGdecoder를제공한다고하지않는다. */
Graphics_Status Graphics_PreloadImage(const lv_image_dsc_t *image);
Graphics_Status Graphics_PreloadText(const lv_font_t *font, const char *text);
Graphics_Status Graphics_ClearAssetCache(void);
uint32_t Graphics_GetAssetBytesUsed(void);
uint32_t Graphics_GetAssetBytesFree(void);
const volatile Graphics_Diagnostics *Graphics_GetDiagnostics(void);
/* snapshot은소유태스크에서사용한다. ISR에서동시에고정된사진을요청하지않는다. */
Graphics_Status Graphics_CopyDiagnostics(Graphics_Diagnostics *destination);

/* 입력 callback은Graphics_Process문맥에서만호출된다. enum값은BSP의public
 * UP0/DOWN1/ENTER2와PRESS1/RELEASE2/SHORT3/LONG4/VERY_LONG5를유지한다.
 * callback은구조체를보관하지말고값을복사하며Graphics_Process재진입금지. */
typedef void (*Graphics_ButtonCallback)(uint32_t button, uint32_t event,
                                        uint32_t duration_ms, void *context);
Graphics_Status Graphics_SetButtonCallback(Graphics_ButtonCallback callback, void *context);

/* assert는로그/진단을고정하고BSP fault를기록한뒤RTOS를양보한다. 잘못된렌더를
 * 계속실행하지않으며자동reset/flash쓰기없이SWD진단이가능하도록남는다. */
void Graphics_AssertFail(const char *file, uint32_t line) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif
#endif
