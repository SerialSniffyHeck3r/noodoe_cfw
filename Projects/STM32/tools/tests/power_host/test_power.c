#include "stm32f4xx_hal.h"
#include "BSP_LowPower.h"
#include "BSP_Watchdog.h"
#include "BSP_Clock.h"
#include "BSP_RAM.h"
#include "PowerService.h"
#include "task.h"
#include "rtc.h"
#include "BSP_Display.h"
#include "bsp_eve.h"
#include "bsp_lcd_panel.h"
#include "ButtonFeedback.h"
#include "BSP_Buttons.h"
#include <stddef.h>
volatile BSP_Clock_Diagnostics g_bsp_clock;
volatile BSP_RAM_Diagnostics g_bsp_ram;
RTC_HandleTypeDef hrtc;
__IO uint32_t uwTick;
volatile uint32_t test_wfi_count,test_emulate_time,test_assertions,test_failure;
static uint32_t stepped,notified,abort_sleep;
static uint32_t eve_busy,panel_error,eve_sleep_calls,brightness_calls;
#define CHECK(x) do {++test_assertions;if(!(x)){test_failure=__LINE__;return __LINE__;}}while(0)
eSleepModeStatus eTaskConfirmSleepModeStatus(void){return abort_sleep?eAbortSleep:eStandardSleep;}
void vTaskStepTick(TickType_t t){stepped+=t;}
TaskHandle_t xTaskGetCurrentTaskHandle(void){return (void*)1;}
uint32_t ulTaskNotifyTake(BaseType_t c,TickType_t t){(void)c;(void)t;return 0;}
void vTaskNotifyGiveFromISR(TaskHandle_t t,BaseType_t *w){if(t)++notified;*w=pdTRUE;}
void xTaskNotifyGive(TaskHandle_t t){if(t)++notified;}
void vPortSuppressTicksAndSleep(TickType_t ticks);
void *memset(void *p,int v,size_t n){unsigned char *d=p;while(n--)*d++=(unsigned char)v;return p;}
uint32_t ButtonEvents_Subscribe(ButtonEventHandler h,void *c){(void)h;(void)c;return 1;}
uint32_t HAL_GetTick(void){return uwTick;}
BSP_Display_Status BSP_Display_SetBrightnessPercent(uint32_t value){(void)value;++brightness_calls;return BSP_DISPLAY_OK;}
uint32_t BSP_EVE_Sleep(void){++eve_sleep_calls;return eve_busy;}
uint32_t BSP_EVE_WakeBegin(void){return 0;}
uint32_t BSP_EVE_WakeFinish(void){return 0;}
HAL_StatusTypeDef BSP_LCD_PanelSleep(void){return panel_error?HAL_ERROR:HAL_OK;}
HAL_StatusTypeDef BSP_LCD_PanelWakeBegin(void){return HAL_OK;}
HAL_StatusTypeDef BSP_LCD_PanelWakeFinish(void){return HAL_OK;}
/* Execute the actual BSP port against emulated MMIO. WFI changes only RTC
 * elapsed time in the runner; this verifies sequencing/guards, not silicon. */
uint32_t Power_Test(void)
{
    hrtc.Instance=RTC;g_bsp_clock.ready=1;g_bsp_ram.ready=1;
    RTC->PRER=(127U<<16)|255U;RTC->SSR=255;RTC->TR=0;RTC->DR=0x242101U;
    RTC->ISR=RTC_ISR_WUTWF;GPIOG->IDR=GPIO_PIN_13;
    IWDG->PR=6;IWDG->RLR=4095;
    RCC->CR=RCC_CR_HSEON|RCC_CR_HSERDY|RCC_CR_PLLON|RCC_CR_PLLRDY;
    RCC->CFGR=RCC_CFGR_SW_PLL|RCC_CFGR_SWS_PLL;
    SysTick->CTRL=7;TIM6->DIER=TIM_DIER_UIE;
    PowerService_Request(POWER_DEEP);CHECK(!g_bsp_lowpower.stop_allowed);
    PowerService_Acknowledge(POWER_OWNER_IO|POWER_OWNER_STORAGE|POWER_OWNER_GRAPHICS,1);
    CHECK(!g_bsp_lowpower.stop_allowed);
    PowerService_Acknowledge(POWER_OWNER_BT,1);CHECK(g_bsp_lowpower.stop_allowed);
    PowerService_Acknowledge(POWER_OWNER_IO,0);CHECK(!g_bsp_lowpower.stop_allowed);
    PowerService_Acknowledge(POWER_OWNER_IO,1);
    GPIOG->IDR=0;vPortSuppressTicksAndSleep(200);CHECK(!test_wfi_count&&g_bsp_lowpower.aborts==1);
    GPIOG->IDR=GPIO_PIN_13;abort_sleep=1;vPortSuppressTicksAndSleep(200);
    CHECK(!test_wfi_count&&g_bsp_lowpower.aborts==2);abort_sleep=0;
    test_emulate_time=1;vPortSuppressTicksAndSleep(200);
    CHECK(test_wfi_count==1&&g_bsp_lowpower.stops==1);
    CHECK(stepped==125&&uwTick==125);
    CHECK(SysTick->CTRL==7&&TIM6->DIER==TIM_DIER_UIE);
    CHECK(!(SCB->SCR&SCB_SCR_SLEEPDEEP_Msk));
    CHECK(!(RTC->CR&(RTC_CR_WUTE|RTC_CR_WUTIE)));
    CHECK(FMC_Bank5_6->SDCMR==(FMC_SDRAM_CMD_NORMAL_MODE|FMC_SDRAM_CMD_TARGET_BANK1));
    CHECK(RTC->DR==0x242101U&&g_bsp_lowpower.error==0);
    DMA1_Stream0->CR=DMA_SxCR_EN;vPortSuppressTicksAndSleep(200);
    CHECK(test_wfi_count==2&&g_bsp_lowpower.stops==1&&stepped==250);
    DMA1_Stream0->CR=0;
    PowerService_Wait(POWER_OWNER_IO,10);PowerService_Wait(POWER_OWNER_STORAGE,10);PowerService_Wait(POWER_OWNER_GRAPHICS,10);
    PowerService_IgnitionIRQ();CHECK(!g_bsp_lowpower.stop_allowed&&notified==3);
    PowerService_Notify(POWER_OWNER_GRAPHICS);CHECK(notified==4);
    PowerService_Request(POWER_RUN);CHECK(!g_power_service.acknowledged);
    test_emulate_time=0;vPortSuppressTicksAndSleep(1000);
    CHECK(test_wfi_count==3&&g_bsp_lowpower.stops==1);
    /* BT retention: active RX DMA is legal in tickless Sleep, never STOP.
     * UART/PLL/FMC configuration and RTC calendar survive unchanged. */
    PowerService_Request(POWER_DISPLAY_SLEEP);CHECK(!g_bsp_lowpower.stop_allowed);
    DMA1_Stream0->CR=DMA_SxCR_EN;RTC->SSR=255;RTC->ISR=RTC_ISR_WUTWF;test_emulate_time=1;
    uint32_t saved_cr=RCC->CR,saved_cfgr=RCC->CFGR,saved_fmc=FMC_Bank5_6->SDCMR;
    vPortSuppressTicksAndSleep(200);
    CHECK(test_wfi_count==4&&g_bsp_lowpower.sleeps==3&&g_bsp_lowpower.stops==1);
    CHECK(stepped==375&&uwTick==375&&g_bsp_lowpower.error==0);
    CHECK(RCC->CR==saved_cr&&RCC->CFGR==saved_cfgr&&FMC_Bank5_6->SDCMR==saved_fmc);
    CHECK(DMA1_Stream0->CR==DMA_SxCR_EN&&!(SCB->SCR&SCB_SCR_SLEEPDEEP_Msk));
    CHECK(SysTick->CTRL==7&&TIM6->DIER==TIM_DIER_UIE);
    DMA1_Stream0->CR=0;test_emulate_time=0;
    /* Busy EVE, failed panel sleep, staged wake and immediate IGN reversal:
     * none may acknowledge sleep early or touch sleeping EVE registers. */
    eve_busy=1;CHECK(BSP_Display_SetSleeping(1)==1&&!BSP_Display_IsSleeping());
    eve_busy=0;panel_error=1;CHECK(BSP_Display_SetSleeping(1)==3&&BSP_Display_IsSleeping());
    uint32_t calls=brightness_calls;panel_error=0;
    CHECK(BSP_Display_SetSleeping(1)==0&&BSP_Display_IsSleeping());
    CHECK(brightness_calls==calls&&eve_sleep_calls==2);
    CHECK(BSP_Display_SetSleeping(0)==1&&BSP_Display_IsSleeping());
    uwTick+=29;CHECK(BSP_Display_SetSleeping(0)==1);
    uwTick+=1;CHECK(BSP_Display_SetSleeping(0)==1);
    uwTick+=119;CHECK(BSP_Display_SetSleeping(0)==1);
    uwTick+=1;CHECK(BSP_Display_SetSleeping(0)==0&&!BSP_Display_IsSleeping());
    CHECK(BSP_Display_SetSleeping(1)==0);
    CHECK(BSP_Display_SetSleeping(0)==1);
    CHECK(BSP_Display_SetSleeping(1)==1);
    uwTick+=30;CHECK(BSP_Display_SetSleeping(1)==1);
    uwTick+=120;CHECK(BSP_Display_SetSleeping(1)==0&&BSP_Display_IsSleeping());
    ButtonFeedback_Init(0);ButtonFeedback_SetContext(4,1,1);
    ButtonEvent press={BSP_BUTTON_ENTER,BSP_BUTTON_EVENT_PRESS,10000,0,0};
    ButtonFeedback_Handle(&press);(void)ButtonFeedback_Visibility(10000);
    CHECK(ButtonFeedback_Visibility(10300)==255);
    ButtonFeedback_SetContext(4,2,0);
    CHECK(!ButtonFeedback_Visibility(10400)&&!g_button_feedback.keys[BSP_BUTTON_ENTER].pressed);
    ButtonFeedback_SetContext(4,3,1);
    CHECK(!ButtonFeedback_Visibility(11000)); /* Old hold cannot pin hints on. */
    ButtonEvent release={BSP_BUTTON_ENTER,BSP_BUTTON_EVENT_RELEASE,11000,1000,0};
    ButtonFeedback_Handle(&release);CHECK(!ButtonFeedback_ActionPhase(BSP_BUTTON_ENTER,4,0,11001));
    press.timestamp_ms=12000;ButtonFeedback_Handle(&press);
    (void)ButtonFeedback_Visibility(12000);CHECK(ButtonFeedback_Visibility(12300)==255);
    release.timestamp_ms=12400;ButtonFeedback_Handle(&release);
    CHECK(ButtonFeedback_ActionPhase(BSP_BUTTON_ENTER,4,0,12401)==1);
    (void)ButtonFeedback_Visibility(18000);CHECK(!ButtonFeedback_Visibility(18300));
    /* Repeated real STOP-port entry/exit with emulated WFI. This checks
     * re-arming and restoration, not silicon retention or current draw. */
    uint32_t previous_stops=g_bsp_lowpower.stops;
    for(uint32_t i=0;i<100;++i){
        PowerService_Request(POWER_DEEP);
        PowerService_Acknowledge(POWER_OWNER_IO|POWER_OWNER_STORAGE|POWER_OWNER_GRAPHICS|POWER_OWNER_BT,1);
        RTC->SSR=255;RTC->ISR=RTC_ISR_WUTWF;test_emulate_time=1;
        vPortSuppressTicksAndSleep(200);
        CHECK(g_bsp_lowpower.stops==previous_stops+i+1);
        CHECK(FMC_Bank5_6->SDCMR==(FMC_SDRAM_CMD_NORMAL_MODE|FMC_SDRAM_CMD_TARGET_BANK1));
        CHECK(!(SCB->SCR&SCB_SCR_SLEEPDEEP_Msk));
        PowerService_IgnitionIRQ();PowerService_Request(POWER_RUN);
        CHECK(!g_bsp_lowpower.stop_allowed);
    }
    /* Active health policy must grant STOP explicitly. The real sleep hook
     * cannot feed or sleep past a stale supervisor token. */
    PowerService_Request(POWER_DEEP);
    PowerService_Acknowledge(15U,1);
    RTC->SSR=255;RTC->ISR=RTC_ISR_WUTWF;test_emulate_time=1;
    g_bsp_watchdog.initialized=1;g_bsp_watchdog.phase=WATCHDOG_RUN;
    g_bsp_watchdog.failure=0;g_bsp_watchdog.sleep_until_ms=uwTick;
    uint32_t slept=g_bsp_lowpower.stops,feeds=g_bsp_watchdog.feeds;
    vPortSuppressTicksAndSleep(200);CHECK(g_bsp_lowpower.stops==slept);
    BSP_Watchdog_GrantSleep(uwTick);vPortSuppressTicksAndSleep(200);
    CHECK(g_bsp_lowpower.stops==slept+1);CHECK(g_bsp_watchdog.feeds==feeds);
    g_bsp_watchdog.failure=1;RTC->SSR=255;RTC->ISR=RTC_ISR_WUTWF;
    vPortSuppressTicksAndSleep(200);CHECK(g_bsp_lowpower.stops==slept+1);
    return 0;
}
