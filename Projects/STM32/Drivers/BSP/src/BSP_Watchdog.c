#include "BSP_Watchdog.h"
#include "stm32f4xx.h"

#define WD_MAGIC 0x57444732U
#define WD_RAM __attribute__((section(".RamFunc.watchdog"),noinline,noclone))
volatile BSP_WatchdogDiagnostics g_bsp_watchdog;

/* This is the only IWDG key-register store in project-owned C. Keeping it in
 * SRAM also permits a bounded flash worker to use the same physical writer. */
static WD_RAM void HardwareKey(uint32_t key){IWDG->KR=key;}

/* StartEarly cannot call the RAM address of HardwareKey: .RamFunc has not yet
 * been copied by C startup. Both entry points use the SAME store instruction
 * by running this tiny global-free helper
 * from its flash load address before startup; ordinary calls use its RAM VMA. */
uint32_t BSP_Watchdog_StartEarly(void)
{
    if(__get_IPSR())return 0;
    RCC->CSR|=RCC_CSR_LSION;
    uint32_t left=1000000U;
    while(!(RCC->CSR&RCC_CSR_LSIRDY))if(!--left)return 0;
    /* The linker places .RamFunc inside .data. Its flash load image already
     * exists before C initialization; this small routine has no RAM references.
     * Preserve the Thumb bit while translating its VMA to its LMA. */
    extern uint8_t _sidata[],_sdata[];
    void (*key)(uint32_t)=(void(*)(uint32_t))((uintptr_t)_sidata+
        ((uintptr_t)HardwareKey-(uintptr_t)_sdata));
    /* Match the device HAL sequence: start the watchdog's kernel before
     * requesting prescaler/reload updates. With only RCC.LSION enabled, the
     * F429 keeps PVU/RVU set and the previous sequence timed out before ever
     * reaching its start key (observed SR=3, PR=0 on the bench). */
    key(0xCCCCU);key(0x5555U);
    IWDG->PR=6U;IWDG->RLR=4095U;
    left=1000000U;
    while(IWDG->SR&3U)if(!--left)return 0;
    key(0xAAAAU);
    return 1;
}
static uint32_t ValidPhase(uint32_t phase,uint32_t budget)
{
    return (phase==WATCHDOG_BOOT&&budget&&budget<=30000U)||
           (phase==WATCHDOG_RECOVERY&&budget&&budget<=600000U)||
           ((phase==WATCHDOG_WAIT||phase==WATCHDOG_RUN)&&budget==0U);
}
void BSP_Watchdog_Fail(uint32_t reason)
{
    if(g_bsp_watchdog.magic!=WD_MAGIC)return;
    if(!g_bsp_watchdog.failure)g_bsp_watchdog.failure=reason?reason:1U;
    g_bsp_watchdog.phase=WATCHDOG_FAILED;g_bsp_watchdog.sleep_until_ms=0;
}
uint32_t BSP_Watchdog_Init(uint32_t now,uint32_t phase,uint32_t budget)
{
    if(__get_IPSR()||g_bsp_watchdog.initialized||!ValidPhase(phase,budget))return 0;
    CoreDebug->DEMCR|=CoreDebug_DEMCR_TRCENA_Msk;DWT->CTRL|=DWT_CTRL_CYCCNTENA_Msk;
    g_bsp_watchdog=(BSP_WatchdogDiagnostics){.magic=WD_MAGIC,.version=2,
        .phase=phase,.started_ms=now,.last_progress_ms=now,.last_feed_ms=now,
        .last_feed_cycles=DWT->CYCCNT,
        .budget_ms=budget,.progress=UINT32_MAX,.initialized=1};
    return 1;
}
uint32_t BSP_Watchdog_SetPhase(uint32_t now,uint32_t phase,uint32_t budget)
{
    if(__get_IPSR()||!g_bsp_watchdog.initialized||g_bsp_watchdog.failure||
       g_bsp_watchdog.phase==WATCHDOG_FLASH||!ValidPhase(phase,budget))return 0;
    /* Re-entering the same phase must not extend an absolute boot/job budget. */
    if(g_bsp_watchdog.phase==phase)return 0;
    g_bsp_watchdog.phase=phase;g_bsp_watchdog.started_ms=now;
    g_bsp_watchdog.last_progress_ms=now;g_bsp_watchdog.progress=UINT32_MAX;
    g_bsp_watchdog.budget_ms=budget;return 1;
}
static uint32_t Feed(uint32_t now)
{
    if(__get_IPSR()||!g_bsp_watchdog.initialized||g_bsp_watchdog.failure){++g_bsp_watchdog.rejected;return 0;}
    HardwareKey(0xAAAAU);g_bsp_watchdog.last_feed_ms=now;
    g_bsp_watchdog.last_feed_cycles=DWT->CYCCNT;++g_bsp_watchdog.feeds;return 1;
}
uint32_t BSP_Watchdog_Checkpoint(uint32_t now,uint32_t progress)
{
    uint32_t phase=g_bsp_watchdog.phase;
    if(__get_IPSR()||!g_bsp_watchdog.initialized||g_bsp_watchdog.failure||
       (phase!=WATCHDOG_BOOT&&phase!=WATCHDOG_WAIT&&phase!=WATCHDOG_RECOVERY))return 0;
    if((g_bsp_watchdog.budget_ms&&now-g_bsp_watchdog.started_ms>g_bsp_watchdog.budget_ms)||
       now-g_bsp_watchdog.last_progress_ms>2000U+g_bsp_watchdog.flash_grace_ms){BSP_Watchdog_Fail(0x100U|phase);return 0;}
    if(progress==g_bsp_watchdog.progress)return 0;
    g_bsp_watchdog.flash_grace_ms=0;
    g_bsp_watchdog.progress=progress;g_bsp_watchdog.last_progress_ms=now;
    return Feed(now);
}
uint32_t BSP_Watchdog_RunCheckpoint(uint32_t now)
{return g_bsp_watchdog.phase==WATCHDOG_RUN?Feed(now):0;}

uint32_t BSP_Watchdog_BeginFlash(uint32_t now,uint32_t hz)
{
    if(__get_IPSR()||!g_bsp_watchdog.initialized||g_bsp_watchdog.failure||
       g_bsp_watchdog.phase==WATCHDOG_FLASH||hz<1000000U||hz>180000000U)return 0;
    /* Runtime flash must follow a recent successful all-owner check. A
     * stalled supervisor cannot grant itself a new exception to its timeout. */
    if(g_bsp_watchdog.phase==WATCHDOG_RUN&&(!g_bsp_watchdog.feeds||
       now-g_bsp_watchdog.last_feed_ms>250U||
       (uint32_t)(DWT->CYCCNT-g_bsp_watchdog.last_feed_cycles)>hz/4U))return 0;
    CoreDebug->DEMCR|=CoreDebug_DEMCR_TRCENA_Msk;DWT->CTRL|=DWT_CTRL_CYCCNTENA_Msk;
    g_bsp_watchdog.flash_started_cycles=DWT->CYCCNT;
    g_bsp_watchdog.flash_cycles=hz*5U;g_bsp_watchdog.flash_previous_phase=g_bsp_watchdog.phase;
    g_bsp_watchdog.phase=WATCHDOG_FLASH;g_bsp_watchdog.sleep_until_ms=0;
    return Feed(now);
}
uint32_t WD_RAM BSP_Watchdog_RamCheckpoint(void)
{
    if(__get_IPSR()||g_bsp_watchdog.phase!=WATCHDOG_FLASH||g_bsp_watchdog.failure)return 0;
    if((uint32_t)(DWT->CYCCNT-g_bsp_watchdog.flash_started_cycles)>=g_bsp_watchdog.flash_cycles){
        g_bsp_watchdog.failure=0x105U;g_bsp_watchdog.phase=WATCHDOG_FAILED;return 0;}
    HardwareKey(0xAAAAU);++g_bsp_watchdog.feeds;return 1;
}
uint32_t WD_RAM BSP_Watchdog_EndFlash(void)
{
    if(!BSP_Watchdog_RamCheckpoint())return 0;
    /* One completed lease is a bounded pause, not a new phase budget. The
     * caller's clock may keep running (gate DWT) or stop (masked HAL tick), so
     * never synthesize a future timestamp. Consume this grace at the next
     * progress checkpoint; the runtime supervisor separately observes the
     * successful completion counter and requires fresh owner iterations. */
    g_bsp_watchdog.last_flash_ms=(DWT->CYCCNT-g_bsp_watchdog.flash_started_cycles)/(g_bsp_watchdog.flash_cycles/5000U);
    g_bsp_watchdog.flash_grace_ms=g_bsp_watchdog.last_flash_ms+1U;
    ++g_bsp_watchdog.flash_completions;
    g_bsp_watchdog.phase=g_bsp_watchdog.flash_previous_phase;return 1;
}
void BSP_Watchdog_GrantSleep(uint32_t now)
{
    if(g_bsp_watchdog.phase==WATCHDOG_RUN&&!g_bsp_watchdog.failure){
        g_bsp_watchdog.sleep_until_ms=now+600U;++g_bsp_watchdog.sleep_grants;}
}
uint32_t BSP_Watchdog_CanSleep(uint32_t now,uint32_t ms)
{
    /* Profiles without an active supervisor retain the existing port. An
     * active watchdog, however, cannot bypass a denied/expired health grant. */
    if(!g_bsp_watchdog.initialized)return 1;
    return g_bsp_watchdog.phase==WATCHDOG_RUN&&!g_bsp_watchdog.failure&&ms<=500U&&
        (int32_t)(g_bsp_watchdog.sleep_until_ms-now)>=(int32_t)ms;
}
