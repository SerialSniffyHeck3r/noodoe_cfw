/* Production service with real settings worker and mocked journal from the
 * neighbouring test. Separate TU keeps production static ownership intact. */
#include "../../../App_Logic/Vehicle/src/OilUsageService.c"
#define CHECK(x) do{if(!(x))return __LINE__;}while(0)
unsigned OilUsage_Test(void)
{
    UsageCounter c={0};UsageCounter_Sample(&c,0xfffffff0U,1,1);
    UsageCounter_Sample(&c,0x10U,1,0);CHECK(c.on_ms==32U);
    UsageCounter_Sample(&c,0x20U,1,1);CHECK(c.on_ms==32U);
    UsageCounter_Sample(&c,0x30U,0,0);CHECK(c.on_ms==48U);
    UsageCounter_Sample(&c,0x40U,1,1);CHECK(c.on_ms==48U);
    UsageCounter_Sample(&c,5000U,1,1);CHECK(c.gaps==1U&&c.on_ms==48U);
    c.on_ms=UINT64_MAX-3U;UsageCounter_Sample(&c,5004U,1,1);CHECK(c.on_ms==UINT64_MAX);
    Settings_Values values;CHECK(SettingsService_GetValues(&values)==SETTINGS_OK);
    uint64_t base=values.oil_on_ms;OilUsageSnapshot v;
    OilUsageService_Process(0U,1U,1U,1U);CHECK(OilUsageService_Get(&v)&&v.restored&&v.total_ms==base);
    for(uint32_t t=1000;t<=60000;t+=1000)OilUsageService_Process(t,1U,1U,1U);
    OilUsageService_Get(&v);CHECK(v.total_ms==base+60000U&&v.pending&&v.committed_ms==base);
    SettingsService_Process(60000U);OilUsageService_Process(60001U,1U,1U,1U);
    OilUsageService_Get(&v);CHECK(v.committed_ms==base+60000U&&!v.pending);
    OilUsageService_Process(60500U,1U,0U,1U);
    OilUsageService_Process(61001U,1U,0U,1U);SettingsService_Process(61001U);
    OilUsageService_Process(61002U,1U,0U,1U);OilUsageService_Get(&v);
    CHECK(v.total_ms==base+60500U&&v.committed_ms==v.total_ms);
    CHECK(SettingsService_GetValues(&values)==SETTINGS_OK&&values.oil_on_ms==v.total_ms);
    /* Simulated MCU reset clears this service's RAM, keeping the committed
     * settings journal. First boot sample restores without charging OFF time. */
    counter=(UsageCounter){0};origin=committed=queued_value=0;
    loaded=restored=pending=last_attempt=attempted=last_on=save_result=0;
    OilUsageService_Process(0U,1U,0U,1U);OilUsageService_Get(&v);
    CHECK(v.restored&&v.total_ms==values.oil_on_ms&&v.boot_on_ms==0U);
    return 0U;
}
