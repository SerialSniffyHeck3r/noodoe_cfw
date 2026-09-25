#include "ClockService.h"
#include "BSP_Calendar.h"
#include "FreeRTOS.h"
#include "task.h"
#include "stm32f4xx_hal.h"
static struct {BSP_Clock_Time value;uint32_t next,pending,done,result,queued_ms;} clock_request;
/* Copy under a short critical section. A busy owner cannot be overwritten. */
uint32_t ClockService_Request(const BSP_Clock_Time *value,uint32_t *id)
{
    if(!value||!id)return BSP_CLOCK_ARGUMENT;
    taskENTER_CRITICAL();
    if(clock_request.pending){taskEXIT_CRITICAL();return BSP_CLOCK_BUSY;}
    if(!++clock_request.next)++clock_request.next;
    clock_request.queued_ms=HAL_GetTick();clock_request.value=*value;*id=clock_request.pending=clock_request.next;
    taskEXIT_CRITICAL();return BSP_CLOCK_OK;
}
uint32_t ClockService_Result(uint32_t id,uint32_t *result)
{
    if(!id||!result)return 0;
    taskENTER_CRITICAL();uint32_t done=clock_request.done==id;
    if(done)*result=clock_request.result;
    taskEXIT_CRITICAL();return done;
}
/* The RTC read and write share the existing storage worker. Readback accepts
 * the requested second or its immediate successor across midnight. A failed
 * second HAL transaction may leave a partial write; never report it applied. */
void ClockService_Process(void)
{
    taskENTER_CRITICAL();uint32_t id=clock_request.pending;BSP_Clock_Time value=clock_request.value;taskEXIT_CRITICAL();
    if(!id)return;
    /* Expired queued writes cannot execute after the UI has reported timeout. */
    uint32_t result=HAL_GetTick()-clock_request.queued_ms>=4000U?BSP_CLOCK_TIMEOUT:BSP_Clock_Set(&value);
    if(!result){
        BSP_Clock_Time read={0};result=BSP_Clock_Read(&read);
        BSP_CalendarDateTime t={value.year,value.month,value.day,0,value.hour,value.minute,value.second},next;
        if(!result){
            uint32_t same=read.valid&&read.year==t.year&&read.month==t.month&&read.day==t.day&&read.hour==t.hour&&read.minute==t.minute&&read.second==t.second;
            if(!same&&BSP_Calendar_AddSeconds(&t,1,&next))same=read.valid&&read.year==next.year&&read.month==next.month&&read.day==next.day&&read.hour==next.hour&&read.minute==next.minute&&read.second==next.second;
            if(!same)result=BSP_CLOCK_IO;
        }
    }
    taskENTER_CRITICAL();clock_request.result=result;clock_request.done=id;clock_request.pending=0;taskEXIT_CRITICAL();
}
