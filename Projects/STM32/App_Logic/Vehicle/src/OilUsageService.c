#if !NOODOE_PRODUCT
#include "OilUsageService.h"
#include "UsageCounter.h"
#include "SettingsService.h"
#include "stm32f4xx_hal.h"
#include <string.h>
volatile OilUsageSnapshot g_oil_usage;
static UsageCounter counter;
static uint64_t origin,committed,queued_value;
static uint32_t loaded,restored,pending,last_attempt,attempted,last_on,save_result;

/* No storage I/O on the2ms control task. Queue at most one checkpoint per
 * minute or on key OFF, and acknowledge only the settings worker's verified
 * commit. NOR unprovisioned is an explicit volatile state, never permission
 * to format or overwrite the stock filesystem. Sudden power loss can lose
 * the time since the latest successful checkpoint. */
void OilUsageService_Process(uint32_t now,uint32_t valid,uint32_t on,uint32_t settings_ready)
{
    UsageCounter_Sample(&counter,now,valid,on);
    Settings_Values values={0};Settings_Diagnostics status={0};
    Settings_Status access=SettingsService_GetValues(&values);
    if(settings_ready&&!loaded){
        if(access==SETTINGS_OK&&values.oil_usage_valid){origin=values.oil_on_ms;committed=origin;restored=1U;}
        loaded=1U;
    }
    uint64_t total=UINT64_MAX-origin<counter.on_ms?UINT64_MAX:origin+counter.on_ms;
    if(pending){
        SettingsService_GetDiagnostics(&status);
        if(status.completed_request==pending){
            save_result=status.completed_result;
            if(save_result==SETTINGS_OK)committed=queued_value;
            pending=0U;
        }
    }
    uint32_t off_edge=valid&&!on&&last_on;last_on=valid&&on;
    if(loaded&&access==SETTINGS_OK&&!pending&&total!=committed&&
       (off_edge||total-committed>=60000ULL)&&(!attempted||now-last_attempt>=1000U)){
        last_attempt=now;attempted=1U;uint32_t request=0U;
        save_result=SettingsService_RequestOilUsage(total,&request);
        if(save_result==SETTINGS_OK){pending=request;queued_value=total;}
    }
    /* A failed OFF-edge save remains eligible on later polls below60seconds. */
    if(!on&&loaded&&access==SETTINGS_OK&&!pending&&total!=committed&&
       (!attempted||now-last_attempt>=1000U)){
        last_attempt=now;attempted=1U;uint32_t request=0U;
        save_result=SettingsService_RequestOilUsage(total,&request);
        if(save_result==SETTINGS_OK){pending=request;queued_value=total;}
    }
    uint32_t mask=__get_PRIMASK();__disable_irq();
    ++g_oil_usage.seq;__DMB();
    g_oil_usage.magic=0x4F494C31U;g_oil_usage.version=1U;g_oil_usage.ready=loaded;
    g_oil_usage.restored=restored;g_oil_usage.persistent=access==SETTINGS_OK;
    g_oil_usage.save_result=access==SETTINGS_OK?save_result:(uint32_t)access;
    g_oil_usage.gaps=counter.gaps;g_oil_usage.now_ms=now;
    g_oil_usage.ign_valid=!!valid;g_oil_usage.ign_on=!!on;g_oil_usage.pending=pending;
    g_oil_usage.total_ms=total;g_oil_usage.committed_ms=committed;g_oil_usage.boot_on_ms=counter.on_ms;
    __DMB();++g_oil_usage.seq;__set_PRIMASK(mask);
}

/* IRQ masking bounds one72byte RAM copy; no mutex or wait. */
uint32_t OilUsageService_Get(OilUsageSnapshot *out)
{
    if(!out)return 0U;
    uint32_t mask=__get_PRIMASK();__disable_irq();memcpy(out,(const void*)&g_oil_usage,sizeof(*out));
    __DMB();__set_PRIMASK(mask);return out->ready;
}

#endif
