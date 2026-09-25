#ifndef BSP_FAULT_H
#define BSP_FAULT_H
#include <stdint.h>

/* CPU fault와 RTOS/HAL 오류를 디버거가 같은 형식으로 조회한다. */
typedef struct {
  uint32_t magic;
  uint32_t code;
  uint32_t ipsr;
  uint32_t cfsr;
  uint32_t hfsr;
  uint32_t dfsr;
  uint32_t mmfar;
  uint32_t bfar;
  uint32_t msp;
  uint32_t psp;
} BSP_FaultStatus;
extern volatile BSP_FaultStatus g_bsp_fault;
void BSP_FaultClear(void);
void BSP_FaultSnapshot(uint32_t code);
void BSP_FaultSetMemoryObserver(void (*observer)(uint32_t));
void BSP_FaultNotifyMemory(void);
__attribute__((noreturn)) void BSP_FaultRecord(uint32_t code);
#endif
