#include "Graphics.h"
#include "bsp_cpu_load.h"
#include "stm32f4xx_hal.h"

volatile Graphics_Performance g_graphics_performance;
static uint32_t last_service_ms, credit, last_cpu_sequence, last_frames;

/* LVGL의 invalidate event가 refresh timer를 resume하더라도 자동 renderer는
 * 실행하지 않는다. 포트의 절대 시간 누산기가 lv_refr_now를 한 번만 호출한다. */
static void DeferredRefresh(lv_timer_t *timer) { lv_timer_pause(timer); }

void Graphics_PerformanceInit(lv_display_t *display)
{
    g_graphics_performance = (Graphics_Performance){0};
    g_graphics_performance.magic = 0x47504631U;
    g_graphics_performance.version = 1U;
    g_graphics_performance.target_fps_milli = 30000U;
    last_service_ms = HAL_GetTick();
    credit = last_cpu_sequence = last_frames = 0U;
    lv_timer_set_cb(lv_display_get_refr_timer(display), DeferredRefresh);
    lv_timer_pause(lv_display_get_refr_timer(display));
    BSP_CPU_LoadInit();
}

/* caller는 Graphics의 소유 task/인자 검사를 먼저 한다. rate의 단위는0.001Hz다. */
void Graphics_PerformanceSetRate(uint32_t rate)
{
    g_graphics_performance.target_fps_milli = rate;
    credit = 0U;
    last_service_ms = HAL_GetTick();
}

/* 1000ms * 1000(rate 단위)를 한 frame으로 누적한다. 늦어진 frame을 몰아서
 * 재생하지 않고 missed 슬롯을 기록한다. 애니메이션은 실제 시간을 사용한다. */
void Graphics_PerformanceRender(lv_display_t *display)
{
    const uint32_t now = HAL_GetTick();
    const uint32_t elapsed = now - last_service_ms;
    last_service_ms = now;
    const uint64_t accrued = (uint64_t)elapsed * g_graphics_performance.target_fps_milli + credit;
    const uint32_t slots = (uint32_t)(accrued / 1000000U);
    credit = (uint32_t)(accrued % 1000000U);
    if (!slots) return;
    g_graphics_performance.frame_slots_missed += slots - 1U;
    if (g_graphics_performance.continuous) lv_obj_invalidate(lv_display_get_screen_active(display));
    lv_refr_now(display);
}

/* CPU 창과 같은 cycle 간격으로 실제 완료 프레임을 나눠 FPS를 구한다.
 * 1초 평균 표시 갱신은 자체가30Hz 문자열 할당 부하를 만들지 않게 한다. */
void Graphics_PerformanceSample(void)
{
    BSP_CPU_LoadSample();
    if (last_cpu_sequence == g_bsp_cpu_load.sequence) return;
    last_cpu_sequence = g_bsp_cpu_load.sequence;
    const uint32_t frames = g_graphics.render_count - last_frames;
    last_frames = g_graphics.render_count;
    g_graphics_performance.valid = g_bsp_cpu_load.ready;
    g_graphics_performance.cpu_tenths = g_bsp_cpu_load.busy_tenths;
    g_graphics_performance.cpu_window_cycles = g_bsp_cpu_load.total_cycles;
    g_graphics_performance.idle_window_cycles = g_bsp_cpu_load.idle_cycles;
    g_graphics_performance.window_ms = g_bsp_cpu_load.total_cycles / (SystemCoreClock / 1000U);
    g_graphics_performance.window_frames = frames;
    g_graphics_performance.fps_tenths = (uint32_t)((uint64_t)frames * SystemCoreClock * 10U / g_bsp_cpu_load.total_cycles);
}

const volatile Graphics_Performance *Graphics_GetPerformance(void) { return &g_graphics_performance; }
