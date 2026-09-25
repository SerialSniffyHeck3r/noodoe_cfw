#include "bsp_fault.h"
#include "BSP_Watchdog.h"
#include "stm32f4xx.h"

/* 초기 fault도 남길 수 있도록 C의 .bss 초기화와 독립적인 영역에 둔다. */
volatile BSP_FaultStatus g_bsp_fault __attribute__((section(".noinit.bsp_fault"), aligned(8)));
static void (*memory_observer)(uint32_t);
/* Product/Bootstrap can request a reset into a minimal waiting screen. The
 * default bring-up policy still preserves evidence and waits for SWD. */
__attribute__((weak)) void BSP_FaultRecoveryRequested(uint32_t code){(void)code;}
void BSP_FaultSetMemoryObserver(void (*observer)(uint32_t)){memory_observer=observer;}

/* main에 정상 도달한 새 시험에서 이전 실행의 오류 표시를 해제한다. */
void BSP_FaultClear(void)
{
  g_bsp_fault.magic = 0U;
  g_bsp_fault.code = 0U;
}

/*
 * 오류 뒤 정상 루프로 복귀하지 않는다. .noinit 상태를 남기고 복구 훅을 호출한다.
 * 치명적 오류 루프에서는 watchdog을 갱신하지 않아 독립 gate로 돌아가게 한다.
 * 이 레코드의 MSP/PSP는 함수 내부 관측값이며 예외 진입 당시 stacked PC가 아니다.
 */
void BSP_FaultSnapshot(uint32_t code)
{
  uint32_t irq=__get_PRIMASK();
  __disable_irq();
  g_bsp_fault.code = code;
  g_bsp_fault.ipsr = __get_IPSR();
  g_bsp_fault.cfsr = SCB->CFSR;
  g_bsp_fault.hfsr = SCB->HFSR;
  g_bsp_fault.dfsr = SCB->DFSR;
  g_bsp_fault.mmfar = SCB->MMFAR;
  g_bsp_fault.bfar = SCB->BFAR;
  g_bsp_fault.msp = __get_MSP();
  g_bsp_fault.psp = __get_PSP();
  __DSB();
  g_bsp_fault.magic = 0x4641554CU; /* FAUL: 모든 필드가 기록된 뒤 공개한다. */
  __set_PRIMASK(irq);
}
/* Recoverable allocation failure in ordinary task context may publish a
 * static system error. CPU/stack faults retain the original terminal policy. */
void BSP_FaultNotifyMemory(void)
{
  if(memory_observer&&!__get_IPSR()&&!__get_PRIMASK()&&!__get_BASEPRI()){
    BSP_FaultSnapshot(0x302U);memory_observer(0x302U);return;
  }
  BSP_FaultRecord(0x302U);
}
__attribute__((noreturn)) void BSP_FaultRecord(uint32_t code)
{
  __disable_irq();BSP_FaultSnapshot(code);
  BSP_Watchdog_Fail(code);
  BSP_FaultRecoveryRequested(code);
  for (;;) {
    __NOP();
  }
}
