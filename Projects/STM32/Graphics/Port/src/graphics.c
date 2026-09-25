#include "graphics_internal.h"
#include "Graphics_Memory.h"
#include "Graphics_Background.h"
#include "BSP_Display.h"
#if !NOODOE_INTEGRATED
#include "BSP_RAM.h"
#include "dma.h"
#endif
#include "bsp_eve_bus.h"
#include "bsp_fault.h"
#include "bsp_bringup.h"
#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"
#include <stddef.h>
#include <string.h>

_Static_assert(sizeof(Graphics_Diagnostics) == 160U, "Graphics SWD ABI must be 40 words");
volatile Graphics_Diagnostics g_graphics;
/* 문자열은 SWD 장애 분석용이며 UI에 구현 세부 내용을 그리지 않는다. */
volatile uint32_t g_graphics_assert_line;
const char * volatile g_graphics_assert_file;
char g_graphics_last_log[160];
static lv_display_t *graphics_display;
static lv_group_t *graphics_group;
static osThreadId_t graphics_owner;
static uint32_t brightness;
static uint32_t lvgl_started;
static void (*fatal_handler)(uint32_t);
void Graphics_SetFatalHandler(void (*handler)(uint32_t)){fatal_handler=handler;}

/* 프레임 시간/CPU 계측 구현은 widget/lifecycle 코드와 별도 파일로 유지한다. */
void Graphics_PerformanceInit(lv_display_t *display);
void Graphics_PerformanceSetRate(uint32_t rate);
void Graphics_PerformanceRender(lv_display_t *display);
void Graphics_PerformanceSample(void);

/* HAL tick은 TIM6 기반이며 LVGL의 millisecond clock과 같은 단위다.
 * 그래픽 태스크가 밀려도 실제 경과 시간을 돌려주고 ISR에 lv_tick_inc를 추가하지 않는다. */
static uint32_t GraphicsTick(void) { return HAL_GetTick(); }
static void GraphicsDelay(uint32_t ms)
{
    const uint32_t ticks = (ms * osKernelGetTickFreq() + 999U) / 1000U;
    osDelay(ticks ? ticks : 1U);
}

/* 라이브러리 경고를 마지막 문자열과 누적 수로 보존한다. printf/UART/heap을
 * 사용하지 않아 그래픽 오류 로깅 때문에 추가 드라이버 의존성이 생기지 않는다. */
static void GraphicsLog(lv_log_level_t level, const char *message)
{
    if (level >= LV_LOG_LEVEL_WARN) { ++g_graphics.log_warnings; }
    uint32_t index = 0U;
    if (message) {
        while (message[index] && index + 1U < sizeof(g_graphics_last_log)) {
            g_graphics_last_log[index] = message[index];
            ++index;
        }
    }
    g_graphics_last_log[index] = '\0';
}

static Graphics_Status GraphicsCheckOwner(void)
{
    if (__get_IPSR() || __get_PRIMASK() || __get_BASEPRI() || osThreadGetId() != graphics_owner) {
        return GRAPHICS_ERROR_CONTEXT;
    }
    if (!g_graphics.initialized || !graphics_display) { return GRAPHICS_ERROR_STATE; }
    if (g_graphics.last_error) { return (Graphics_Status)g_graphics.last_error; }
    return GRAPHICS_OK;
}

/* Remote inspection of the retained OFF display need not refresh its scene.
 * The mailbox routine performs no SPI transaction when there is no request. */
void Graphics_ServiceCapture(void)
{if(GraphicsCheckOwner()==GRAPHICS_OK)BSP_Display_CaptureProcess();}

/* 최초 runtime 오류를 유지한다. API 인자 오류 같은 caller 실수는 호출부에서
 * 직접 반환하므로 모든 잘못된 인자를 하드웨어 정지로 격상하지 않는다. */
Graphics_Status Graphics_RecordError(Graphics_Status error)
{
    if (error != GRAPHICS_OK && !g_graphics.last_error) {
        g_graphics.last_error = (uint32_t)error;
        g_graphics.stage = GRAPHICS_STAGE_FAULT;
    }
    return error;
}

static void GraphicsSampleMemory(void)
{
    if(!GraphicsMemory_Check())Graphics_RecordError(GRAPHICS_ERROR_MEMORY);
    lv_mem_monitor_t monitor;
    lv_mem_monitor(&monitor);
    g_graphics.heap_total = monitor.total_size;
    g_graphics.heap_free = monitor.free_size;
    g_graphics.heap_largest = monitor.free_biggest_size;
    g_graphics.heap_used = monitor.total_size - monitor.free_size;
    g_graphics.heap_peak = monitor.max_used;
    g_graphics.heap_fragmentation = monitor.frag_pct;
}

Graphics_Status Graphics_Init(void)
{
    if (__get_IPSR() || __get_PRIMASK() || __get_BASEPRI() || osKernelGetState() != osKernelRunning) {
        return GRAPHICS_ERROR_CONTEXT;
    }
    if (lvgl_started) { return GRAPHICS_ERROR_STATE; }
    graphics_owner = osThreadGetId();
    g_graphics = (Graphics_Diagnostics){0};
    g_graphics.magic = GRAPHICS_DIAGNOSTIC_MAGIC;
    g_graphics.version = GRAPHICS_DIAGNOSTIC_VERSION;
    g_graphics.sample_seq = 1U;
    g_graphics.stage = GRAPHICS_STAGE_BSP;
    if (BSP_Display_Init() != BSP_DISPLAY_OK) { return Graphics_RecordError(GRAPHICS_ERROR_DISPLAY); }
#if !NOODOE_INTEGRATED
    /* Standalone Graphics has no StorageTask to initialize SDRAM. Capture
     * still needs its retained RGB565 copy, so initialize before consumers. */
    MX_DMA_Init();
    if(BSP_RAM_Init())return Graphics_RecordError(GRAPHICS_ERROR_MEMORY);
#endif
    g_graphics.stage = GRAPHICS_STAGE_LVGL;
    lv_init();
    lvgl_started = 1U;
    lv_tick_set_cb(GraphicsTick);
    lv_delay_set_cb(GraphicsDelay);
    lv_log_register_print_cb(GraphicsLog);
    g_graphics.stage = GRAPHICS_STAGE_DISPLAY;
    graphics_display = Graphics_EveCreateDisplay();
    if (!graphics_display) { return Graphics_RecordError(GRAPHICS_ERROR_DISPLAY); }
#if !NOODOE_PRODUCT
    graphics_group = lv_group_create();
    if (!graphics_group) { return Graphics_RecordError(GRAPHICS_ERROR_MEMORY); }
    lv_group_set_default(graphics_group);
    lv_group_set_wrap(graphics_group, true);
#endif
    Graphics_InputInit(graphics_display, graphics_group);
#if !NOODOE_PRODUCT
    if (!Graphics_InputGetDevice()) { return Graphics_RecordError(GRAPHICS_ERROR_MEMORY); }
#endif
    /* 기본 theme의 layer/그림자를 제품 요구와 혼동하지 않는다. UI 시험은
     * 각 객체에 지원되는 단순 스타일을 직접 부여하며 초기 theme는 제거한다. */
    lv_display_set_theme(graphics_display, NULL);
    uint32_t initial_light=GRAPHICS_DEFAULT_BRIGHTNESS;
#if NOODOE_PRODUCT
    initial_light=0U;
#endif
    if (BSP_Display_SetBrightnessPercent(initial_light) != BSP_DISPLAY_OK) {
        return Graphics_RecordError(GRAPHICS_ERROR_DISPLAY);
    }
    brightness = initial_light;
    g_graphics.initialized = 1U;
    g_graphics.stage = GRAPHICS_STAGE_READY;
    /* 초기 갱신 주기는 lv_conf.h와 같게 둔다. 상위의 SetRefreshPeriodMs는
     * 이 display timer만 바꾸며, 시험의 합성 값 갱신 주기와는 별개다. */
    lv_timer_set_period(lv_display_get_refr_timer(graphics_display), LV_DEF_REFR_PERIOD);
    Graphics_PerformanceInit(graphics_display);
    GraphicsSampleMemory();
    if (Graphics_EvePoll() != GRAPHICS_OK) { return (Graphics_Status)g_graphics.last_error; }
    ++g_graphics.sample_seq;
    return GRAPHICS_OK;
}

Graphics_Status Graphics_Process(void)
{
    Graphics_Status result = GraphicsCheckOwner();
    if (result != GRAPHICS_OK) { return result; }
    if (g_graphics.last_error) { return (Graphics_Status)g_graphics.last_error; }
    BSP_Display_CaptureProcess();
    if(BSP_Display_CaptureBusy())return GRAPHICS_OK;
    ++g_graphics.sample_seq;
    const uint32_t started = HAL_GetTick();
    Graphics_InputProcess();
    lv_timer_handler();
    Graphics_PerformanceRender(graphics_display);
    Graphics_PerformanceSample();
    if (HAL_GetTick() - g_graphics.hardware_sample_ms >= 250U) {
        GraphicsSampleMemory();
        result = Graphics_EvePoll();
    }
    const uint32_t now = HAL_GetTick();
    ++g_graphics.process_count;
    g_graphics.last_process_ms = now;
    g_graphics.last_process_duration_ms = now - started;
    if (g_graphics.last_process_duration_ms > g_graphics.max_process_duration_ms) {
        g_graphics.max_process_duration_ms = g_graphics.last_process_duration_ms;
    }
    ++g_graphics.sample_seq;
    return result;
}

Graphics_Status Graphics_Shutdown(void)
{
    if (__get_IPSR() || __get_PRIMASK() || __get_BASEPRI() || osThreadGetId() != graphics_owner) { return GRAPHICS_ERROR_CONTEXT; }
    if (!lvgl_started && !g_graphics.initialized) { return GRAPHICS_OK; }
    Graphics_InputDeinit();
    if (lvgl_started) {
        Graphics_EveReleaseCache();
        lv_deinit();
        lvgl_started = 0U;
    }
    graphics_display = NULL;
    graphics_group = NULL;
    BSP_Display_Shutdown();
    brightness = 0U;
    g_graphics.initialized = 0U;
    g_graphics.stage = GRAPHICS_STAGE_OFF;
    return GRAPHICS_OK;
}

uint32_t Graphics_IsReady(void) { return g_graphics.initialized && !g_graphics.last_error; }
uint32_t Graphics_GetCapabilities(void)
{
    return GRAPHICS_CAP_TEXT_4BPP | GRAPHICS_CAP_PRIMITIVES | GRAPHICS_CAP_ARC |
        GRAPHICS_CAP_IMAGE_RGB565 | GRAPHICS_CAP_IMAGE_ALPHA4 | GRAPHICS_CAP_IMAGE_TRANSFORM |
        GRAPHICS_CAP_BUTTON_FOCUS | GRAPHICS_CAP_FLEX_GRID | GRAPHICS_CAP_ANIMATION;
}
lv_display_t *Graphics_GetDisplay(void) { return graphics_display; }
lv_indev_t *Graphics_GetInput(void) { return g_graphics.initialized ? Graphics_InputGetDevice() : NULL; }
lv_group_t *Graphics_GetFocusGroup(void) { return graphics_group; }
lv_obj_t *Graphics_GetScreen(void) { return graphics_display ? lv_display_get_screen_active(graphics_display) : NULL; }

Graphics_Status Graphics_LoadScreen(lv_obj_t *screen)
{
    Graphics_Status result = GraphicsCheckOwner();
    if (result != GRAPHICS_OK) { return result; }
    if (!screen || lv_obj_get_parent(screen) || lv_obj_get_display(screen) != graphics_display) {
        return GRAPHICS_ERROR_ARGUMENT;
    }
    lv_screen_load(screen);
    return GRAPHICS_OK;
}
Graphics_Status Graphics_Invalidate(void)
{
    Graphics_Status result = GraphicsCheckOwner();
    if (result != GRAPHICS_OK) { return result; }
    lv_obj_invalidate(Graphics_GetScreen());
    return GRAPHICS_OK;
}
Graphics_Status Graphics_SetBrightnessPercent(uint32_t percent)
{
    Graphics_Status result = GraphicsCheckOwner();
    if (result != GRAPHICS_OK) { return result; }
    if (percent > 100U) { return GRAPHICS_ERROR_ARGUMENT; }
    if (BSP_Display_SetBrightnessPercent(percent) != BSP_DISPLAY_OK) { return Graphics_RecordError(GRAPHICS_ERROR_DISPLAY); }
    brightness = percent;
    return GRAPHICS_OK;
}
uint32_t Graphics_GetBrightnessPercent(void) { return brightness; }
/* Preserve LVGL objects, GPU addresses and current model through hardware
 * sleep. Only the display owner calls this bounded staged BSP facade. */
uint32_t Graphics_SetDisplaySleeping(uint32_t sleeping)
{
    if(GraphicsCheckOwner()!=GRAPHICS_OK)return 10;
    uint32_t result=BSP_Display_SetSleeping(sleeping);
    if(sleeping)brightness=0;
    return result;
}
uint32_t Graphics_DisplaySleeping(void){return BSP_Display_IsSleeping();}
Graphics_Status Graphics_SetRefreshPeriodMs(uint32_t milliseconds)
{
    Graphics_Status result = GraphicsCheckOwner();
    if (result != GRAPHICS_OK) { return result; }
    if (milliseconds < 5U || milliseconds > 1000U) { return GRAPHICS_ERROR_ARGUMENT; }
    Graphics_PerformanceSetRate(1000000U / milliseconds);
    return GRAPHICS_OK;
}
Graphics_Status Graphics_SetTargetFPS(uint32_t fps)
{
    Graphics_Status result = GraphicsCheckOwner();
    if (result != GRAPHICS_OK) return result;
    if (!fps || fps > 60U) return GRAPHICS_ERROR_ARGUMENT;
    Graphics_PerformanceSetRate(fps * 1000U);
    return GRAPHICS_OK;
}
Graphics_Status Graphics_SetContinuousRendering(uint32_t enabled)
{
    Graphics_Status result = GraphicsCheckOwner();
    if (result == GRAPHICS_OK) g_graphics_performance.continuous = enabled != 0U;
    return result;
}
Graphics_Status Graphics_SetInputEnabled(uint32_t enabled)
{
    Graphics_Status result = GraphicsCheckOwner();
    if (result == GRAPHICS_OK) { Graphics_InputSetEnabled(enabled); }
    return result;
}
Graphics_Status Graphics_FocusNext(void)
{
    Graphics_Status result = GraphicsCheckOwner();
    if (result == GRAPHICS_OK) { lv_group_focus_next(graphics_group); }
    return result;
}
Graphics_Status Graphics_FocusPrevious(void)
{
    Graphics_Status result = GraphicsCheckOwner();
    if (result == GRAPHICS_OK) { lv_group_focus_prev(graphics_group); }
    return result;
}
Graphics_Status Graphics_SetEditing(uint32_t editing)
{
    Graphics_Status result = GraphicsCheckOwner();
    if (result == GRAPHICS_OK) { lv_group_set_editing(graphics_group, editing != 0U); }
    return result;
}
Graphics_Status Graphics_PreloadImage(const lv_image_dsc_t *image)
{
    Graphics_Status result = GraphicsCheckOwner();
    return result == GRAPHICS_OK ? Graphics_EvePreloadImage(image) : result;
}
Graphics_Status Graphics_PreloadText(const lv_font_t *font, const char *text)
{
    Graphics_Status result = GraphicsCheckOwner();
    return result == GRAPHICS_OK ? Graphics_EvePreloadText(font, text) : result;
}
Graphics_Status Graphics_ClearAssetCache(void)
{
    Graphics_Status result = GraphicsCheckOwner();
    if (result == GRAPHICS_OK) { result = Graphics_EveResetCache(); }
    return result == GRAPHICS_OK ? Graphics_Invalidate() : result;
}
uint32_t Graphics_GetAssetBytesUsed(void) { return g_graphics.ramg_used; }
uint32_t Graphics_GetAssetBytesFree(void)
{
#if NOODOE_PRODUCT
    const uint32_t limit=BACKGROUND_CACHE_END;
#else
    const uint32_t limit=BSP_DISPLAY_CAPTURE_RAM_G;
#endif
    return g_graphics.ramg_used<limit?limit-g_graphics.ramg_used:0U;
}
const volatile Graphics_Diagnostics *Graphics_GetDiagnostics(void) { return &g_graphics; }
Graphics_Status Graphics_CopyDiagnostics(Graphics_Diagnostics *destination)
{
    Graphics_Status result = GraphicsCheckOwner();
    if (result != GRAPHICS_OK) { return result; }
    if (!destination) { return GRAPHICS_ERROR_ARGUMENT; }
    *destination = g_graphics;
    return GRAPHICS_OK;
}
Graphics_Status Graphics_SetButtonCallback(Graphics_ButtonCallback callback, void *context)
{
    Graphics_Status result = GraphicsCheckOwner();
    if (result == GRAPHICS_OK) { Graphics_InputSetCallback(callback, context); }
    return result;
}

void Graphics_AssertFail(const char *file, uint32_t line)
{
    g_graphics_assert_file = file;
    g_graphics_assert_line = line;
    Graphics_RecordError(GRAPHICS_ERROR_STATE);
    BSP_EVE_BusDeselect();
    if(fatal_handler&&!__get_IPSR()&&!__get_PRIMASK()&&!__get_BASEPRI()&&osThreadGetId()==graphics_owner){
        BSP_FaultSnapshot(0x510U);g_graphics.render_busy=0U;fatal_handler(line);
    }
    BSP_Display_Shutdown();
    BSP_FaultRecord(0x510U);
    g_graphics.render_busy = 0U;
    if (g_graphics.sample_seq & 1U) { ++g_graphics.sample_seq; }
    for (;;) {
        if (!__get_IPSR() && !__get_PRIMASK() && osKernelGetState() == osKernelRunning) {
            osDelay(100U);
            BSP_BringupSample();
        }
    }
}
