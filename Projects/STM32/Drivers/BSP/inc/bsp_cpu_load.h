#ifndef BSP_CPU_LOAD_H
#define BSP_CPU_LOAD_H
#include <stdint.h>

/* DWT cycle 기준 1초 창. tenths는 0..1000 = 0.0..100.0%이며 ready 전에는
 * 유효한 사용률이 아니다. IRQ 시간은 당시 실행 task에 귀속되는 RTOS 추정치다. */
typedef struct {
    uint32_t sequence, ready, busy_tenths, total_cycles, idle_cycles;
} BSP_CPU_Load;
extern volatile BSP_CPU_Load g_bsp_cpu_load;

/* scheduler 실행 후 task에서 초기화/주기 호출한다. WFI/sleep/CPU clock 변경 없이
 * 현재168MHz DWT를 사용하며 sample 간격은 CYCCNT 한 바퀴(약25.6초)보다 짧아야 한다. */
void BSP_CPU_LoadInit(void);
void BSP_CPU_LoadSample(void);
/* FreeRTOS trace hook 전용. scheduler 문맥에서 호출하며 HAL/RTOS API/할당을 쓰지 않는다. */
void BSP_CPU_TraceSwitch(void *task);
#endif
