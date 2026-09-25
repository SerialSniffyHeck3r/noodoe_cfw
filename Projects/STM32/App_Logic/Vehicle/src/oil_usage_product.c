#if NOODOE_PRODUCT
#include "OilUsageService.h"
#include "UsageCounter.h"
#include "App_Persistence.h"
#include "stm32f4xx_hal.h"
#include <string.h>
volatile OilUsageSnapshot g_oil_usage;
static UsageCounter counter;
static uint64_t origin;
static uint32_t loaded,known;
/* I/O task collects immediately, including while the FAT audit is running.
 * The durable base is attached once. Raw OFF never starts an independent CFG
 * write; the official session event requests a complete RideStore snapshot. */
void OilUsageService_Process(uint32_t now,uint32_t valid,uint32_t on,uint32_t settings_ready)
{
    (void)settings_ready;
    UsageCounter_Sample(&counter,now,valid,on);
    uint64_t base=0,saved=0;
    uint32_t k=0;
    if(RideStore_GetUsage(&base,&saved,&k)&&!loaded) {
        origin=base;
        known=k;
        loaded=1;
    }
    uint64_t total=UINT64_MAX-origin<counter.on_ms?UINT64_MAX:origin+counter.on_ms;
    uint32_t m=__get_PRIMASK();
    __disable_irq();
    ++g_oil_usage.seq;
    g_oil_usage.magic=0x4F494C31;
    g_oil_usage.version=2;
    g_oil_usage.ready=loaded&&known;
    g_oil_usage.restored=loaded&&known;
    g_oil_usage.persistent=loaded&&known;
    g_oil_usage.save_result=g_app_persistence.ride_status;
    g_oil_usage.gaps=counter.gaps;
    g_oil_usage.now_ms=now;
    g_oil_usage.ign_valid=valid;
    g_oil_usage.ign_on=on;
    g_oil_usage.pending=g_app_persistence.saving;
    g_oil_usage.total_ms=total;
    g_oil_usage.committed_ms=saved;
    g_oil_usage.boot_on_ms=counter.on_ms;
    __DMB();
    ++g_oil_usage.seq;
    __set_PRIMASK(m);
}
uint32_t OilUsageService_Get(OilUsageSnapshot *out)
{
    if(!out)return 0;
    uint32_t m=__get_PRIMASK();
    __disable_irq();
    memcpy(out,(const void*)&g_oil_usage,sizeof(*out));
    __DMB();
    __set_PRIMASK(m);
    return out->ready;
}
#endif
