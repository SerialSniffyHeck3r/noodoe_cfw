#include "Product_Boot.h"
#include "ProductUI.h"
#include "Product_TextFonts.h"
#include "Product_NumberFonts.h"
#include "Resources.h"
#include "SystemError.h"
#include "SystemError_View.h"
#include "Graphics.h"
#include "Graphics_Memory.h"
#include "Power_UI.h"
#include "PowerService.h"
#include "BSP_Display.h"
#include "NoodoeRuntime.h"
#include "Health_Service.h"
#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"
#include "bsp_fault.h"
static void AllocationFailed(uint32_t code){SystemError_Report(SYSTEM_ERROR_MEMORY,code);}
/* Both normal boot failure and LVGL assertion recovery use the same bounded
 * read-only retry path. Refresh state after RequestLoad: the prior FAILED
 * snapshot must not immediately cancel the newly queued retry indication. */
static void ErrorRecovery(void)
{
    if(g_system_error.retry_request!=g_system_error.retry_ack){
        if(Resources_RequestLoad()){
            g_system_error.recovery=1;
            g_system_error.retry_ack=g_system_error.retry_request;
        }
    }
    ResourcesState state=Resources_GetStatus();
    if(g_system_error.recovery==1&&state==RESOURCES_READY)g_system_error.recovery=2;
    else if(g_system_error.recovery==1&&state==RESOURCES_FAILED)g_system_error.recovery=0;
}
/* A valid task stack can render a ROM-font failure without returning into
 * the failed LVGL call. All background storage/IGN tasks remain schedulable. */
static void GraphicsFatal(uint32_t line)
{
    SystemError_Report(SYSTEM_ERROR_MEMORY,line);
    PowerService_Request(POWER_RUN);
    uint32_t display=BSP_Display_Init()==BSP_DISPLAY_OK;
    if(display)(void)BSP_Display_SetBrightnessPercent(25);
    for(;;){SystemErrorRecord r;ErrorRecovery();SystemError_GetStatus(&r);
        if(display)(void)SystemErrorView_Draw(r.code,r.detail,r.stage,g_system_error.recovery);
        NoodoeRuntime_GraphicsHeartbeat();(void)osDelay(1000);}
}
/* UI owner boot policy. UART/IGN/storage recovery start before optional UI.
 * A latched failure never retries allocation or enters normal UI automatically. */
void ProductBoot_Run(void)
{
    uint32_t ready=0,started=0,last_error_frame=0,display_ready=0,fault_latched=0;
    SystemError_SetStage(1);
    BSP_FaultSetMemoryObserver(AllocationFailed);Graphics_SetFatalHandler(GraphicsFatal);
    (void)NoodoeRuntime_Start();
    ProductUI_InitModel(HAL_GetTick());
    display_ready=BSP_Display_Init()==BSP_DISPLAY_OK;
    if(!display_ready)SystemError_Report(SYSTEM_ERROR_DISPLAY,1);
    /* Keep retained/test display lists invisible until the UI owner confirms
     * its first real scanout. The independent error view owns its own light. */
    SystemError_SetStage(2);
    for(;;){
        uint32_t now=HAL_GetTick();ResourcesState resources=Resources_GetStatus();
        if(resources==RESOURCES_FAILED&&!SystemError_GetStatus(NULL))
            SystemError_Report(g_resources.error==RESOURCE_RAM?SYSTEM_ERROR_SDRAM:SYSTEM_ERROR_RESOURCE,g_resources.error);
        if(!SystemError_GetStatus(NULL)&&resources==RESOURCES_READY&&!started){
            started=1;SystemError_SetStage(3);
            if(!Product_TextFontsBind()||!Product_NumberFontsBind())SystemError_Report(SYSTEM_ERROR_RESOURCE,RESOURCE_FORMAT);
            else if(Graphics_Init()!=GRAPHICS_OK)SystemError_Report(SYSTEM_ERROR_MEMORY,g_graphics.last_error);
            else if(!ProductUI_Init(now,g_graphics.input_boot_held_mask))SystemError_Report(SYSTEM_ERROR_MEMORY,g_product_ui.error);
            else {ready=1;SystemError_SetStage(4);HealthService_BootReady();}
        }
        if(SystemError_GetStatus(NULL)){
            /* A working error UI with responsive IO/storage is degraded, not
             * a CPU hang. Missing BT/assets must not cause endless resets. */
            HealthService_BootReady();
            /* Retry is explicitly requested through diagnostics. It can only
             * read/verify; a successful repair still requires an explicit reset. */
            ErrorRecovery();
            if(!fault_latched){PowerService_Request(POWER_RUN);BSP_Display_CaptureInvalidate();fault_latched=1;}
            BSP_Display_CaptureProcess();
            if(display_ready&&!BSP_Display_CaptureBusy()&&now-last_error_frame>=1000){SystemErrorRecord r;SystemError_GetStatus(&r);
                if(SystemErrorView_Draw(r.code,r.detail,r.stage,g_system_error.recovery)){
                    last_error_frame=now;(void)BSP_Display_SetBrightnessPercent(25);}}
        }else if(ready){
            ++g_graphics.sample_seq;ProductUI_Process(now);
            Graphics_Status status=GRAPHICS_OK;
            if(PowerUI_GraphicsDue())status=Graphics_Process();
            else Graphics_ServiceCapture(); /* Mailbox/download only; sleeping GPU rejects new snapshots. */
            PowerUI_AfterGraphics(HAL_GetTick());++g_graphics.sample_seq;
            if(status!=GRAPHICS_OK)SystemError_Report(SYSTEM_ERROR_DISPLAY,status);
            if(!GraphicsMemory_Check())SystemError_Report(SYSTEM_ERROR_MEMORY,0xCC01);
        }
        NoodoeRuntime_GraphicsHeartbeat();
        /* Stripe capture is bounded by a2s wall-clock deadline; do not impose
         * the ordinary idle/error10ms delay on each4KiB SPI read step. */
        PowerService_Wait(POWER_OWNER_GRAPHICS,BSP_Display_CaptureBusy()?1U:
            ready&&!SystemError_GetStatus(NULL)?PowerUI_WaitMs():10U);
    }
}
