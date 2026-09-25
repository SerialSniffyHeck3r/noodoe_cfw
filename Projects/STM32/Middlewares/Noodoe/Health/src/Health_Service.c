#include "Health_Service.h"
#include "BSP_Watchdog.h"
#include "stm32f4xx.h"
volatile HealthDiagnostics g_health;
static uint32_t stable_reported,healthy_started,flash_seen,owners_sleeping;
/* One deadline policy for both arrival and supervision. A late heartbeat
 * must not erase a missed deadline before its owner runs the supervisor. */
static uint32_t OwnerLimit(uint32_t owner)
{return owner==HEALTH_STORAGE?2000U:owner==HEALTH_GRAPHICS||owners_sleeping?2500U:1000U;}
__attribute__((weak)) void HealthService_StableBoot(void){}
void HealthService_Init(uint32_t now)
{
    if(g_health.magic)return;
    g_health=(HealthDiagnostics){.magic=0x484C5431U,.version=1,.failed_owner=UINT32_MAX};
    stable_reported=0;healthy_started=0;owners_sleeping=0;
    if(!g_bsp_watchdog.initialized){
        /* Publish diagnostics before the bounded hardware start. Otherwise a
         * failed LSI startup would have no initialized record in which to
         * latch failure, and a later supervisor could appear healthy. */
        if(!BSP_Watchdog_Init(now,WATCHDOG_BOOT,30000U)||!BSP_Watchdog_StartEarly())
            BSP_Watchdog_Fail(0x210U);
    }
    flash_seen=g_bsp_watchdog.flash_completions;
}
void HealthService_Register(uint32_t owner,uint32_t now)
{
    if(owner>=HEALTH_OWNER_COUNT||__get_IPSR())return;
    uint32_t mask=__get_PRIMASK();__disable_irq();
    uint32_t bit=1U<<owner;
    if(!(g_health.registered&bit)){g_health.last_ms[owner]=now;g_health.registered|=bit;}
    __set_PRIMASK(mask);
}
void HealthService_Progress(uint32_t owner,uint32_t now)
{
    if(owner>=HEALTH_OWNER_COUNT||__get_IPSR())return;
    /* An owner registers before task creation. Reporting cannot silently add
     * an optional worker or forgive one that has already missed its deadline. */
    if(g_health.registered&(1U<<owner)){
        if(g_bsp_watchdog.failure)return;
        /* Only the completed, bounded FLASH lease may pause all task clocks.
         * Process consumes that grace once. First BOOT reports retain their
         * existing total startup budget; ordinary reports never renew it. */
        if(g_health.progress[owner]&&flash_seen==g_bsp_watchdog.flash_completions&&
           now-g_health.last_ms[owner]>OwnerLimit(owner)){
            g_health.failed_owner=owner;BSP_Watchdog_Fail(0x200U+owner);return;
        }
        g_health.last_ms[owner]=now;__DMB();++g_health.progress[owner];
    }
}
void HealthService_BootReady(void){g_health.boot_ready=1U;}
uint32_t HealthService_Process(uint32_t now,uint32_t sleeping)
{
    if(__get_IPSR()||!g_health.magic||g_bsp_watchdog.failure)return 0;
    /* FLASH owns a finite independent DWT lease while the scheduler/IRQs are
     * stopped. This branch grants no refresh and cannot prolong that lease. */
    if(g_bsp_watchdog.phase==WATCHDOG_FLASH)return 0;
    uint32_t mask=__get_PRIMASK();__disable_irq();
    owners_sleeping=!!sleeping;
    /* A completed finite FLASH phase may have suspended all task scheduling.
     * HAL time might have stopped too, hence no arithmetic with DWT elapsed
     * time here. Rebase once, without changing progress counters or clearing
     * a failure; every owner must return within its normal deadline again. */
    if(flash_seen!=g_bsp_watchdog.flash_completions){
        flash_seen=g_bsp_watchdog.flash_completions;
        for(uint32_t owner=0;owner<HEALTH_OWNER_COUNT;++owner)
            if(g_health.registered&(1U<<owner))g_health.last_ms[owner]=now;
    }
    for(uint32_t owner=0;owner<HEALTH_OWNER_COUNT;++owner){
        if(!(g_health.registered&(1U<<owner)))continue;
        uint32_t limit=OwnerLimit(owner);
        /* Initial resource/peripheral setup has a total30s BOOT budget. Once
         * a worker has returned its first complete iteration, ordinary limits
         * apply even while another optional resource is still initializing. */
        if(!g_health.progress[owner]&&g_bsp_watchdog.phase==WATCHDOG_BOOT)continue;
        if(now-g_health.last_ms[owner]>limit){
            g_health.failed_owner=owner;BSP_Watchdog_Fail(0x200U+owner);
            __set_PRIMASK(mask);return 0;
        }
    }
    ++g_health.checks;
    uint32_t complete=g_health.boot_ready;
    for(uint32_t n=0;n<HEALTH_OWNER_COUNT;++n)
        if((g_health.registered&(1U<<n))&&!g_health.progress[n])complete=0;
    if(complete&&g_bsp_watchdog.phase==WATCHDOG_BOOT)
        (void)BSP_Watchdog_SetPhase(now,WATCHDOG_RUN,0);
    __set_PRIMASK(mask);
    uint32_t ok=g_bsp_watchdog.phase==WATCHDOG_RUN?
        BSP_Watchdog_RunCheckpoint(now):BSP_Watchdog_Checkpoint(now,g_health.checks);
    if(ok&&g_bsp_watchdog.phase==WATCHDOG_RUN){
        BSP_Watchdog_GrantSleep(now);
        if(!healthy_started){healthy_started=1;g_health.healthy_since=now;}
        if(!stable_reported&&now-g_health.healthy_since>=HEALTH_STABLE_BOOT_MS){
            stable_reported=1;HealthService_StableBoot();
        }
    }
    return ok;
}
