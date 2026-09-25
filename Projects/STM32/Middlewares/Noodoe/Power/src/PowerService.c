#include "PowerService.h"
#include "BSP_LowPower.h"
#include "FreeRTOS.h"
#include "task.h"
volatile PowerDiagnostics g_power_service={.magic=0x50575331U,.version=1};
static TaskHandle_t owners[3];
__attribute__((weak)) uint32_t PowerService_RunRequired(void){return 0;}
__attribute__((weak)) uint32_t PowerService_DeepAllowed(void){return 1;}
/* Bounded atomic policy/ack publication. All four owners AND physical IGN OFF
 * are necessary for STOP; no owner suspends another task during a bus call. */
void PowerService_Request(uint32_t mode)
{
    if(mode>POWER_DEEP)return;
    taskENTER_CRITICAL();
    if(g_power_service.requested!=mode){g_power_service.requested=mode;g_power_service.acknowledged=0;++g_power_service.changes;}
    uint32_t effective=PowerService_Mode();
    BSP_LowPower_SetPolicy(effective,effective==POWER_DEEP&&g_power_service.acknowledged==15U);
    taskEXIT_CRITICAL();
}
uint32_t PowerService_Mode(void){if(PowerService_RunRequired())return POWER_RUN;uint32_t mode=g_power_service.requested;return mode==POWER_DEEP&&!PowerService_DeepAllowed()?POWER_DISPLAY_SLEEP:mode;}
void PowerService_Acknowledge(uint32_t owner,uint32_t ready)
{
    taskENTER_CRITICAL();
    if(ready)g_power_service.acknowledged|=owner;else g_power_service.acknowledged&=~owner;
    uint32_t effective=PowerService_Mode();
    BSP_LowPower_SetPolicy(effective,effective==POWER_DEEP&&g_power_service.acknowledged==15U);
    taskEXIT_CRITICAL();
}
/* Owner registration followed by a notification wait closes the IGN-before-
 * sleep race: a pending token makes the wait return immediately. */
void PowerService_Wait(uint32_t owner,uint32_t ms)
{
    uint32_t n=owner==POWER_OWNER_IO?0:owner==POWER_OWNER_STORAGE?1:2;
    taskENTER_CRITICAL();owners[n]=xTaskGetCurrentTaskHandle();taskEXIT_CRITICAL();
    (void)ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(ms?ms:1U));
}
/* Publication precedes notification. Unlike a raw edge, this wakes the UI
 * only after the stable input/clock snapshot is available to consume. */
void PowerService_Notify(uint32_t mask)
{
    taskENTER_CRITICAL();
    for(uint32_t i=0;i<3;++i)if((mask&(1U<<i))&&owners[i])xTaskNotifyGive(owners[i]);
    taskEXIT_CRITICAL();
}
/* EXTI15_10 priority5 permits FreeRTOS FromISR calls. No debounce/render/bus
 * work here. Every physical edge cancels an in-flight sleep decision. */
void PowerService_IgnitionIRQ(void)
{
    BaseType_t wake=pdFALSE;++g_power_service.wakes;BSP_LowPower_CancelFromISR();
    for(uint32_t i=0;i<3;++i)if(owners[i])vTaskNotifyGiveFromISR(owners[i],&wake);
    portYIELD_FROM_ISR(wake);
}
