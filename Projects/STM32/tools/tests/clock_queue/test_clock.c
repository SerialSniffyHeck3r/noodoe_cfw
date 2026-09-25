#include "ClockService.h"
#include "BSP_Calendar.h"
#include <string.h>
volatile uint32_t g_assertions,g_failure_line;
#define CHECK(x) do{++g_assertions;if(!(x)){g_failure_line=__LINE__;return 1;}}while(0)
static uint32_t now,sets,set_error,read_error,drift;static BSP_Clock_Time stored;
uint32_t HAL_GetTick(void){return now;}
uint32_t BSP_Clock_Set(const BSP_Clock_Time *t){++sets;if(set_error)return set_error;stored=*t;return 0;}
uint32_t BSP_Clock_Read(BSP_Clock_Time *t)
{
    if(read_error)return read_error;
    *t=stored;t->valid=1;
    if(drift){BSP_CalendarDateTime a={t->year,t->month,t->day,0,t->hour,t->minute,t->second},b;BSP_Calendar_AddSeconds(&a,drift,&b);
        *t=(BSP_Clock_Time){b.year,b.month,b.day,b.weekday,b.hour,b.minute,b.second,1};}
    return 0;
}
uint32_t Clock_TestMain(void)
{
    BSP_Clock_Time value={2024,2,29,0,23,59,59,1};uint32_t id,r,other;
    CHECK(ClockService_Request(0,&id)==BSP_CLOCK_ARGUMENT);
    CHECK(ClockService_Request(&value,&id)==0&&id);CHECK(!sets);
    CHECK(ClockService_Request(&value,&other)==BSP_CLOCK_BUSY);CHECK(!ClockService_Result(id,&r));
    now=10;ClockService_Process();CHECK(ClockService_Result(id,&r)&&!r&&sets==1);
    ClockService_Process();CHECK(sets==1);
    drift=1;CHECK(!ClockService_Request(&value,&id));ClockService_Process();CHECK(ClockService_Result(id,&r)&&!r&&sets==2);
    drift=2;CHECK(!ClockService_Request(&value,&id));ClockService_Process();CHECK(ClockService_Result(id,&r)&&r==BSP_CLOCK_IO);drift=0;
    set_error=BSP_CLOCK_TIMEOUT;CHECK(!ClockService_Request(&value,&id));ClockService_Process();CHECK(ClockService_Result(id,&r)&&r==BSP_CLOCK_TIMEOUT);set_error=0;
    read_error=BSP_CLOCK_IO;CHECK(!ClockService_Request(&value,&id));ClockService_Process();CHECK(ClockService_Result(id,&r)&&r==BSP_CLOCK_IO);read_error=0;
    uint32_t before=sets;CHECK(!ClockService_Request(&value,&id));now+=4000;ClockService_Process();CHECK(ClockService_Result(id,&r)&&r==BSP_CLOCK_TIMEOUT&&sets==before);
    now=0xFFFFF800;CHECK(!ClockService_Request(&value,&id));now+=3999;ClockService_Process();CHECK(ClockService_Result(id,&r)&&!r&&sets==before+1);
    return 0;
}

