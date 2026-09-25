#include "SystemError.h"
#include "FreeRTOS.h"
#include "task.h"
volatile SystemErrorDiagnostics g_system_error;
void SystemError_Report(uint32_t code,uint32_t detail)
{
    if(!code)return;
    taskENTER_CRITICAL();
    g_system_error.magic=0x45525231;g_system_error.version=1;
    SystemErrorRecord r={code,detail,g_system_error.stage};
    if(!g_system_error.active)g_system_error.first=r;
    g_system_error.last=r;g_system_error.history[g_system_error.count%8]=r;
    ++g_system_error.count;g_system_error.active=1;
    taskEXIT_CRITICAL();
}
uint32_t SystemError_GetStatus(SystemErrorRecord *out)
{taskENTER_CRITICAL();if(out)*out=g_system_error.first;uint32_t active=g_system_error.active;taskEXIT_CRITICAL();return active;}
void SystemError_SetStage(uint32_t stage){g_system_error.stage=stage;}
