#include "bsp_cpu_load.h"
#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"

volatile BSP_CPU_Load g_bsp_cpu_load;
static void *idle_task;
static volatile uint32_t enabled, last_cycle, idle_total, was_idle;
static uint32_t sample_cycle, sample_idle;

/* modulo32 차이는 wrap을 지나도 각 switch 사이 실제 cycle 수를 보존한다.
 * 높은 우선순위 ISR에서 이 hook/상태를 호출하지 않는다. */
void BSP_CPU_TraceSwitch(void *task)
{
    if (!enabled) return;
    const uint32_t now = DWT->CYCCNT;
    if (was_idle) idle_total += now - last_cycle;
    last_cycle = now;
    was_idle = task == idle_task;
}

/* trace가 이미 움직이는 scheduler의 비유휴 task에서 호출한다. CYCCNT를 reset하지
 * 않아 다른 진단의 epoch를 깨지 않고 counter enable bit만 추가한다. */
void BSP_CPU_LoadInit(void)
{
    taskENTER_CRITICAL();
    enabled = 0U;
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    idle_task = xTaskGetIdleTaskHandle();
    idle_total = sample_idle = was_idle = 0U;
    sample_cycle = last_cycle = DWT->CYCCNT;
    g_bsp_cpu_load = (BSP_CPU_Load){0};
    enabled = 1U;
    taskEXIT_CRITICAL();
}

/* 1초 이상 경과했을 때만 새 값을 발행한다. task-switch 누계 복사만 critical로
 * 보호하고 64bit 비율 연산은 IRQ가 열린 상태에서 한다. debugger halt 때 DWT도
 * 정지하므로 CPU를 멈춘 시간을 idle로 꾸며 넣지 않는다. */
void BSP_CPU_LoadSample(void)
{
    if (!enabled) return;
    uint32_t now, idle;
    taskENTER_CRITICAL();
    now = DWT->CYCCNT;
    idle = idle_total + (was_idle ? now - last_cycle : 0U);
    taskEXIT_CRITICAL();
    const uint32_t total = now - sample_cycle;
    if (total < SystemCoreClock) return;
    const uint32_t idle_delta = idle - sample_idle;
    sample_cycle = now;
    sample_idle = idle;
    g_bsp_cpu_load.total_cycles = total;
    g_bsp_cpu_load.idle_cycles = idle_delta;
    /* counter 정합성이 깨진 표본은 0%로 위장하지 않고 ready=0으로 표시한다. */
    g_bsp_cpu_load.ready = idle_delta <= total;
    g_bsp_cpu_load.busy_tenths = idle_delta <= total ?
        1000U - (uint32_t)((uint64_t)idle_delta * 1000U / total) : 0U;
    ++g_bsp_cpu_load.sequence;
}
