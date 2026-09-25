#include "BSP_Watchdog.h"
#include "Health_Service.h"
#include "stm32f4xx.h"
#include <stddef.h>
volatile uint32_t test_failure,test_assertions,stable_calls;
void *memset(void *p,int v,size_t n){unsigned char *b=p;while(n--)*b++=(unsigned char)v;return p;}
void *memcpy(void *d,const void *s,size_t n){unsigned char *a=d;const unsigned char *b=s;while(n--)*a++=*b++;return d;}
void HealthService_StableBoot(void){++stable_calls;}
#define CHECK(v) do{++test_assertions;if(!(v)){test_failure=__LINE__;return 1;}}while(0)
static void Reset(void){memset((void*)&g_bsp_watchdog,0,sizeof(g_bsp_watchdog));memset((void*)&g_health,0,sizeof(g_health));RCC->CSR=RCC_CSR_LSIRDY;IWDG->SR=0;DWT->CYCCNT=0;}
static void Progress(uint32_t now){for(uint32_t i=0;i<4;++i)HealthService_Progress(i,now);}
static void Runtime(uint32_t now){HealthService_Init(now);for(uint32_t i=0;i<4;++i)HealthService_Register(i,now);Progress(now);HealthService_BootReady();}
uint32_t Watchdog_Test(void)
{
    Reset();CHECK(BSP_Watchdog_StartEarly());CHECK(IWDG->PR==6&&IWDG->RLR==4095);
    CHECK(BSP_Watchdog_Init(100,WATCHDOG_BOOT,30000));
    CHECK(!BSP_Watchdog_Init(101,WATCHDOG_WAIT,0));
    CHECK(BSP_Watchdog_Checkpoint(101,1));uint32_t count=g_bsp_watchdog.feeds;
    CHECK(!BSP_Watchdog_Checkpoint(900,1));CHECK(g_bsp_watchdog.feeds==count);
    CHECK(!BSP_Watchdog_Checkpoint(2200,2));CHECK(g_bsp_watchdog.phase==WATCHDOG_FAILED);
    CHECK(!BSP_Watchdog_SetPhase(2201,WATCHDOG_WAIT,0));CHECK(!BSP_Watchdog_RunCheckpoint(2201));

    Reset();CHECK(BSP_Watchdog_Init(0,WATCHDOG_BOOT,30000));
    for(uint32_t n=1;n<=30;++n)CHECK(BSP_Watchdog_Checkpoint(n*1000,n));
    CHECK(!BSP_Watchdog_SetPhase(30000,WATCHDOG_BOOT,30000));
    CHECK(!BSP_Watchdog_Checkpoint(30001,31));CHECK(g_bsp_watchdog.failure);

    Reset();Runtime(0);CHECK(HealthService_Process(0,0));CHECK(g_bsp_watchdog.phase==WATCHDOG_RUN);
    /* IRQ/tick and graphics can continue while IO is dead: other reports do
     * not forgive its missed deadline or permit recovery by a late report. */
    for(uint32_t n=1;n<=10;++n){for(uint32_t i=1;i<4;++i)HealthService_Progress(i,n*100);CHECK(HealthService_Process(n*100,0));}
    for(uint32_t i=1;i<4;++i)HealthService_Progress(i,1100);
    CHECK(!HealthService_Process(1100,0));CHECK(g_health.failed_owner==HEALTH_IO);
    count=g_bsp_watchdog.feeds;Progress(1101);CHECK(!HealthService_Process(1101,0));CHECK(g_bsp_watchdog.feeds==count);

    Reset();Runtime(0);CHECK(HealthService_Process(0,1));
    /* Retained/deep owners legitimately return only every1000ms. No new
     * UART packet, display frame, or successful BT connection is required. */
    for(uint32_t n=1;n<=61;++n){Progress(n*1000);CHECK(HealthService_Process(n*1000,1));
        CHECK(stable_calls==(n>=30?1U:0U));}
    CHECK(stable_calls==1);CHECK(BSP_Watchdog_CanSleep(61000,500));
    CHECK(!BSP_Watchdog_CanSleep(61200,500));CHECK(!BSP_Watchdog_CanSleep(61000,501));
    count=g_bsp_watchdog.feeds;CHECK(!BSP_Watchdog_CanSleep(62000,1));CHECK(g_bsp_watchdog.feeds==count);
    /* A worker cannot renew its registration instead of making progress. */
    HealthService_Register(HEALTH_STORAGE,63101);
    CHECK(!HealthService_Process(63101,1));CHECK(g_health.failed_owner==HEALTH_STORAGE);

    Reset();Runtime(0xFFFFFF00U);CHECK(HealthService_Process(0xFFFFFF00U,0));
    Progress(0x50U);CHECK(HealthService_Process(0x50U,0));
    CHECK(BSP_Watchdog_CanSleep(0x50U,500));
    CHECK(!HealthService_Process(0x450U,0));

    Reset();DWT->CYCCNT=0xFFF00000U;Runtime(0);CHECK(HealthService_Process(0,0));
    CHECK(BSP_Watchdog_BeginFlash(1,16000000));
    CHECK(!BSP_Watchdog_BeginFlash(2,16000000));
    CHECK(!BSP_Watchdog_SetPhase(2,WATCHDOG_WAIT,0));
    count=g_bsp_watchdog.feeds;CHECK(!HealthService_Process(3,0));CHECK(g_bsp_watchdog.feeds==count);
    DWT->CYCCNT=0xFFF00000U+79000000U;CHECK(BSP_Watchdog_RamCheckpoint());
    DWT->CYCCNT=0xFFF00000U+80000000U;CHECK(!BSP_Watchdog_RamCheckpoint());
    CHECK(g_bsp_watchdog.phase==WATCHDOG_FAILED);CHECK(!BSP_Watchdog_EndFlash());
    count=g_bsp_watchdog.feeds;CHECK(!BSP_Watchdog_RamCheckpoint());CHECK(g_bsp_watchdog.feeds==count);

    Reset();Runtime(0);CHECK(HealthService_Process(0,0));
    CHECK(BSP_Watchdog_BeginFlash(10,180000000));DWT->CYCCNT=180000000;
    CHECK(BSP_Watchdog_EndFlash());CHECK(g_bsp_watchdog.phase==WATCHDOG_RUN);
    Progress(1000);CHECK(HealthService_Process(1000,0));
    /* A stale runtime check cannot be converted into a flash exemption. */
    DWT->CYCCNT+=180000000U;CHECK(!BSP_Watchdog_BeginFlash(1100,180000000));
    Reset();Runtime(0);CHECK(HealthService_Process(0,0));
    CHECK(BSP_Watchdog_BeginFlash(1,16000000));DWT->CYCCNT=64000000;
    CHECK(BSP_Watchdog_EndFlash());CHECK(g_bsp_watchdog.last_flash_ms==4000);
    CHECK(HealthService_Process(1,0));CHECK(g_health.last_ms[HEALTH_IO]==1);
    CHECK(!HealthService_Process(1002,0));CHECK(g_health.failed_owner==HEALTH_IO);
    Reset();CHECK(BSP_Watchdog_Init(0,WATCHDOG_WAIT,0));CHECK(BSP_Watchdog_Checkpoint(1,1));
    CHECK(BSP_Watchdog_BeginFlash(2,16000000));DWT->CYCCNT=64000000;
    CHECK(BSP_Watchdog_EndFlash());CHECK(BSP_Watchdog_Checkpoint(4003,2));
    CHECK(!BSP_Watchdog_Checkpoint(7000,3));
    /* A missing mandatory task may not leave BOOT healthy forever. */
    Reset();HealthService_Init(0);HealthService_Register(HEALTH_IO,0);HealthService_BootReady();
    for(uint32_t n=0;n<=30;++n)CHECK(HealthService_Process(n*1000,0));
    CHECK(!HealthService_Process(30100,0));CHECK(g_bsp_watchdog.failure);
    /* The supervisor itself may be the blocked storage owner. Its late
     * return must not erase the deadline miss before the next health check. */
    Reset();Runtime(0);CHECK(HealthService_Process(0,0));
    count=g_bsp_watchdog.feeds;
    HealthService_Progress(HEALTH_STORAGE,2101);
    CHECK(g_bsp_watchdog.failure&&g_health.failed_owner==HEALTH_STORAGE);
    Progress(2101);CHECK(!HealthService_Process(2101,0));CHECK(g_bsp_watchdog.feeds==count);
    /* Leave a genuinely healthy RUN state for the injected exception test;
     * a pre-existing failed phase would hide a missing IPSR guard. */
    Reset();Runtime(0);CHECK(HealthService_Process(0,0));
    return 0;
}
uint32_t Watchdog_IRQ_Test(void)
{
    uint32_t feed=g_bsp_watchdog.feeds,progress=g_health.progress[HEALTH_IO];
    HealthService_Progress(HEALTH_IO,100);(void)BSP_Watchdog_RunCheckpoint(100);
    (void)BSP_Watchdog_Checkpoint(100,100);(void)BSP_Watchdog_RamCheckpoint();
    return BSP_Watchdog_StartEarly()!=0||feed!=g_bsp_watchdog.feeds||progress!=g_health.progress[HEALTH_IO];
}
uint32_t Watchdog_StartFailure_Test(void)
{
    Reset();RCC->CSR=0;HealthService_Init(123);
    CHECK(g_bsp_watchdog.initialized);CHECK(g_bsp_watchdog.failure==0x210U);
    CHECK(g_bsp_watchdog.phase==WATCHDOG_FAILED);
    CHECK(!HealthService_Process(124,0));CHECK(!g_bsp_watchdog.feeds);
    return 0;
}
