#include "BSP_Clock.h"
#include "BSP_Calendar.h"
#include "rtc.h"

volatile BSP_Clock_Diagnostics g_bsp_clock;
static volatile uint32_t api_busy;

/* HAL setters can wait on the tick. Reject IRQ/masked/unprivileged calls;
 * serialize complete Time/Date pairs without masking IRQs across HAL waits. */
static uint32_t Acquire(void)
{
    if(__get_IPSR() || (__get_CONTROL()&1U) || __get_PRIMASK() ||
       __get_BASEPRI() || __get_FAULTMASK())return BSP_CLOCK_CONTEXT;
    uint32_t mask=__get_PRIMASK();__disable_irq();
    if(api_busy){__set_PRIMASK(mask);return BSP_CLOCK_BUSY;}
    api_busy=1U;__DMB();__set_PRIMASK(mask);return BSP_CLOCK_OK;
}
/* Preserve the diagnostic ABI prefix. Rejected callers never release another
 * operation's ownership. error is the last completed/rejected API result. */
static uint32_t Result(uint32_t result)
{return g_bsp_clock.error=result;}
static uint32_t Release(uint32_t result)
{
    Result(result);uint32_t mask=__get_PRIMASK();__disable_irq();
    __DMB();api_busy=0U;__set_PRIMASK(mask);return result;
}
static uint32_t HalResult(HAL_StatusTypeDef status)
{
    if(status==HAL_OK)return BSP_CLOCK_OK;
    if(status==HAL_BUSY)return BSP_CLOCK_BUSY;
    if(status==HAL_TIMEOUT)return BSP_CLOCK_TIMEOUT;
    return BSP_CLOCK_IO;
}
/* RTC encodes only2000..2099. Calendar arithmetic itself is Gregorian1..9999.
 * Weekday0 means omitted; other1..7 inputs are accepted but always derived. */
static uint32_t CalendarValid(const BSP_Clock_Time *time)
{
    return time&&time->year>=2000U&&time->year<=2099U&&time->weekday<=7U&&
        time->hour<=23U&&time->minute<=59U&&time->second<=59U&&
        BSP_Calendar_Weekday(time->year,time->month,time->day)!=0U;
}
/* GetDate must run even if GetTime failed: it unlocks shadow registers. Only
 * a complete successful pair replaces the sample; failed reads clear validity
 * without publishing partially filled HAL structs. Caller owns api_busy. */
static uint32_t ReadLocked(BSP_Clock_Time *time)
{
    RTC_TimeTypeDef t={0};RTC_DateTypeDef d={0};
    HAL_StatusTypeDef ts=HAL_RTC_GetTime(&hrtc,&t,RTC_FORMAT_BIN);
    HAL_StatusTypeDef ds=HAL_RTC_GetDate(&hrtc,&d,RTC_FORMAT_BIN);
    if(ts!=HAL_OK || ds!=HAL_OK){
        time->valid=0U;g_bsp_clock.time.valid=0U;
        return HalResult(ts!=HAL_OK?ts:ds);
    }
    BSP_Clock_Time next={2000U+d.Year,d.Month,d.Date,d.WeekDay,t.Hours,t.Minutes,t.Seconds,0U};
    /* Ignore an inherited incorrect weekday register; derive the published day
     * from Y/M/D without repairing/writing RTC merely because it was read. */
    next.weekday=BSP_Calendar_Weekday(next.year,next.month,next.day);
    next.valid=(RTC->ISR&RTC_ISR_INITS)!=0U && CalendarValid(&next);
    *time=next;g_bsp_clock.time=next;++g_bsp_clock.reads;return BSP_CLOCK_OK;
}
/* Attach to a retained LSE RTC, or select LSE when RTCSEL is still unassigned
 * after complete power loss. No backup reset, calendar/backup/alarm writes or
 * generated MX_RTC_Init (which sets a dummy date and alarm) are used. */
uint32_t BSP_Clock_Init(void)
{
    uint32_t status=Acquire();if(status)return Result(status);
    g_bsp_clock.magic=0x52544331U;g_bsp_clock.ready=0U;g_bsp_clock.time.valid=0U;
    __HAL_RCC_PWR_CLK_ENABLE();HAL_PWR_EnableBkUpAccess();
    if(!(RCC->BDCR&RCC_BDCR_LSERDY))return Release(BSP_CLOCK_NOT_READY);
    uint32_t source=RCC->BDCR&RCC_BDCR_RTCSEL;
    if(source&&source!=RCC_BDCR_RTCSEL_0)return Release(BSP_CLOCK_NOT_READY);
    if(!source)SET_BIT(RCC->BDCR,RCC_BDCR_RTCSEL_0);
    __HAL_RCC_RTC_ENABLE();
    if(RTC->CR&RTC_CR_FMT)return Release(BSP_CLOCK_NOT_READY);
    hrtc.Instance=RTC;hrtc.Lock=HAL_UNLOCKED;hrtc.State=HAL_RTC_STATE_READY;
    hrtc.Init.HourFormat=RTC_HOURFORMAT_24;
    hrtc.Init.AsynchPrediv=(RTC->PRER>>16U)&0x7FU;
    hrtc.Init.SynchPrediv=RTC->PRER&0x7FFFU;
    if(!(RTC->ISR&RTC_ISR_INITS)){
        /* Start only the prescaler/counter on a virgin RTC. Year stays zero,
         * so INITS/calendar-valid stays false until explicit date/time setting.
         * STOP timing needs a running counter, not a fabricated wall clock. */
        hrtc.Init.AsynchPrediv=127U;hrtc.Init.SynchPrediv=255U;
        hrtc.Init.OutPut=RTC_OUTPUT_DISABLE;hrtc.Init.OutPutPolarity=RTC_OUTPUT_POLARITY_HIGH;
        hrtc.Init.OutPutType=RTC_OUTPUT_TYPE_OPENDRAIN;
        status=HalResult(HAL_RTC_Init(&hrtc));if(status)return Release(status);
    }
    if(!(RTC->CR&RTC_CR_BYPSHAD)){
        status=HalResult(HAL_RTC_WaitForSynchro(&hrtc));if(status)return Release(status);
    }
    g_bsp_clock.backup19=HAL_RTCEx_BKUPRead(&hrtc,RTC_BKP_DR19);
    BSP_Clock_Time now;status=ReadLocked(&now);
    if(!status)g_bsp_clock.ready=1U;
    return Release(status);
}
uint32_t BSP_Clock_Read(BSP_Clock_Time *time)
{
    if(!time)return Result(BSP_CLOCK_ARGUMENT);
    uint32_t status=Acquire();
    if(status){time->valid=0U;return Result(status);}
    if(!g_bsp_clock.ready){time->valid=0U;g_bsp_clock.time.valid=0U;return Release(BSP_CLOCK_NOT_READY);}
    return Release(ReadLocked(time));
}
/* Guard spans both HAL writes and real readback. Hardware has two separate
 * INIT transactions: failure after SetDate can leave that date applied. Keep
 * the error and invalidate the sample instead of attempting another rollback. */
uint32_t BSP_Clock_Set(const BSP_Clock_Time *time)
{
    if(!CalendarValid(time))return Result(BSP_CLOCK_ARGUMENT);
    uint32_t status=Acquire();if(status)return Result(status);
    if(!g_bsp_clock.ready)return Release(BSP_CLOCK_NOT_READY);
    RTC_TimeTypeDef t={0};RTC_DateTypeDef d={0};
    t.Hours=time->hour;t.Minutes=time->minute;t.Seconds=time->second;
    d.Year=time->year-2000U;d.Month=time->month;d.Date=time->day;d.WeekDay=BSP_Calendar_Weekday(time->year,time->month,time->day);
    HAL_StatusTypeDef hal=HAL_RTC_SetDate(&hrtc,&d,RTC_FORMAT_BIN);
    if(hal==HAL_OK)hal=HAL_RTC_SetTime(&hrtc,&t,RTC_FORMAT_BIN);
    if(hal!=HAL_OK){g_bsp_clock.time.valid=0U;return Release(HalResult(hal));}
    ++g_bsp_clock.sets;BSP_Clock_Time actual;return Release(ReadLocked(&actual));
}
/* A/B share EXTI17 and NVIC. Clear A but preserve a pending enabled B. If B
 * arrives between checking and W1C clearing the shared line, its peripheral
 * flag remains set: re-pend NVIC. This bounded critical section never waits. */
static uint32_t AlarmBPending(void)
{return (RTC->ISR&RTC_ISR_ALRBF)!=0U && (RTC->CR&RTC_CR_ALRBIE)!=0U;}
static void ClearAlarmAPending(void)
{
    uint32_t mask=__get_PRIMASK();__disable_irq();
    __HAL_RTC_ALARM_CLEAR_FLAG(&hrtc,RTC_FLAG_ALRAF);
    if(!AlarmBPending()){
        __HAL_RTC_ALARM_EXTI_CLEAR_FLAG();
        HAL_NVIC_ClearPendingIRQ(RTC_Alarm_IRQn);
    }
    if(AlarmBPending())HAL_NVIC_SetPendingIRQ(RTC_Alarm_IRQn);
    __DSB();__set_PRIMASK(mask);
}
static void RestoreAlarmIRQ(uint32_t enabled)
{if(enabled)HAL_NVIC_EnableIRQ(RTC_Alarm_IRQn);else HAL_NVIC_DisableIRQ(RTC_Alarm_IRQn);}
/* Disable NVIC before touching inherited A. HAL errors restore its entry
 * enable state; only successful installation enables the new alarm IRQ.
 * Alarm B registers, flags, and an existing B vector priority are preserved. */
uint32_t BSP_Clock_SetDailyAlarm(uint32_t hour,uint32_t minute,uint32_t second)
{
    if(hour>23U || minute>59U || second>59U)return Result(BSP_CLOCK_ARGUMENT);
    uint32_t status=Acquire();if(status)return Result(status);
    if(!g_bsp_clock.ready)return Release(BSP_CLOCK_NOT_READY);
    uint32_t irq_enabled=NVIC_GetEnableIRQ(RTC_Alarm_IRQn);
    HAL_NVIC_DisableIRQ(RTC_Alarm_IRQn);
    HAL_StatusTypeDef hal=HAL_RTC_DeactivateAlarm(&hrtc,RTC_ALARM_A);
    if(hal==HAL_OK){
        ClearAlarmAPending();
        RTC_AlarmTypeDef a={0};a.Alarm=RTC_ALARM_A;
        a.AlarmTime.Hours=hour;a.AlarmTime.Minutes=minute;a.AlarmTime.Seconds=second;
        a.AlarmMask=RTC_ALARMMASK_DATEWEEKDAY;a.AlarmDateWeekDay=1U;
        a.AlarmSubSecondMask=RTC_ALARMSUBSECONDMASK_ALL;
        hal=HAL_RTC_SetAlarm_IT(&hrtc,&a,RTC_FORMAT_BIN);
    }
    if(hal==HAL_OK){
        if(!(RTC->CR&RTC_CR_ALRBIE))HAL_NVIC_SetPriority(RTC_Alarm_IRQn,15U,0U);
        HAL_NVIC_EnableIRQ(RTC_Alarm_IRQn);
    }else RestoreAlarmIRQ(irq_enabled);
    return Release(HalResult(hal));
}
/* Cancellation returns the real HAL error and restores entry NVIC state on
 * every path. An unrelated B is never claimed as cancelled by this A-only API. */
uint32_t BSP_Clock_CancelAlarm(void)
{
    uint32_t status=Acquire();if(status)return Result(status);
    if(!g_bsp_clock.ready)return Release(BSP_CLOCK_NOT_READY);
    uint32_t irq_enabled=NVIC_GetEnableIRQ(RTC_Alarm_IRQn);
    HAL_NVIC_DisableIRQ(RTC_Alarm_IRQn);
    HAL_StatusTypeDef hal=HAL_RTC_DeactivateAlarm(&hrtc,RTC_ALARM_A);
    if(hal==HAL_OK)ClearAlarmAPending();
    RestoreAlarmIRQ(irq_enabled);return Release(HalResult(hal));
}
/* IRQ callback never re-enters the serialized task API. */
void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *rtc)
{if(rtc && rtc->Instance==RTC)++g_bsp_clock.alarms;}
