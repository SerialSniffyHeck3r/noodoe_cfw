/* Actual production BSP and service C; only HAL boundaries are mocked.
 * No physical device is opened. CMSIS critical sections run on emulated ARM. */
#include <stddef.h>
#include <string.h>
#include "i2c.h"
#include "BSP_Ambient.c"
#include "AmbientService.c"
I2C_HandleTypeDef hi2c3;
volatile BSP_AmbientBitbang_Diagnostics g_bsp_ambient_bitbang;
volatile BSP_AmbientAddress_Diagnostics g_bsp_ambient_address;
static uint32_t bitbang_calls,bitbang_result,bitbang_restore;
static uint32_t address_calls,address_result,address_restore;
uint32_t BSP_AmbientAddress_ReadFixedCandidates(uint32_t id,uint32_t seq)
{
    ++address_calls;
    g_bsp_ambient_address.operation_id=id;g_bsp_ambient_address.request_seq=seq;
    g_bsp_ambient_address.result=address_result;g_bsp_ambient_address.restore_result=address_restore;
    g_bsp_ambient_address.sequence+=2U;return address_result;
}
/* Electrical/register driver is independently tested by test_bitbang.c.
 * Here the service boundary checks queue ownership and failure quarantine. */
uint32_t BSP_AmbientBitbang_ReadIDs(uint32_t id,uint32_t seq)
{
    ++bitbang_calls;
    g_bsp_ambient_bitbang.pullup_mode=0U;
    g_bsp_ambient_bitbang.operation_id=id;g_bsp_ambient_bitbang.request_seq=seq;
    g_bsp_ambient_bitbang.restore_result=bitbang_restore;
    g_bsp_ambient_bitbang.result=bitbang_result;
    g_bsp_ambient_bitbang.sequence+=2U;
    return bitbang_result;
}
uint32_t BSP_AmbientBitbang_ReadIDsWithPullup(uint32_t id,uint32_t seq)
{
    uint32_t result=BSP_AmbientBitbang_ReadIDs(id,seq);
    g_bsp_ambient_bitbang.pullup_mode=1U;return result;
}
volatile uint32_t g_assertions,g_failure_line,g_mock_error;
static uint32_t tick,hal_calls,reads,writes,deinits,inits,last_speed;
static uint32_t fail_phase,fail_status,fail_error,fail_sr1,nested,inside_id;
static uint32_t nested_status,nested_opid,nested_phase,expected_old_response;
static uint16_t sensor_manufacturer,sensor_device,sensor_config,sensor_result;
static Ambient_Snapshot nested_snapshot;
#define CHECK(x) do{++g_assertions;if(!(x)){g_failure_line=__LINE__;return 1U;}}while(0)
void *memset(void *p,int v,size_t n){uint8_t *d=p;for(size_t i=0;i<n;++i)d[i]=(uint8_t)v;return p;}
void *memcpy(void *p,const void *s,size_t n){uint8_t *d=p;const uint8_t *a=s;for(size_t i=0;i<n;++i)d[i]=a[i];return p;}
uint32_t HAL_GetTick(void){return tick;}
static HAL_StatusTypeDef MockStatus(I2C_HandleTypeDef *h)
{
    ++hal_calls;
    if(h!=&hi2c3 || h->Instance!=I2C3 || __get_PRIMASK() || __get_BASEPRI())++g_mock_error;
    if(nested && g_bsp_ambient.phase==nested_phase) {
        nested=0U;inside_id=g_ambient_service.pending_id;
        nested_opid=0xAABBCCDDU;
        nested_status=AmbientService_RequestProbe(100000U,&nested_opid);
        AmbientService_GetSnapshot(&nested_snapshot);
        if(g_ambient_mailbox.response_seq!=expected_old_response)++g_mock_error;
    }
    if(g_bsp_ambient.phase==fail_phase) {
        tick+=20U;h->ErrorCode=fail_error;h->Instance->SR1=fail_sr1;
        return (HAL_StatusTypeDef)fail_status;
    }
    ++tick;h->ErrorCode=HAL_I2C_ERROR_NONE;h->Instance->SR1=0U;return HAL_OK;
}
HAL_StatusTypeDef HAL_I2C_DeInit(I2C_HandleTypeDef *h)
{++deinits;return MockStatus(h);}
HAL_StatusTypeDef HAL_I2C_Init(I2C_HandleTypeDef *h)
{++inits;last_speed=h->Init.ClockSpeed;if(last_speed!=80000U && last_speed!=100000U && last_speed!=400000U)++g_mock_error;return MockStatus(h);}
HAL_StatusTypeDef HAL_I2CEx_ConfigAnalogFilter(I2C_HandleTypeDef *h,uint32_t value)
{if(value!=I2C_ANALOGFILTER_ENABLE)++g_mock_error;return MockStatus(h);}
HAL_StatusTypeDef HAL_I2CEx_ConfigDigitalFilter(I2C_HandleTypeDef *h,uint32_t value)
{if(value)++g_mock_error;return MockStatus(h);}
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *h,uint16_t address,uint16_t reg,
    uint16_t size,uint8_t *data,uint16_t length,uint32_t timeout)
{
    ++reads;
    if(address!=0x8AU || size!=I2C_MEMADD_SIZE_8BIT || length!=2U || timeout!=20U)++g_mock_error;
    HAL_StatusTypeDef status=MockStatus(h);if(status!=HAL_OK)return status;
    uint16_t value=0;
    if(reg==0x7EU)value=sensor_manufacturer;
    else if(reg==0x7FU)value=sensor_device;
    else if(reg==1U)value=sensor_config;
    else if(reg==0U)value=sensor_result;
    else ++g_mock_error;
    data[0]=(uint8_t)(value>>8U);data[1]=(uint8_t)value;return status;
}
HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *h,uint16_t address,uint16_t reg,
    uint16_t size,uint8_t *data,uint16_t length,uint32_t timeout)
{
    ++writes;
    if(address!=0x8AU || reg!=1U || size!=I2C_MEMADD_SIZE_8BIT || length!=2U || timeout!=20U)++g_mock_error;
    HAL_StatusTypeDef status=MockStatus(h);
    if(status==HAL_OK)sensor_config=((uint16_t)data[0]<<8U)|data[1];
    return status;
}
static void Reset(void)
{
    memset(&hi2c3,0,sizeof(hi2c3));memset((void *)&g_bsp_ambient,0,sizeof(g_bsp_ambient));
    memset((void *)&g_ambient_service,0,sizeof(g_ambient_service));
    memset((void *)&g_ambient_mailbox,0,sizeof(g_ambient_mailbox));
    memset(&request,0,sizeof(request));
    last_poll=0U;initialized=0U;busy=0U;queued=0U;next_id=0U;last_mailbox_seq=0U;
    state=0U;requested_hz=0U;desired_enabled=0U;retries=0U;next_retry=0U;
    diagnostic_quarantine=0U;bitbang_calls=0U;bitbang_result=0U;bitbang_restore=0U;
    memset((void*)&g_bsp_ambient_bitbang,0,sizeof(g_bsp_ambient_bitbang));
    memset((void*)&g_bsp_ambient_address,0,sizeof(g_bsp_ambient_address));
    address_calls=address_result=address_restore=0U;
    tick=0U;hal_calls=0U;reads=0U;writes=0U;deinits=0U;inits=0U;
    fail_phase=0U;fail_status=HAL_ERROR;fail_error=HAL_I2C_ERROR_TIMEOUT;fail_sr1=I2C_SR1_ARLO;
    nested=0U;expected_old_response=0U;sensor_manufacturer=0x5449U;sensor_device=0x3001U;
    sensor_config=0xCE10U;sensor_result=0x1234U;
}
static void Mailbox(uint32_t seq,uint32_t command,uint32_t argument)
{g_ambient_mailbox.command=command;g_ambient_mailbox.argument=argument;__DMB();g_ambient_mailbox.request_seq=seq;}

uint32_t Ambient_TestMain(void)
{
    Reset();
    CHECK(offsetof(BSP_Ambient_Diagnostics,phase)==48U && sizeof(Ambient_Mailbox)==124U);
    CHECK(offsetof(Ambient_Mailbox,driver)==48U && sizeof(Ambient_Snapshot)==128U);
    CHECK(BSP_Ambient_Probe(123U)==7U && hal_calls==0U);
    __disable_irq();uint32_t result=BSP_Ambient_Probe(100000U);__enable_irq();
    CHECK(result==9U && hal_calls==0U);
    CHECK(BSP_Ambient_Init()==0U && last_speed==400000U && reads==2U && writes==1U);
    CHECK(g_bsp_ambient.ready && !g_bsp_ambient.valid && g_bsp_ambient.enabled);
    CHECK(g_bsp_ambient.manufacturer==0x5449U && g_bsp_ambient.device==0x3001U);
    CHECK(g_bsp_ambient.configuration==0xCE10U && g_bsp_ambient.phase==BSP_AMBIENT_PHASE_ENABLE);
    tick+=200U;sensor_config|=0x80U;
    CHECK(BSP_Ambient_Poll()==0U && g_bsp_ambient.valid);
    CHECK(g_bsp_ambient.raw==0x1234U && g_bsp_ambient.millilux==11280U);
    uint32_t old_sample=g_bsp_ambient.sample_ms,old_samples=g_bsp_ambient.samples;
    tick+=200U;sensor_config&=~0x80U;g_bsp_ambient.error=2U;
    CHECK(BSP_Ambient_Poll()==0U && !g_bsp_ambient.error);
    CHECK(g_bsp_ambient.valid && g_bsp_ambient.sample_ms==old_sample && g_bsp_ambient.samples==old_samples);
    uint32_t confirmed=g_bsp_ambient.configuration,failures=g_bsp_ambient.failures;
    fail_phase=BSP_AMBIENT_PHASE_ENABLE;fail_status=HAL_BUSY;
    CHECK(BSP_Ambient_SetEnabled(0U)==3U);
    CHECK(!g_bsp_ambient.ready && !g_bsp_ambient.valid && g_bsp_ambient.enabled);
    CHECK(g_bsp_ambient.configuration==confirmed && g_bsp_ambient.failures==failures+1U);
    CHECK(g_bsp_ambient.hal_status==HAL_BUSY && g_bsp_ambient.elapsed_ms==20U);
    CHECK(g_bsp_ambient.hal_error==HAL_I2C_ERROR_TIMEOUT && g_bsp_ambient.sr1==0x200U);

    /* Failure stage evidence survives immediately; no later init stage runs. */
    for(uint32_t phase=BSP_AMBIENT_PHASE_DEINIT;phase<=BSP_AMBIENT_PHASE_ENABLE;++phase) {
        Reset();fail_phase=phase;
        CHECK(BSP_Ambient_Probe(100000U)==2U);
        CHECK(!g_bsp_ambient.ready && !g_bsp_ambient.valid && g_bsp_ambient.phase==phase);
        CHECK(g_bsp_ambient.sr1==I2C_SR1_ARLO && g_bsp_ambient.hal_error==HAL_I2C_ERROR_TIMEOUT);
        CHECK(g_bsp_ambient.elapsed_ms==20U && g_bsp_ambient.failures==1U);
        CHECK(hal_calls==phase);
    }
    Reset();fail_phase=BSP_AMBIENT_PHASE_MANUFACTURER;
    fail_sr1=I2C_SR1_AF;fail_error=HAL_I2C_ERROR_AF;
    CHECK(BSP_Ambient_Probe(100000U)==2U);
    CHECK(g_bsp_ambient.sr1==0x400U && g_bsp_ambient.hal_error==HAL_I2C_ERROR_AF);
    Reset();CHECK(BSP_Ambient_Probe(80000U)==0U && last_speed==80000U);
    CHECK(g_bsp_ambient.bus_hz==80000U && g_bsp_ambient.ready);
    CHECK(BSP_Ambient_Init()==0U && last_speed==400000U);
    Reset();sensor_device=0x9999U;
    CHECK(BSP_Ambient_Probe(100000U)==5U && !writes && !g_bsp_ambient.ready);
    CHECK(g_bsp_ambient.device==0x9999U && g_bsp_ambient.hal_status==HAL_OK);
    sensor_device=0x3001U;
    CHECK(BSP_Ambient_Probe(100000U)==0U && last_speed==100000U);
    g_bsp_ambient.valid=1U;fail_phase=BSP_AMBIENT_PHASE_MANUFACTURER;
    CHECK(BSP_Ambient_Probe(400000U)==2U && !g_bsp_ambient.valid && !g_bsp_ambient.ready);
    CHECK(!g_bsp_ambient.manufacturer && !g_bsp_ambient.device);
    fail_phase=0U;CHECK(BSP_Ambient_Probe(400000U)==0U);
    tick+=200U;sensor_config|=0x80U;sensor_result=0xF001U;
    CHECK(BSP_Ambient_Poll()==6U && !g_bsp_ambient.valid && g_bsp_ambient.ready);
    CHECK(BSP_Ambient_SetEnabled(0U)==0U && !g_bsp_ambient.enabled);
    uint32_t calls=hal_calls;tick+=300U;
    CHECK(BSP_Ambient_Poll()==0U && hal_calls==calls);

    /* Submission and snapshot never touch HAL; execution claims slot through
     * callback reentry and only publishes a complete result after all IO. */
    Reset();uint32_t id=0xDEADBEEFU;
    CHECK(AmbientService_RequestProbe(100000U,&id)==AMBIENT_NOT_INITIALIZED && id==0xDEADBEEFU);
    AmbientService_Init();CHECK(hal_calls==0U && g_ambient_service.pending_id==1U);
    CHECK(g_ambient_service.stale && !g_ambient_service.driver.valid);
    CHECK(AmbientService_RequestProbe(100000U,&id)==AMBIENT_BUSY && id==0xDEADBEEFU);
    CHECK(AmbientService_RequestProbe(200000U,&id)==AMBIENT_ARGUMENT);
    nested=1U;nested_phase=BSP_AMBIENT_PHASE_MANUFACTURER;
    AmbientService_Process(tick);
    CHECK(nested_status==AMBIENT_BUSY && nested_opid==0xAABBCCDDU && inside_id==1U);
    CHECK(!nested_snapshot.driver.ready && nested_snapshot.state==AMBIENT_STATE_RUNNING);
    CHECK(g_ambient_service.completed_id==1U && !g_ambient_service.completed_result);
    CHECK(g_ambient_service.state==AMBIENT_STATE_READY && !g_ambient_service.pending_id);
    calls=hal_calls;Ambient_Snapshot snapshot;AmbientService_GetSnapshot(&snapshot);
    CHECK(hal_calls==calls && snapshot.stale && !snapshot.driver.valid);
    CHECK(AmbientService_RequestProbe(100000U,&id)==AMBIENT_ACCEPTED && id==2U && hal_calls==calls);
    AmbientService_Process(tick);
    CHECK(last_speed==100000U && g_ambient_service.completed_id==id && !g_ambient_service.completed_result);
    CHECK(AmbientService_RequestEnabled(0U,&id)==AMBIENT_ACCEPTED);
    AmbientService_Process(tick);CHECK(g_ambient_service.state==AMBIENT_STATE_DISABLED && !g_bsp_ambient.enabled);
    calls=hal_calls;tick+=300U;AmbientService_Process(tick);
    CHECK(hal_calls==calls && g_ambient_service.state==AMBIENT_STATE_DISABLED);
    CHECK(AmbientService_RequestEnabled(1U,&id)==AMBIENT_ACCEPTED);
    AmbientService_Process(tick);tick+=200U;sensor_config|=0x80U;
    AmbientService_Process(tick);AmbientService_GetSnapshot(&snapshot);
    CHECK(snapshot.driver.valid && !snapshot.stale);
    tick+=AMBIENT_SAMPLE_STALE_MS+1U;AmbientService_GetSnapshot(&snapshot);
    CHECK(snapshot.stale && snapshot.driver.valid);
    sensor_config&=~0x80U;AmbientService_Process(tick);
    CHECK(g_ambient_service.stale && g_ambient_service.driver.valid);
    CHECK(g_ambient_service.sample_age_ms==HAL_GetTick()-g_bsp_ambient.sample_ms);
    __disable_irq();result=AmbientService_RequestEnabled(0U,&id);__enable_irq();
    CHECK(result==AMBIENT_CONTEXT);

    /* A failed initialization recovers with finite backoff; an explicit result
     * remains failure even if an automatic attempt later succeeds. */
    Reset();fail_phase=BSP_AMBIENT_PHASE_MANUFACTURER;AmbientService_Init();
    AmbientService_Process(tick);
    CHECK(g_ambient_service.state==AMBIENT_STATE_BACKOFF && g_ambient_service.completed_result==2U);
    CHECK(g_ambient_service.stale && !g_ambient_service.driver.valid);
    calls=hal_calls;tick=next_retry-1U;AmbientService_Process(tick);CHECK(hal_calls==calls);
    fail_phase=0U;tick=next_retry;AmbientService_Process(tick);
    CHECK(g_ambient_service.state==AMBIENT_STATE_READY && g_ambient_service.retries==1U);
    CHECK(g_ambient_service.completed_id==1U && g_ambient_service.completed_result==2U);
    Reset();fail_phase=BSP_AMBIENT_PHASE_MANUFACTURER;AmbientService_Init();AmbientService_Process(tick);
    for(uint32_t attempt=1U;attempt<=3U;++attempt) {
        tick=next_retry;AmbientService_Process(tick);
        CHECK(g_ambient_service.retries==attempt);
    }
    CHECK(g_ambient_service.state==AMBIENT_STATE_FAILED);
    calls=hal_calls;tick+=100000U;AmbientService_Process(tick);CHECK(calls==hal_calls);
    fail_phase=0U;CHECK(AmbientService_RequestProbe(100000U,&id)==AMBIENT_ACCEPTED);
    AmbientService_Process(tick);CHECK(g_ambient_service.state==AMBIENT_STATE_READY && !g_ambient_service.retries);

    /* Mailbox sequences commit after IO,freeze diagnostics across retries,and
     * reject unsupported operations without sending a single bus transaction. */
    Reset();AmbientService_Init();AmbientService_Process(tick);
    Mailbox(10U,AMBIENT_COMMAND_PROBE,100000U);
    nested=1U;nested_phase=BSP_AMBIENT_PHASE_INIT;expected_old_response=0U;
    fail_phase=BSP_AMBIENT_PHASE_MANUFACTURER;AmbientService_Process(tick);
    CHECK(g_ambient_mailbox.response_seq==10U && g_ambient_mailbox.operation_id==2U);
    CHECK(g_ambient_mailbox.request_status==AMBIENT_ACCEPTED && g_ambient_mailbox.result==2U);
    CHECK(g_ambient_mailbox.driver.sr1==I2C_SR1_ARLO && g_ambient_mailbox.driver.phase==BSP_AMBIENT_PHASE_MANUFACTURER);
    uint32_t frozen_ms=g_ambient_mailbox.completed_ms;
    fail_phase=0U;tick=next_retry;AmbientService_Process(tick);
    CHECK(g_ambient_mailbox.response_seq==10U && g_ambient_mailbox.result==2U && g_ambient_mailbox.completed_ms==frozen_ms);
    CHECK(g_ambient_mailbox.driver.sr1==I2C_SR1_ARLO && g_ambient_service.driver.sr1==0U);
    calls=hal_calls;Mailbox(11U,99U,0U);AmbientService_Process(tick);
    CHECK(g_ambient_mailbox.request_status==AMBIENT_ARGUMENT && g_ambient_mailbox.response_seq==11U);
    CHECK(g_ambient_mailbox.operation_id==0U && hal_calls==calls);
    CHECK(AmbientService_RequestEnabled(0U,&id)==AMBIENT_ACCEPTED);
    Mailbox(12U,AMBIENT_COMMAND_PROBE,400000U);AmbientService_Process(tick);
    CHECK(g_ambient_mailbox.request_status==AMBIENT_BUSY && g_ambient_mailbox.response_seq==12U);
    CHECK(g_ambient_service.completed_id==id && g_ambient_service.state==AMBIENT_STATE_DISABLED);
    calls=hal_calls;AmbientService_Process(tick);CHECK(hal_calls==calls);
    Mailbox(13U,AMBIENT_COMMAND_PROBE,400000U);AmbientService_Process(tick);
    CHECK(g_ambient_mailbox.response_seq==13U && !g_ambient_mailbox.result && last_speed==400000U);
    CHECK(!g_bsp_ambient.enabled && g_ambient_mailbox.driver.configuration==0xC810U);
    Mailbox(14U,AMBIENT_COMMAND_PROBE,80000U);AmbientService_Process(tick);
    CHECK(g_ambient_mailbox.response_seq==14U && !g_ambient_mailbox.result && last_speed==80000U);
    CHECK(g_ambient_mailbox.driver.bus_hz==80000U && g_ambient_service.requested_hz==80000U);
    /* ID-only command does not rewrite normal sensor config or change retry
     * policy. Its fresh evidence is separate and identified by the request. */
    Reset();AmbientService_Init();AmbientService_Process(tick);
    BSP_AmbientBitbang_Diagnostics diagnostic={0};
    CHECK(!AmbientService_GetIDDiagnosticSnapshot(&diagnostic));
    calls=hal_calls;uint32_t saved_writes=writes;
    Mailbox(21U,AMBIENT_COMMAND_ID_DIAGNOSTIC,0U);AmbientService_Process(tick);
    CHECK(bitbang_calls==1U && hal_calls==calls && writes==saved_writes);
    CHECK(g_ambient_mailbox.response_seq==21U && g_ambient_mailbox.result==0U);
    CHECK(AmbientService_GetIDDiagnosticSnapshot(&diagnostic));
    CHECK(diagnostic.operation_id==g_ambient_mailbox.operation_id && diagnostic.request_seq==21U);
    CHECK(g_ambient_service.state==AMBIENT_STATE_READY && requested_hz==400000U);
    g_bsp_ambient_bitbang.sequence|=1U;diagnostic.operation_id=12345U;
    CHECK(!AmbientService_GetIDDiagnosticSnapshot(&diagnostic) && diagnostic.operation_id==12345U);
    ++g_bsp_ambient_bitbang.sequence;
    Mailbox(22U,AMBIENT_COMMAND_ID_DIAGNOSTIC,2U);AmbientService_Process(tick);
    CHECK(g_ambient_mailbox.request_status==AMBIENT_ARGUMENT && bitbang_calls==1U);
    bitbang_result=BSP_ABB_RESTORE_FAILED;bitbang_restore=1U;
    CHECK(AmbientService_RequestIDDiagnostic(&id)==AMBIENT_ACCEPTED);
    AmbientService_Process(tick);
    CHECK(g_ambient_service.state==AMBIENT_STATE_FAILED && diagnostic_quarantine);
    CHECK(!g_bsp_ambient.ready && !g_bsp_ambient.valid && !next_retry);
    calls=hal_calls;tick+=100000U;AmbientService_Process(tick);CHECK(hal_calls==calls);
    CHECK(AmbientService_RequestEnabled(1U,&id)==AMBIENT_BUSY);
    CHECK(AmbientService_RequestEnabled(0U,&id)==AMBIENT_BUSY);
    CHECK(AmbientService_RequestIDDiagnostic(&id)==AMBIENT_BUSY);
    CHECK(AmbientService_RequestIDDiagnosticWithPullup(&id)==AMBIENT_BUSY);
    AmbientService_Process(tick);CHECK(hal_calls==calls && !next_retry);
    CHECK(AmbientService_RequestProbe(400000U,&id)==AMBIENT_ACCEPTED);
    AmbientService_Process(tick);
    CHECK(!diagnostic_quarantine && g_ambient_service.state==AMBIENT_STATE_READY);
    /* Separate upper APIs and mailbox argument must select precisely one
     * temporary variant; neither operation calls the normal HAL sensor API. */
    calls=hal_calls;
    CHECK(AmbientService_RequestIDDiagnosticWithPullup(&id)==AMBIENT_ACCEPTED);
    CHECK(hal_calls==calls);
    bitbang_result=0U;bitbang_restore=0U;AmbientService_Process(tick);
    CHECK(AmbientService_GetIDDiagnosticSnapshot(&diagnostic) && diagnostic.pullup_mode==1U);
    CHECK(diagnostic.operation_id==id && !diagnostic.request_seq && hal_calls==calls);
    CHECK(AmbientService_RequestIDDiagnostic(&id)==AMBIENT_ACCEPTED);
    AmbientService_Process(tick);
    CHECK(AmbientService_GetIDDiagnosticSnapshot(&diagnostic) && diagnostic.pullup_mode==0U);
    Mailbox(23U,AMBIENT_COMMAND_ID_DIAGNOSTIC,1U);AmbientService_Process(tick);
    CHECK(g_ambient_mailbox.response_seq==23U && g_ambient_mailbox.request_status==AMBIENT_ACCEPTED);
    CHECK(AmbientService_GetIDDiagnosticSnapshot(&diagnostic) && diagnostic.pullup_mode==1U &&
          diagnostic.request_seq==23U && diagnostic.operation_id==g_ambient_mailbox.operation_id);
    CHECK(hal_calls==calls && requested_hz==400000U && desired_enabled==1U);
    /* Fixed-candidate command is separately queued/correlated and cannot turn
     * a failed diagnostic into normal initialization or an implicit retry. */
    BSP_AmbientAddress_Diagnostics addresses={0};
    CHECK(!AmbientService_GetAddressDiagnosticSnapshot(&addresses));
    calls=hal_calls;uint32_t old_idseq=g_bsp_ambient_bitbang.sequence;
    CHECK(AmbientService_RequestAddressDiagnostic(&id)==AMBIENT_ACCEPTED);
    CHECK(!address_calls && hal_calls==calls);
    address_result=BSP_ABB_NACK;AmbientService_Process(tick);
    CHECK(address_calls==1U && hal_calls==calls && !diagnostic_quarantine);
    CHECK(AmbientService_GetAddressDiagnosticSnapshot(&addresses));
    CHECK(addresses.operation_id==id && !addresses.request_seq && addresses.result==BSP_ABB_NACK);
    CHECK(g_bsp_ambient_bitbang.sequence==old_idseq && requested_hz==400000U);
    addresses.operation_id=12345U;g_bsp_ambient_address.sequence|=1U;
    CHECK(!AmbientService_GetAddressDiagnosticSnapshot(&addresses) && addresses.operation_id==12345U);
    ++g_bsp_ambient_address.sequence;
    Mailbox(24U,AMBIENT_COMMAND_ADDRESS_DIAGNOSTIC,1U);AmbientService_Process(tick);
    CHECK(g_ambient_mailbox.request_status==AMBIENT_ARGUMENT && address_calls==1U);
    address_result=BSP_ABB_RESTORE_FAILED;address_restore=1U;
    Mailbox(25U,AMBIENT_COMMAND_ADDRESS_DIAGNOSTIC,0U);AmbientService_Process(tick);
    CHECK(g_ambient_mailbox.response_seq==25U && g_ambient_mailbox.result==BSP_ABB_RESTORE_FAILED);
    CHECK(AmbientService_GetAddressDiagnosticSnapshot(&addresses) && addresses.request_seq==25U);
    CHECK(addresses.operation_id==g_ambient_mailbox.operation_id && diagnostic_quarantine);
    CHECK(AmbientService_RequestAddressDiagnostic(&id)==AMBIENT_BUSY);
    CHECK(AmbientService_RequestIDDiagnostic(&id)==AMBIENT_BUSY);
    CHECK(AmbientService_RequestEnabled(1U,&id)==AMBIENT_BUSY);
    tick+=100000U;AmbientService_Process(tick);CHECK(hal_calls==calls && !next_retry);
    CHECK(AmbientService_RequestProbe(400000U,&id)==AMBIENT_ACCEPTED);
    AmbientService_Process(tick);CHECK(!diagnostic_quarantine);
    CHECK(!AmbientService_SetSleeping(1U)||!g_bsp_ambient.enabled);
    CHECK(AmbientService_SetSleeping(1U)&&!g_bsp_ambient.enabled);
    CHECK(AmbientService_SetSleeping(0U)&&g_bsp_ambient.enabled);
    CHECK(!g_mock_error);return 0U;
}
