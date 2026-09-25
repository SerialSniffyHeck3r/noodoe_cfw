#if NOODOE_PRODUCT
#include "SettingsService.h"
#include "Bluetooth_KeyCodec.h"
#include "Config_Store.h"
#include "App_Persistence.h"
#include "stm32f4xx_hal.h"
#include <string.h>
volatile Settings_Diagnostics g_settings;
static Settings_Values current;
static Bluetooth_KeyStore keys;
static uint32_t pending_revision,key_revision,key_generation,next_id,request_building;
static uint32_t Lock(void) {
    uint32_t m=__get_PRIMASK();
    __disable_irq();
    return m;
}
static void Unlock(uint32_t m) {
    __DMB();
    __set_PRIMASK(m);
}
Settings_Status SettingsService_Init(void)
{
    if(g_settings.initialized)return (Settings_Status)g_settings.last_result;
    if(!g_config_store.ready)return SETTINGS_NOT_READY;
    memset(&current,0,sizeof(current));
    current.backlight_percent=25;
    memset(&keys,0,sizeof(keys));
    keys.version=1;
    g_settings.magic=0x53455431;
    g_settings.version=2;
    g_settings.initialized=1;
    if(g_config_store.error) {
        g_settings.state=SETTINGS_STATE_ERROR;
        g_settings.last_result=SETTINGS_STORAGE_ERROR;
        return SETTINGS_STORAGE_ERROR;
    }
    uint8_t b[152];
    uint32_t n=0,e=ConfigStore_Get(CONFIG_FIELD_BT_KEYS,b,sizeof(b),&n);
    if(!e&&!Bluetooth_DecodeKeys(&keys,b,n))goto invalid;
    if(e&&e!=CFW_MISSING)goto invalid;
    e=ConfigStore_Get(CONFIG_FIELD_RIDER,b,sizeof(b),&n);
    if(!e) {
        if(!RiderName_Validate((const char*)b,n))goto invalid;
        memcpy(current.rider_name,b,n);
    }
    else if(e!=CFW_MISSING)goto invalid;
    e=ConfigStore_Get(CONFIG_FIELD_ELM,b,sizeof(b),&n);
    if(!e) {
        if(n!=8||b[6]>30||b[7]>1)goto invalid;
        memcpy(current.roles[0].address,b,6);
        current.roles[0].channel=b[6];
        current.roles[0].enabled=b[7];
    }
    else if(e!=CFW_MISSING)goto invalid;
    if(!ConfigStore_Get(0x1002,b,4,&n)&&n==4&&Cfw_Get32(b)<=100)current.backlight_percent=Cfw_Get32(b);
    if(!ConfigStore_Get(0x1011,b,4,&n)&&n==4&&Cfw_Get32(b)<=1)current.units=Cfw_Get32(b);
    if(Bluetooth_ImportKeys(&keys)!=BLUETOOTH_OK) {
        g_settings.last_result=SETTINGS_BT_IMPORT_ERROR;
        return SETTINGS_BT_IMPORT_ERROR;
    }
    g_settings.provisioned=1;
    g_settings.state=SETTINGS_STATE_READY;
    g_settings.last_result=0;
    return SETTINGS_OK;
    invalid:
    ConfigStore_Reject(CFW_CORRUPT);
    g_settings.state=SETTINGS_STATE_ERROR;
    g_settings.last_result=SETTINGS_INVALID_RECORD;
    return SETTINGS_INVALID_RECORD;
}
static Settings_Status Begin(uint32_t *id)
{
    if(!id||__get_IPSR())return SETTINGS_ARGUMENT;
    if(!g_settings.provisioned)return SETTINGS_NOT_PROVISIONED;
    uint32_t m=Lock();
    if(g_settings.pending_request||request_building) {
        Unlock(m);
        return SETTINGS_BUSY;
    }
    request_building=1;
    Unlock(m);
    return SETTINGS_OK;
}
static Settings_Status Rejected(Settings_Status result)
{
    uint32_t m=Lock();
    request_building=0;
    Unlock(m);
    return result;
}
static Settings_Status Accepted(uint32_t revision,uint32_t *id)
{
    uint32_t m=Lock();
    if(!++next_id)++next_id;
    *id=next_id;
    g_settings.pending_request=*id;
    pending_revision=revision;
    g_settings.state=SETTINGS_STATE_SAVING;
    request_building=0;
    Unlock(m);
    return SETTINGS_OK;
}
Settings_Status SettingsService_RequestProvision(uint32_t token,uint32_t *id)
{
    (void)token;
    (void)id;
    return SETTINGS_DENIED;
    /* Only the audited host installer creates files. */
}
Settings_Status SettingsService_RequestPreferences(uint32_t backlight,uint32_t units,uint32_t *id)
{
    if(backlight>100||units>1)return SETTINGS_ARGUMENT;
    Settings_Status e=Begin(id);
    if(e)return e;
    const uint16_t fields[2]={0x1002,0x1011};
    const uint32_t values[2]={backlight,units};uint32_t r;
    if(ConfigStore_SetWords(fields,values,2,&r))return Rejected(SETTINGS_STORAGE_ERROR);
    uint32_t m=Lock();
    current.backlight_percent=backlight;
    current.units=units;
    Unlock(m);
    return Accepted(r,id);
}
Settings_Status SettingsService_RequestRiderName(const char *text,uint32_t bytes,uint32_t *id)
{
    if(!RiderName_Validate(text,bytes))return SETTINGS_ARGUMENT;
    Settings_Status e=Begin(id);
    if(e)return e;
    uint32_t r;
    if(ConfigStore_Set(CONFIG_FIELD_RIDER,text?text:"",bytes,&r))return Rejected(SETTINGS_STORAGE_ERROR);
    uint32_t m=Lock();
    memset(current.rider_name,0,sizeof(current.rider_name));
    if(bytes)memcpy(current.rider_name,text,bytes);
    Unlock(m);
    (void)RiderName_Set(text,bytes);
    return Accepted(r,id);
}
Settings_Status SettingsService_RequestRoleBinding(Bluetooth_Role role,const uint8_t address[6],uint8_t channel,uint8_t enabled,uint32_t *id)
{
    if(role!=BLUETOOTH_ELM||channel>30||enabled>1||(enabled&&!address))return SETTINGS_ARGUMENT;
    Settings_Status e=Begin(id);
    if(e)return e;
    uint8_t b[8]= {
        0
    };
    if(enabled) {
        memcpy(b,address,6);
        b[6]=channel;
        b[7]=1;
    }
    uint32_t nonzero=0,notff=0;
    for(uint32_t i=0;i<6;++i) {
        nonzero|=b[i];
        notff|=b[i]^255;
    }
    if(enabled&&(!nonzero||!notff))return Rejected(SETTINGS_ARGUMENT);
    uint32_t r;
    if(ConfigStore_Set(CONFIG_FIELD_ELM,b,8,&r))return Rejected(SETTINGS_STORAGE_ERROR);
    uint32_t m=Lock();
    memcpy(current.roles[0].address,b,6);
    current.roles[0].channel=b[6];
    current.roles[0].enabled=b[7];
    Unlock(m);
    return Accepted(r,id);
}
Settings_Status SettingsService_RequestOilUsage(uint64_t ms,uint32_t *id)
{
    (void)ms;
    (void)id;
    return SETTINGS_DENIED;
    /* Lifetime usage now belongs to RideStore, never CFG. */
}
Settings_Status SettingsService_GetValues(Settings_Values *out)
{
    if(!out)return SETTINGS_ARGUMENT;
    uint32_t m=Lock();
    *out=current;
    Unlock(m);
    uint8_t b[4];
    uint32_t n;
    if(!ConfigStore_Get(0x1002,b,4,&n)&&n==4)out->backlight_percent=Cfw_Get32(b);
    if(!ConfigStore_Get(0x1011,b,4,&n)&&n==4)out->units=Cfw_Get32(b);
    uint64_t base,saved;
    uint32_t known;
    if(RideStore_GetUsage(&base,&saved,&known)) {
        out->oil_on_ms=base;
        out->oil_usage_valid=known;
    }
    return g_settings.provisioned?SETTINGS_OK:SETTINGS_NOT_PROVISIONED;
}
Settings_Status SettingsService_GetRiderName(char *out,uint32_t capacity)
{
    if(!out||capacity<RIDER_NAME_CAPACITY)return SETTINGS_ARGUMENT;
    uint32_t m=Lock();
    memcpy(out,current.rider_name,RIDER_NAME_CAPACITY);
    Unlock(m);
    return g_settings.provisioned?SETTINGS_OK:SETTINGS_NOT_PROVISIONED;
}
void SettingsService_GetDiagnostics(Settings_Diagnostics *out)
{
    if(!out)return;
    uint32_t m=Lock();
    memcpy(out,(const void*)&g_settings,sizeof(*out));
    Unlock(m);
}
void SettingsService_Process(uint32_t now)
{
    (void)now;
    if(!g_settings.initialized) {
        (void)SettingsService_Init();
        return;
    }
    if(!g_settings.provisioned)return;
    if(g_settings.pending_request) {
        uint32_t e=ConfigStore_Result(pending_revision);
        if(e!=CFW_PENDING) {
            uint32_t m=Lock();
            g_settings.completed_request=g_settings.pending_request;
            g_settings.pending_request=0;
            g_settings.completed_result=e?SETTINGS_STORAGE_ERROR:0;
            g_settings.last_result=g_settings.completed_result;
            g_settings.state=e?SETTINGS_STATE_ERROR:SETTINGS_STATE_READY;
            if(e)++g_settings.write_failures;
            Unlock(m);
        }
    }
    if(key_revision&&ConfigStore_Result(key_revision)==CFW_OK) {
        Bluetooth_MarkKeysPersisted(key_generation);
        key_revision=0;
    }
    Bluetooth_KeyStore latest;
    if(Bluetooth_ExportKeys(&latest)==BLUETOOTH_OK&&latest.generation!=keys.generation) {
        uint8_t b[152];
        Bluetooth_EncodeKeys(b,&latest);
        uint32_t r;
        if(!ConfigStore_Set(CONFIG_FIELD_BT_KEYS,b,sizeof(b),&r)) {
            keys=latest;
            key_revision=r;
            key_generation=latest.generation;
        }
    }
    g_settings.record_generation=g_cfw_store.files[0].generation;
    g_settings.writes=g_cfw_store.files[0].writes;
    g_settings.key_generation=keys.generation;
}
#endif
