#include "diagnostic.h"
#include "gate_abi.h"
#include "bsp_fault.h"
#include "BSP_Watchdog.h"
#include "FreeRTOS.h"
#include "stm32f4xx.h"
#include <string.h>
const uint32_t diagnostic_contract[4] __attribute__((section(".product_contract"),used))={0x3250554e,2,3,0x60000};
const uint32_t diagnostic_resources[11] __attribute__((section(".resource_requirement"),used))={0x51534352,1,0};
volatile GateMailbox g_recovery_mailbox __attribute__((section(".gate_mailbox"),aligned(8)));
uint8_t ucHeap[configTOTAL_HEAP_SIZE] __attribute__((aligned(8)));
void BSP_BootSafetyStart(void){(void)BSP_Watchdog_StartEarly();}
/* Magic-last publication cannot fabricate an approved restore on a torn reset. */
void Diagnostic_ResetToGate(uint32_t reason)
{
 GateRetained r;memcpy(&r,(const void*)&g_recovery_mailbox.request,sizeof(r));
 GateRetained_Request(&r,reason);g_recovery_mailbox.request.magic=0;__DMB();
 memcpy((uint8_t*)&g_recovery_mailbox.request+4,(const uint8_t*)&r+4,sizeof(r)-4);
 __DMB();g_recovery_mailbox.request.magic=r.magic;__DSB();NVIC_SystemReset();
}
uint32_t AppRecovery_IsFaultBoot(void)
{GateFault f;memcpy(&f,(const void*)&g_recovery_mailbox.fault,sizeof(f));return GateFault_Valid(&f);}
/* A temporary image is never an ordinary Product trial. Old Gate or a direct
 * jump into this binary leads to the independent recovery screen. */
void AppRecovery_EarlyRun(void)
{
 GateRetained r;GateBootContext b;
 memcpy(&r,(const void*)&g_recovery_mailbox.request,sizeof(r));
 memcpy(&b,(const void*)&g_recovery_mailbox.boot,sizeof(b));
 if(!GateRetained_Valid(&r)||!GateBootContext_Valid(&b)||b.sequence!=r.sequence||b.flags!=GATE_F_DIAGNOSTIC)
  Diagnostic_ResetToGate(GATE_REASON_WAIT);
}
void BSP_FaultRecoveryRequested(uint32_t code)
{
 GateFault f={0};f.boot=g_recovery_mailbox.request.sequence;f.code=code;
 const volatile uint32_t *p=&g_bsp_fault.ipsr;
 for(uint32_t i=0;i<8;i++)f.registers[i]=p[i];
 GateFault_Seal(&f);memcpy((void*)&g_recovery_mailbox.fault,&f,sizeof(f));__DSB();
 Diagnostic_ResetToGate(GATE_REASON_FAULT);
}
