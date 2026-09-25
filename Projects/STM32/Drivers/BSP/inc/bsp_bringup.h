#ifndef BSP_BRINGUP_H
#define BSP_BRINGUP_H

#include <stdint.h>

/* SWD에서 화면/UART 없이 읽는, 이번 보드 생존 시험의 상태 값이다. */
typedef struct {
  uint32_t magic;
  uint32_t stage;
  uint32_t heartbeat;
  uint32_t hal_tick;
  uint32_t kernel_tick;
  uint32_t tim6_interrupts;
  uint32_t core_clock;
  uint32_t vtor;
  uint32_t control;
  uint32_t basepri;
  uint32_t primask;
  uint32_t free_heap;
  uint32_t min_free_heap;
  uint32_t stack_free_words;
} BSP_BringupStatus;

extern volatile BSP_BringupStatus g_bsp_bringup;
void BSP_BringupMark(uint32_t stage);
void BSP_BringupHalTick(void);
void BSP_BringupSample(void);
void BSP_BringupTask(void *argument);

#endif
