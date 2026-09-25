#include "BSP_LowPower.h"
#include "BSP_Clock.h"
#include "BSP_RAM.h"
#include "BSP_Watchdog.h"
#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "rtc.h"

volatile BSP_LowPowerDiagnostics g_bsp_lowpower={.magic=0x4C505731U,.version=2};
static uint32_t milliseconds_fraction,ticks_fraction;
extern __IO uint32_t uwTick;
/* The port runs only with the scheduler suspended. No HAL routine that waits
 * on uwTick, heap allocation, flash write or external-RAM access is permitted. */
void BSP_LowPower_SetPolicy(uint32_t policy,uint32_t ready)
{g_bsp_lowpower.policy=policy;g_bsp_lowpower.stop_allowed=ready;}
void BSP_LowPower_CancelFromISR(void)
{++g_bsp_lowpower.ign_edges;g_bsp_lowpower.stop_allowed=0;}

/* Raw RTC shadow sample, 256 units/second with the preserved127/255 dividers.
 * DR unlocks TR/SSR; a second SSR detects rollover. Bounds make broken RTC
 * hardware a visible refusal to sleep rather than an unbounded wait. */
static uint32_t RTCStamp(uint32_t *out)
{
    if(!g_bsp_clock.ready||(RTC->PRER&0x7FFFU)!=255U||((RTC->PRER>>16)&127U)!=127U)return 0;
    for(uint32_t i=0;i<4;++i){uint32_t sub=RTC->SSR,tr=RTC->TR,dr=RTC->DR,after=RTC->SSR;(void)dr;
        /* The rollover check reads SSR again and locks a NEW shadow sample.
         * Unlock that sample too, otherwise the post-STOP read sees the
         * pre-sleep time and loses the entire stopped interval. */
        (void)RTC->DR;
        if(after>sub)continue;
        uint32_t seconds=(tr&15U)+10U*((tr>>4)&7U);
        uint32_t minutes=((tr>>8)&15U)+10U*((tr>>12)&7U);
        uint32_t hours=((tr>>16)&15U)+10U*((tr>>20)&3U);
        if(seconds>59U||minutes>59U||hours>23U||sub>255U)return 0;
        *out=((hours*60U+minutes)*60U+seconds)*256U+255U-sub;return 1;
    }return 0;
}
/* CPU-cycle timeout remains live while interrupts and HAL tick are stopped.
 * HSI after STOP is slower, so loops use a conservative fixed upper bound. */
static uint32_t WaitBits(volatile uint32_t *reg,uint32_t mask,uint32_t value)
{
    for(uint32_t n=0;n<1000000U;++n)if((*reg&mask)==value)return 1;
    return 0;
}
static void RTCUnlock(void){RTC->WPR=0xCAU;RTC->WPR=0x53U;}
/* Own only WUT/EXTI22. Calendar, backup words and Alarm A/B are preserved. */
static uint32_t ArmRTC(uint32_t ms)
{
    RTCUnlock();RTC->CR&=~(RTC_CR_WUTE|RTC_CR_WUTIE);
    if(!WaitBits(&RTC->ISR,RTC_ISR_WUTWF,RTC_ISR_WUTWF)){RTC->WPR=0xFFU;return 0;}
    __HAL_RTC_WAKEUPTIMER_CLEAR_FLAG(&hrtc,RTC_FLAG_WUTF);
    EXTI->PR=EXTI_PR_PR22;NVIC_ClearPendingIRQ(RTC_WKUP_IRQn);
    RTC->WUTR=(ms*2048U/1000U)-1U;
    RTC->CR=(RTC->CR&~RTC_CR_WUCKSEL)|RTC_WAKEUPCLOCK_RTCCLK_DIV16|RTC_CR_WUTIE|RTC_CR_WUTE;
    EXTI->RTSR|=EXTI_RTSR_TR22;EXTI->FTSR&=~EXTI_FTSR_TR22;EXTI->IMR|=EXTI_IMR_MR22;
    NVIC_SetPriority(RTC_WKUP_IRQn,5U);NVIC_EnableIRQ(RTC_WKUP_IRQn);RTC->WPR=0xFFU;return 1;
}
static void DisarmRTC(void)
{
    RTCUnlock();RTC->CR&=~(RTC_CR_WUTE|RTC_CR_WUTIE);
    __HAL_RTC_WAKEUPTIMER_CLEAR_FLAG(&hrtc,RTC_FLAG_WUTF);RTC->WPR=0xFFU;
    EXTI->PR=EXTI_PR_PR22;NVIC_ClearPendingIRQ(RTC_WKUP_IRQn);
}
void HAL_RTCEx_WakeUpTimerEventCallback(RTC_HandleTypeDef *r)
{if(r&&r->Instance==RTC)++g_bsp_lowpower.rtc_wakes;}
/* A fixed channel inventory, not HAL handle state, proves no DMA still owns
 * RAM. A stuck transfer inhibits STOP; it is never forcibly declared complete. */
static uint32_t DMABusy(void)
{
    DMA_Stream_TypeDef *const streams[]={DMA1_Stream0,DMA1_Stream1,DMA1_Stream2,DMA1_Stream3,DMA1_Stream4,DMA1_Stream5,DMA1_Stream6,DMA1_Stream7,
        DMA2_Stream0,DMA2_Stream1,DMA2_Stream2,DMA2_Stream3,DMA2_Stream4,DMA2_Stream5,DMA2_Stream6,DMA2_Stream7};
    for(uint32_t i=0;i<16;++i)if(streams[i]->CR&DMA_SxCR_EN)return 1;
    return 0;
}
/* Half of the shortest plausible configured IWDG period (conservative60kHz
 * LSI upper bound), capped500ms. The sleep port never starts or feeds IWDG. */
static uint32_t SleepLimit(void)
{
    uint32_t pr=IWDG->PR&7U;if(pr>6U)pr=6U;
    uint32_t ms=((4U<<pr)*((IWDG->RLR&4095U)+1U))/120U;
    return ms>500U?500U:ms;
}
/* FMC bank1 retains its initialized geometry and arena. No SDRAM content is
 * read in self-refresh; every owner has acknowledged idle before this call. */
static uint32_t SDRAMCommand(uint32_t command)
{
    if(!g_bsp_ram.ready)return 1;
    if(!WaitBits(&FMC_Bank5_6->SDSR,FMC_SDSR_BUSY,0))return 0;
    FMC_Bank5_6->SDCMR=command|FMC_SDRAM_CMD_TARGET_BANK1;
    return WaitBits(&FMC_Bank5_6->SDSR,FMC_SDSR_BUSY,0);
}
/* STOP returns on HSI. Restore the same HSE/PLL sources and SYSCLK BEFORE
 * leaving SDRAM self-refresh or allowing UART/timer ISRs. A failed oscillator
 * restore is fail-stop with diagnostic error, not a false healthy resume. */
static void RestoreClock(uint32_t cr,uint32_t cfgr)
{
    if(cr&RCC_CR_HSEON){RCC->CR|=RCC_CR_HSEON;if(!WaitBits(&RCC->CR,RCC_CR_HSERDY,RCC_CR_HSERDY))goto failed;}
    if(cr&RCC_CR_PLLON){RCC->CR|=RCC_CR_PLLON;if(!WaitBits(&RCC->CR,RCC_CR_PLLRDY,RCC_CR_PLLRDY))goto failed;}
    RCC->CFGR=(RCC->CFGR&~RCC_CFGR_SW)|(cfgr&RCC_CFGR_SW);
    if(!WaitBits(&RCC->CFGR,RCC_CFGR_SWS,(cfgr&RCC_CFGR_SW)<<2U))goto failed;
    return;
failed:g_bsp_lowpower.error=6;g_bsp_lowpower.phase=99;
    BSP_Watchdog_Fail(0x306U);
    for(;;){__NOP();} /* IWDG returns to the independent gate. */
}
/* Strong implementation replaces only Cube's weak user hook. ON uses ordinary
 * idle WFI. OFF suppresses ticks using LSE; only acknowledged deep policy uses
 * STOP/low-power regulator. RTOS compensation cannot cross its next deadline. */
void vPortSuppressTicksAndSleep(TickType_t expected)
{
    /* Retained-link sleep suppresses the millisecond ticks but leaves all
     * peripheral clocks and DMA running: H4 reception wakes WFI normally.
     * Only the separately acknowledged deep policy can gate HSE/PLL. */
    if(g_bsp_lowpower.policy<2U||expected<16U){
        SCB->SCR&=~SCB_SCR_SLEEPDEEP_Msk;
        ++g_bsp_lowpower.idle_wfi;g_bsp_lowpower.last_mode=1;
        __DSB();__WFI();__ISB();return;
    }
    uint32_t mask=__get_PRIMASK();__disable_irq();
    uint32_t before=0,after=0;
    if(eTaskConfirmSleepModeStatus()==eAbortSleep||!(GPIOG->IDR&GPIO_PIN_13)||
       (SCB->ICSR&SCB_ICSR_PENDSTSET_Msk)||!RTCStamp(&before))goto abort;
    uint32_t ms=expected>configTICK_RATE_HZ?1000U:(uint32_t)expected*1000U/configTICK_RATE_HZ;
    uint32_t limit=SleepLimit();if(ms>limit)ms=limit;
    if(!BSP_Watchdog_CanSleep(uwTick,ms))goto abort;
    if(ms<16U)goto abort;
    ms-=8U; /* RTC subsecond quantization + setup margin: wake before deadline. */
    uint32_t systick=SysTick->CTRL,tim6=TIM6->DIER;
    SysTick->CTRL=0;TIM6->DIER&=~TIM_DIER_UIE;
    if(!ArmRTC(ms)){g_bsp_lowpower.error=1;DisarmRTC();SysTick->CTRL=systick;TIM6->DIER=tim6;goto abort;}
    uint32_t cr=RCC->CR,cfgr=RCC->CFGR,pwr=PWR->CR;
    uint32_t deep=g_bsp_lowpower.policy==3U&&g_bsp_lowpower.stop_allowed&&!DMABusy()&&(GPIOG->IDR&GPIO_PIN_13);
    if(deep&&!SDRAMCommand(FMC_SDRAM_CMD_SELFREFRESH_MODE)){
        g_bsp_lowpower.error=2;deep=0;
        /* A timed-out command may already have reached the SDRAM. Prove
         * NORMAL again before allowing any task to touch the LVGL arena. */
        if(!SDRAMCommand(FMC_SDRAM_CMD_NORMAL_MODE)){
            g_bsp_lowpower.phase=99;BSP_Watchdog_Fail(0x302U);for(;;){__NOP();}
        }
    }
    g_bsp_lowpower.phase=deep?2U:1U;
    if(deep){PWR->CR=(pwr&~PWR_CR_PDDS)|PWR_CR_LPDS|PWR_CR_FPDS;SCB->SCR|=SCB_SCR_SLEEPDEEP_Msk;}
    else SCB->SCR&=~SCB_SCR_SLEEPDEEP_Msk;
    g_bsp_lowpower.last_mode=deep?3U:2U;
    g_bsp_lowpower.entry_rcc_cr=RCC->CR;g_bsp_lowpower.entry_pwr_cr=PWR->CR;
    g_bsp_lowpower.entry_scr=SCB->SCR;
    __DSB();__WFI();__ISB();
    SCB->SCR&=~SCB_SCR_SLEEPDEEP_Msk;
    if(deep){RestoreClock(cr,cfgr);PWR->CR=pwr;
        if(!SDRAMCommand(FMC_SDRAM_CMD_NORMAL_MODE)){g_bsp_lowpower.error=3;BSP_Watchdog_Fail(0x303U);for(;;){__NOP();}}
        ++g_bsp_lowpower.stops;
    }
    ++g_bsp_lowpower.sleeps;
    g_bsp_lowpower.wake_pending=EXTI->PR;
    if(RTC->ISR&RTC_ISR_WUTF)++g_bsp_lowpower.rtc_wakes;
    if(!RTCStamp(&after)){g_bsp_lowpower.error=4;after=before;}
    DisarmRTC();
    uint32_t units=after>=before?after-before:after+86400U*256U-before;
    if(units>512U){g_bsp_lowpower.error=5;units=0;}
    uint32_t scaled=units*1000U+milliseconds_fraction;
    uint32_t elapsed=scaled/256U;milliseconds_fraction=scaled%256U;
    scaled=units*configTICK_RATE_HZ+ticks_fraction;
    TickType_t ticks=scaled/256U;ticks_fraction=scaled%256U;
    if(ticks>=expected){ticks=expected-1U;g_bsp_lowpower.error=7;}
    uwTick+=elapsed;if(ticks)vTaskStepTick(ticks);
    g_bsp_lowpower.last_sleep_ms=elapsed;g_bsp_lowpower.elapsed_ms+=elapsed;
    if(elapsed>g_bsp_lowpower.max_sleep_ms)g_bsp_lowpower.max_sleep_ms=elapsed;
    TIM6->SR&=~TIM_SR_UIF;TIM6->CNT=0;TIM6->DIER=tim6;
    SysTick->VAL=0;SysTick->CTRL=systick;
    g_bsp_lowpower.phase=0;__set_PRIMASK(mask);return;
abort:++g_bsp_lowpower.aborts;__set_PRIMASK(mask);
}
