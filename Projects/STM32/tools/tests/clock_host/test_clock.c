/* Actual BSP_Clock C with installed HAL/CMSIS types. Only HAL operations and
 * W0C/W1C register side effects are replaced at their hardware boundaries. */
#include <stddef.h>
#include <string.h>
#include "rtc.h"
static void ClearAlarmFlag(RTC_HandleTypeDef *rtc,uint32_t flag);
static void ClearAlarmExti(void);
#undef __HAL_RTC_ALARM_CLEAR_FLAG
#define __HAL_RTC_ALARM_CLEAR_FLAG(rtc,flag) ClearAlarmFlag(rtc,flag)
#undef __HAL_RTC_ALARM_EXTI_CLEAR_FLAG
#define __HAL_RTC_ALARM_EXTI_CLEAR_FLAG() ClearAlarmExti()
/* HAL implements this bit through a peripheral bit-band alias. The fixture
 * models the same backing register bit without pretending flat RAM is bit-band. */
#undef __HAL_RCC_RTC_ENABLE
#define __HAL_RCC_RTC_ENABLE() SET_BIT(RCC->BDCR,RCC_BDCR_RTCEN)
#include "BSP_Clock.c"

RTC_HandleTypeDef hrtc;
volatile uint32_t g_assertions,g_failure_line,g_mock_error;
static uint32_t gets_time,gets_date,sets_time,sets_date,deactivations,alarm_sets;
static uint32_t flag_clears,exti_clears,nvic_clears,nvic_pends,priority_sets;
static uint32_t shadow_locked,nested_read,nested_result,inject_b,log_count,log_items[32];
static HAL_StatusTypeDef get_time_status,get_date_status,set_date_status,set_time_status;
static HAL_StatusTypeDef deactivate_status,set_alarm_status;
static HAL_StatusTypeDef init_status,sync_status;
static uint32_t init_calls,sync_calls;
static RTC_TimeTypeDef current_time;
static RTC_DateTypeDef current_date;
static RTC_AlarmTypeDef installed_alarm;
#define CHECK(x) do{++g_assertions;if(!(x)){g_failure_line=__LINE__;return 1U;}}while(0)
void *memset(void *p,int v,size_t n){uint8_t *d=p;for(size_t i=0;i<n;++i)d[i]=(uint8_t)v;return p;}
void *memcpy(void *p,const void *s,size_t n){uint8_t *d=p;const uint8_t *a=s;for(size_t i=0;i<n;++i)d[i]=a[i];return p;}
static void Log(uint32_t item){if(log_count<32U)log_items[log_count++]=item;else ++g_mock_error;}
static void TaskCall(void){if(!api_busy || __get_PRIMASK() || __get_BASEPRI())++g_mock_error;}
static uint32_t IrqBit(void){return 1UL<<((uint32_t)RTC_Alarm_IRQn&31U);}
static uint32_t IrqIndex(void){return (uint32_t)RTC_Alarm_IRQn>>5U;}
void HAL_NVIC_EnableIRQ(IRQn_Type irq){if(irq!=RTC_Alarm_IRQn)++g_mock_error;NVIC->ISER[IrqIndex()]|=IrqBit();Log(8U);}
void HAL_NVIC_DisableIRQ(IRQn_Type irq){if(irq!=RTC_Alarm_IRQn)++g_mock_error;NVIC->ISER[IrqIndex()]&=~IrqBit();Log(1U);}
void HAL_NVIC_ClearPendingIRQ(IRQn_Type irq)
{
    if(irq!=RTC_Alarm_IRQn || !__get_PRIMASK())++g_mock_error;
    ++nvic_clears;NVIC->ISPR[IrqIndex()]&=~IrqBit();Log(5U);
    if(inject_b){inject_b=0U;RTC->ISR|=RTC_ISR_ALRBF;}
}
void HAL_NVIC_SetPendingIRQ(IRQn_Type irq)
{if(irq!=RTC_Alarm_IRQn)++g_mock_error;++nvic_pends;NVIC->ISPR[IrqIndex()]|=IrqBit();}
void HAL_NVIC_SetPriority(IRQn_Type irq,uint32_t preempt,uint32_t sub)
{if(irq!=RTC_Alarm_IRQn || preempt!=15U || sub)++g_mock_error;++priority_sets;Log(7U);}
void HAL_PWR_EnableBkUpAccess(void){TaskCall();PWR->CR|=PWR_CR_DBP;}
HAL_StatusTypeDef HAL_RTC_Init(RTC_HandleTypeDef *rtc)
{TaskCall();++init_calls;if(rtc!=&hrtc||hrtc.State!=HAL_RTC_STATE_READY)++g_mock_error;
 if(init_status==HAL_OK){RTC->PRER=(hrtc.Init.AsynchPrediv<<16)|hrtc.Init.SynchPrediv;}return init_status;}
HAL_StatusTypeDef HAL_RTC_WaitForSynchro(RTC_HandleTypeDef *rtc)
{TaskCall();++sync_calls;if(rtc!=&hrtc)++g_mock_error;return sync_status;}
uint32_t HAL_RTCEx_BKUPRead(RTC_HandleTypeDef *rtc,uint32_t index)
{TaskCall();if(rtc!=&hrtc || index!=RTC_BKP_DR19)++g_mock_error;return RTC->BKP19R;}
HAL_StatusTypeDef HAL_RTC_GetTime(RTC_HandleTypeDef *rtc,RTC_TimeTypeDef *time,uint32_t format)
{
    TaskCall();if(rtc!=&hrtc || format!=RTC_FORMAT_BIN)++g_mock_error;
    ++gets_time;shadow_locked=1U;
    if(nested_read){BSP_Clock_Time nested={0};nested_read=0U;nested_result=BSP_Clock_Read(&nested);}
    *time=current_time;return get_time_status;
}
HAL_StatusTypeDef HAL_RTC_GetDate(RTC_HandleTypeDef *rtc,RTC_DateTypeDef *date,uint32_t format)
{
    TaskCall();if(rtc!=&hrtc || format!=RTC_FORMAT_BIN || !shadow_locked)++g_mock_error;
    ++gets_date;shadow_locked=0U;*date=current_date;return get_date_status;
}
HAL_StatusTypeDef HAL_RTC_SetDate(RTC_HandleTypeDef *rtc,RTC_DateTypeDef *date,uint32_t format)
{TaskCall();if(rtc!=&hrtc || format!=RTC_FORMAT_BIN)++g_mock_error;++sets_date;if(set_date_status==HAL_OK)current_date=*date;return set_date_status;}
HAL_StatusTypeDef HAL_RTC_SetTime(RTC_HandleTypeDef *rtc,RTC_TimeTypeDef *time,uint32_t format)
{TaskCall();if(rtc!=&hrtc || format!=RTC_FORMAT_BIN)++g_mock_error;++sets_time;if(set_time_status==HAL_OK){current_time=*time;RTC->ISR|=RTC_ISR_INITS;}return set_time_status;}
HAL_StatusTypeDef HAL_RTC_DeactivateAlarm(RTC_HandleTypeDef *rtc,uint32_t alarm)
{
    TaskCall();if(rtc!=&hrtc || alarm!=RTC_ALARM_A || NVIC_GetEnableIRQ(RTC_Alarm_IRQn))++g_mock_error;
    ++deactivations;Log(2U);
    /* Installed HAL BUSY exits before writes; timeout happens after A/IE clear. */
    if(deactivate_status!=HAL_BUSY)RTC->CR&=~(RTC_CR_ALRAE|RTC_CR_ALRAIE);
    return deactivate_status;
}
HAL_StatusTypeDef HAL_RTC_SetAlarm_IT(RTC_HandleTypeDef *rtc,RTC_AlarmTypeDef *alarm,uint32_t format)
{
    TaskCall();if(rtc!=&hrtc || format!=RTC_FORMAT_BIN || NVIC_GetEnableIRQ(RTC_Alarm_IRQn) ||
       (RTC->ISR&RTC_ISR_ALRAF) || (RTC->CR&RTC_CR_ALRAE))++g_mock_error;
    ++alarm_sets;Log(6U);installed_alarm=*alarm;
    if(set_alarm_status==HAL_OK)RTC->CR|=RTC_CR_ALRAE|RTC_CR_ALRAIE;
    return set_alarm_status;
}
static void ClearAlarmFlag(RTC_HandleTypeDef *rtc,uint32_t flag)
{
    if(rtc!=&hrtc || flag!=RTC_FLAG_ALRAF || !__get_PRIMASK())++g_mock_error;
    ++flag_clears;RTC->ISR&=~flag;Log(3U);
}
static void ClearAlarmExti(void)
{if(!__get_PRIMASK())++g_mock_error;++exti_clears;EXTI->PR&=~RTC_EXTI_LINE_ALARM_EVENT;Log(4U);}

/* Fresh fixture models MCU C startup, with an existing valid retained calendar
 * and backup words. None of these assignments occurs in production Init. */
static void Fresh(void)
{
    __set_PRIMASK(0U);__set_BASEPRI(0U);__set_FAULTMASK(0U);__set_CONTROL(0U);
    memset((void*)&g_bsp_clock,0,sizeof(g_bsp_clock));memset(&hrtc,0,sizeof(hrtc));
    api_busy=0U;gets_time=gets_date=sets_time=sets_date=deactivations=alarm_sets=0U;
    init_calls=sync_calls=0;init_status=sync_status=HAL_OK;
    flag_clears=exti_clears=nvic_clears=nvic_pends=priority_sets=0U;
    shadow_locked=nested_read=nested_result=inject_b=log_count=0U;
    get_time_status=get_date_status=set_date_status=set_time_status=deactivate_status=set_alarm_status=HAL_OK;
    current_time=(RTC_TimeTypeDef){.Hours=10U,.Minutes=11U,.Seconds=12U};
    current_date=(RTC_DateTypeDef){.Year=26U,.Month=9U,.Date=12U,.WeekDay=6U};
    RCC->BDCR=RCC_BDCR_RTCSEL_0|RCC_BDCR_LSERDY|RCC_BDCR_LSEON;
    RTC->CR=0U;RTC->ISR=RTC_ISR_INITS;RTC->PRER=(127U<<16U)|255U;
    RTC->TR=0x101112U;RTC->DR=0x260912U;RTC->BKP19R=0x13579BDFU;
    RTC->ALRMBR=0x12345678U;RTC->ALRMBSSR=0x87654321U;
    EXTI->PR=0U;NVIC->ISER[IrqIndex()]=NVIC->ISPR[IrqIndex()]=0U;
}
static void PendingA(uint32_t enabled)
{
    RTC->CR|=RTC_CR_ALRAE|RTC_CR_ALRAIE;RTC->ISR|=RTC_ISR_ALRAF;
    EXTI->PR|=RTC_EXTI_LINE_ALARM_EVENT;NVIC->ISPR[IrqIndex()]|=IrqBit();
    if(enabled)NVIC->ISER[IrqIndex()]|=IrqBit();else NVIC->ISER[IrqIndex()]&=~IrqBit();
    log_count=0U;
}
int Clock_TestMain(void)
{
    Fresh();CHECK(BSP_Clock_Init()==BSP_CLOCK_OK && g_bsp_clock.ready && g_bsp_clock.time.valid);
    CHECK(RTC->TR==0x101112U && RTC->DR==0x260912U && RTC->BKP19R==0x13579BDFU);
    CHECK(!(RCC->BDCR&RCC_BDCR_BDRST) && gets_time==1U && gets_date==1U && !sets_time && !sets_date);
    CHECK(hrtc.Init.AsynchPrediv==127U && hrtc.Init.SynchPrediv==255U);
    CHECK(!init_calls&&sync_calls==1);
    /* Full supply loss leaves LSE running but RTCSEL=0. Cold setup selects it
     * and starts counters without inventing a date or resetting backup data. */
    Fresh();RCC->BDCR=RCC_BDCR_LSEON|RCC_BDCR_LSERDY;RTC->ISR=0;current_date.Year=0;
    CHECK(BSP_Clock_Init()==BSP_CLOCK_OK&&g_bsp_clock.ready&&!g_bsp_clock.time.valid);
    CHECK(init_calls==1&&(RCC->BDCR&RCC_BDCR_RTCSEL)==RCC_BDCR_RTCSEL_0&&(RCC->BDCR&RCC_BDCR_RTCEN));
    CHECK(!sets_time&&!sets_date&&RTC->BKP19R==0x13579BDFU&&!(RCC->BDCR&RCC_BDCR_BDRST));
    Fresh();RCC->BDCR=RCC_BDCR_LSERDY|RCC_BDCR_RTCSEL_1;
    CHECK(BSP_Clock_Init()==BSP_CLOCK_NOT_READY&&(RCC->BDCR&RCC_BDCR_RTCSEL)==RCC_BDCR_RTCSEL_1);
    Fresh();RTC->ISR=0;init_status=HAL_TIMEOUT;
    CHECK(BSP_Clock_Init()==BSP_CLOCK_TIMEOUT&&!g_bsp_clock.ready);init_status=HAL_OK;
    Fresh();sync_status=HAL_TIMEOUT;CHECK(BSP_Clock_Init()==BSP_CLOCK_TIMEOUT&&!g_bsp_clock.ready);sync_status=HAL_OK;
    Fresh();
    RCC->BDCR&=~RCC_BDCR_LSERDY;CHECK(BSP_Clock_Init()==BSP_CLOCK_NOT_READY && !g_bsp_clock.ready && !g_bsp_clock.time.valid);
    Fresh();RTC->CR|=RTC_CR_FMT;CHECK(BSP_Clock_Init()==BSP_CLOCK_NOT_READY && !g_bsp_clock.ready && (RTC->CR&RTC_CR_FMT));
    Fresh();get_time_status=HAL_TIMEOUT;CHECK(BSP_Clock_Init()==BSP_CLOCK_TIMEOUT && !g_bsp_clock.ready && !shadow_locked && gets_date==1U);

    Fresh();CHECK(BSP_Clock_Init()==0U);BSP_Clock_Time sample={.year=1234U,.valid=1U};
    get_time_status=HAL_ERROR;uint32_t before=gets_date;
    CHECK(BSP_Clock_Read(&sample)==BSP_CLOCK_IO && gets_date==before+1U && !shadow_locked);
    CHECK(sample.year==1234U && !sample.valid && !g_bsp_clock.time.valid && g_bsp_clock.error==BSP_CLOCK_IO);
    get_time_status=HAL_OK;get_date_status=HAL_BUSY;
    CHECK(BSP_Clock_Read(&sample)==BSP_CLOCK_BUSY && !sample.valid && !shadow_locked);
    get_date_status=HAL_OK;nested_read=1U;before=gets_date;
    CHECK(BSP_Clock_Read(&sample)==0U && nested_result==BSP_CLOCK_BUSY && gets_date==before+1U && sample.valid);
    RTC->ISR&=~RTC_ISR_INITS;CHECK(BSP_Clock_Read(&sample)==0U && !sample.valid);
    RTC->ISR|=RTC_ISR_INITS;current_date.Month=0U;CHECK(BSP_Clock_Read(&sample)==0U && !sample.valid);
    current_date.Month=9U;
    current_date.WeekDay=3U;
    CHECK(BSP_Clock_Read(&sample)==0U&&sample.weekday==6U&&!sets_time&&!sets_date);
    before=gets_time;__disable_irq();CHECK(BSP_Clock_Read(&sample)==BSP_CLOCK_CONTEXT && gets_time==before && __get_PRIMASK());__enable_irq();
    __set_BASEPRI(32U);CHECK(BSP_Clock_CancelAlarm()==BSP_CLOCK_CONTEXT && !deactivations);__set_BASEPRI(0U);

    BSP_Clock_Time desired={2024U,2U,29U,4U,23U,59U,58U,1U};
    CHECK(BSP_Clock_Set(&desired)==0U && g_bsp_clock.sets==1U && g_bsp_clock.time.year==2024U && g_bsp_clock.time.day==29U);
    CHECK(sets_time==1U && sets_date==1U && !shadow_locked);
    desired.weekday=0U;CHECK(BSP_Clock_Set(&desired)==0U&&current_date.WeekDay==4U);
    desired.weekday=7U;CHECK(BSP_Clock_Set(&desired)==0U&&current_date.WeekDay==4U);
    desired.weekday=4U;
    static const uint32_t invalid[][2]={{0U,1999U},{0U,2100U},{1U,0U},{1U,13U},{2U,30U},{3U,8U},{4U,24U},{5U,60U},{6U,60U}};
    before=sets_time+sets_date;
    for(uint32_t i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i){
        BSP_Clock_Time bad=desired;((uint32_t*)&bad)[invalid[i][0]]=invalid[i][1];
        CHECK(BSP_Clock_Set(&bad)==BSP_CLOCK_ARGUMENT && sets_time+sets_date==before);
    }
    BSP_Clock_Time bad=desired;bad.year=2025U;CHECK(BSP_Clock_Set(&bad)==BSP_CLOCK_ARGUMENT && sets_time+sets_date==before);
    set_date_status=HAL_BUSY;before=sets_time;CHECK(BSP_Clock_Set(&desired)==BSP_CLOCK_BUSY && sets_time==before && !g_bsp_clock.time.valid);
    set_date_status=HAL_OK;set_time_status=HAL_TIMEOUT;desired.year=2028U;before=sets_date;
    CHECK(BSP_Clock_Set(&desired)==BSP_CLOCK_TIMEOUT && sets_date==before+1U && current_date.Year==28U && !g_bsp_clock.time.valid);

    Fresh();CHECK(BSP_Clock_Init()==0U);PendingA(1U);
    CHECK(BSP_Clock_SetDailyAlarm(1U,2U,3U)==0U && NVIC_GetEnableIRQ(RTC_Alarm_IRQn));
    CHECK(log_count==8U && log_items[0]==1U && log_items[1]==2U && log_items[5]==6U && log_items[7]==8U);
    CHECK(installed_alarm.Alarm==RTC_ALARM_A && installed_alarm.AlarmMask==RTC_ALARMMASK_DATEWEEKDAY);
    CHECK(installed_alarm.AlarmTime.Hours==1U && installed_alarm.AlarmTime.Minutes==2U && installed_alarm.AlarmTime.Seconds==3U);
    CHECK(installed_alarm.AlarmSubSecondMask==RTC_ALARMSUBSECONDMASK_ALL && !(RTC->ISR&RTC_ISR_ALRAF));
    before=deactivations;CHECK(BSP_Clock_SetDailyAlarm(24U,0U,0U)==BSP_CLOCK_ARGUMENT && deactivations==before);
    /* BUSY cancellation retains the active alarm; failure must be observable. */
    deactivate_status=HAL_BUSY;CHECK(BSP_Clock_CancelAlarm()==BSP_CLOCK_BUSY && g_bsp_clock.error==BSP_CLOCK_BUSY && (RTC->CR&RTC_CR_ALRAE));
    CHECK(NVIC_GetEnableIRQ(RTC_Alarm_IRQn));
    for(uint32_t enabled=0U;enabled<=1U;++enabled){
        for(uint32_t variant=0U;variant<2U;++variant){
            Fresh();CHECK(BSP_Clock_Init()==0U);PendingA(enabled);
            deactivate_status=variant?HAL_TIMEOUT:HAL_BUSY;before=alarm_sets;
            CHECK(BSP_Clock_SetDailyAlarm(1U,2U,3U)==(variant?BSP_CLOCK_TIMEOUT:BSP_CLOCK_BUSY));
            CHECK(NVIC_GetEnableIRQ(RTC_Alarm_IRQn)==enabled && alarm_sets==before && !flag_clears);
            CHECK(BSP_Clock_CancelAlarm()==(variant?BSP_CLOCK_TIMEOUT:BSP_CLOCK_BUSY));
            CHECK(NVIC_GetEnableIRQ(RTC_Alarm_IRQn)==enabled);
            deactivate_status=HAL_OK;set_alarm_status=HAL_TIMEOUT;
            CHECK(BSP_Clock_SetDailyAlarm(1U,2U,3U)==BSP_CLOCK_TIMEOUT && NVIC_GetEnableIRQ(RTC_Alarm_IRQn)==enabled);
            CHECK(!(RTC->CR&RTC_CR_ALRAE));
            CHECK(BSP_Clock_CancelAlarm()==0U && NVIC_GetEnableIRQ(RTC_Alarm_IRQn)==enabled);
        }
    }
    /* Shared pending B survives both installation and cancellation of A. */
    Fresh();CHECK(BSP_Clock_Init()==0U);PendingA(1U);RTC->CR|=RTC_CR_ALRBIE|RTC_CR_ALRBE;RTC->ISR|=RTC_ISR_ALRBF;
    CHECK(BSP_Clock_SetDailyAlarm(4U,5U,6U)==0U && !exti_clears && !nvic_clears && !priority_sets);
    CHECK((RTC->ISR&RTC_ISR_ALRBF) && (EXTI->PR&RTC_EXTI_LINE_ALARM_EVENT) && (NVIC->ISPR[IrqIndex()]&IrqBit()));
    CHECK(RTC->ALRMBR==0x12345678U && RTC->ALRMBSSR==0x87654321U && (RTC->CR&RTC_CR_ALRBIE));
    CHECK(BSP_Clock_CancelAlarm()==0U && !exti_clears && !nvic_clears && NVIC_GetEnableIRQ(RTC_Alarm_IRQn));
    /* B arriving during shared-pending cleanup gets NVIC re-pended. */
    Fresh();CHECK(BSP_Clock_Init()==0U);PendingA(1U);RTC->CR|=RTC_CR_ALRBIE|RTC_CR_ALRBE;inject_b=1U;
    CHECK(BSP_Clock_SetDailyAlarm(4U,5U,6U)==0U && exti_clears==1U && nvic_clears==1U && nvic_pends==1U);
    CHECK((RTC->ISR&RTC_ISR_ALRBF) && (NVIC->ISPR[IrqIndex()]&IrqBit()));
    CHECK(RTC->ALRMBR==0x12345678U && RTC->ALRMBSSR==0x87654321U);
    before=g_bsp_clock.alarms;HAL_RTC_AlarmAEventCallback(NULL);CHECK(g_bsp_clock.alarms==before);
    HAL_RTC_AlarmAEventCallback(&hrtc);CHECK(g_bsp_clock.alarms==before+1U);
    CHECK(!g_mock_error && !api_busy && !__get_PRIMASK());return 0;
}
