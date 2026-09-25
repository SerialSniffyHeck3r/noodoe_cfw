#include "bsp_bringup.h"
#include "bsp_fault.h"
#include "BSP_Watchdog.h"
#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "task.h"

/* C startup에서 0으로 초기화되는 상태다. boot 진입 기록과 일부러 분리한다. */
volatile BSP_BringupStatus g_bsp_bringup;

/* main의 각 USER CODE 지점에서 마지막으로 통과한 초기화 단계를 표시한다. */
void BSP_BringupMark(uint32_t stage)
{
  g_bsp_bringup.magic = 0x42525550U; /* BRUP */
  g_bsp_bringup.stage = stage;
  g_bsp_bringup.core_clock = SystemCoreClock;
  g_bsp_bringup.vtor = SCB->VTOR;
}

/* HAL의 실제 TIM6 IRQ에서 호출한다. 태스크 카운터와 독립적인 진척 증거다. */
void BSP_BringupHalTick(void)
{
  g_bsp_bringup.tim6_interrupts++;
}

/*
 * 실행 중인 현재 태스크에서만 호출하는 1회 진단 수집이다. 지연/반복은 호출자가
 * 소유하므로 LCDTest도 기존 생존 시험과 같은 SWD 관측 형식을 사용할 수 있다.
 * 동적 할당 없이 HAL/RTOS tick, IRQ mask, heap 잔량과 현재 stack 최저 잔량을
 * 갱신한다. heartbeat는 이 함수의 실행 횟수이며 화면 프레임 수와는 다르다.
 */
void BSP_BringupSample(void)
{
  g_bsp_bringup.heartbeat++;
  g_bsp_bringup.hal_tick = HAL_GetTick();
  g_bsp_bringup.kernel_tick = osKernelGetTickCount();
  g_bsp_bringup.control = __get_CONTROL();
  g_bsp_bringup.basepri = __get_BASEPRI();
  g_bsp_bringup.primask = __get_PRIMASK();
  g_bsp_bringup.free_heap = (uint32_t)xPortGetFreeHeapSize();
  g_bsp_bringup.min_free_heap = (uint32_t)xPortGetMinimumEverFreeHeapSize();
  g_bsp_bringup.stack_free_words = (uint32_t)uxTaskGetStackHighWaterMark(NULL);
}

/*
 * 이번 bring-up 전용 태스크. App_Logic에 기능을 넣지 않고 BSP 시험만 수행한다.
 * osDelay 복귀 후 갱신하므로 heartbeat 증가는 scheduler/tick/복귀 경로의 증거다.
 * 전용 시험의 watchdog을 시작하고 완료된 반복만 WAIT checkpoint로 보고한다.
 */
void BSP_BringupTask(void *argument)
{
  (void)argument;
  BSP_BringupMark(6U);
  (void)BSP_Watchdog_StartEarly();
  (void)BSP_Watchdog_Init(HAL_GetTick(),WATCHDOG_WAIT,0);
  for (;;) {
    if (osDelay(100U) != osOK) {
      BSP_FaultRecord(0x100U);
    }
    BSP_BringupSample();
    (void)BSP_Watchdog_Checkpoint(HAL_GetTick(),g_bsp_bringup.heartbeat);
  }
}
