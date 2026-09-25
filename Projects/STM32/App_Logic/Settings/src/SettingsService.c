#if !NOODOE_PRODUCT
#include "SettingsService.h"
#include "settings_record.h"
#include "StorageService.h"
#include "BSP_NOR.h"
#include "stm32f4xx_hal.h"
#include <string.h>

volatile Settings_Diagnostics g_settings;
static Settings_Record current,candidate,verified;
static uint8_t encoded[SETTINGS_RECORD_BYTES],readback[SETTINGS_RECORD_BYTES];
static uint32_t uid[3],next_request,last_attempt,attempted;
typedef struct {uint32_t kind,id;Settings_Values values;} SettingsRequest;
static SettingsRequest queued;
enum {REQUEST_PROVISION=1,REQUEST_PREFERENCES=2,REQUEST_ROLE=3,REQUEST_OIL_USAGE=4,REQUEST_RIDER_NAME=5};

/* Requests/getters only hold IRQ masking around fixed-size RAM copies. No NOR,
 * mutex wait or Bluetooth operation is performed inside a critical section. */
static uint32_t Enter(void){uint32_t mask=__get_PRIMASK();__disable_irq();return mask;}
static void Leave(uint32_t mask){__DMB();__set_PRIMASK(mask);}
static uint32_t TaskOkay(void){return !__get_IPSR() && !__get_PRIMASK() && !__get_BASEPRI();}
static void Publish(const Settings_Record *record)
{
    uint32_t mask=Enter();current=*record;g_settings.record_generation=record->generation;
    g_settings.key_generation=record->keys.generation;Leave(mask);
}
static Settings_Status Result(Settings_Status result)
{
    g_settings.last_result=result;return result;
}
Settings_Status SettingsService_Init(void)
{
    if(!TaskOkay())return SETTINGS_ARGUMENT;
    if(g_settings.initialized)return (Settings_Status)g_settings.last_result;
    BSP_NOR_DisableProvisionedStorage();memset(&current,0,sizeof(current));current.values.backlight_percent=25U;current.keys.version=1U;
    memset(&queued,0,sizeof(queued));next_request=last_attempt=attempted=0U;
    g_settings.magic=0x53455431U;g_settings.version=1U;g_settings.initialized=1U;g_settings.state=SETTINGS_STATE_UNPROVISIONED;
    uid[0]=HAL_GetUIDw0();uid[1]=HAL_GetUIDw1();uid[2]=HAL_GetUIDw2();
    FRESULT fr=StorageService_Init();g_settings.storage_result=fr;
    if(fr!=FR_OK && fr!=FR_NO_FILESYSTEM){g_settings.state=SETTINGS_STATE_ERROR;return Result(SETTINGS_STORAGE_ERROR);}
    uint32_t length=0U;fr=StorageService_NVMGet(readback,sizeof(readback),&length);g_settings.storage_result=fr;
    if(fr==FR_NO_FILE)return Result(SETTINGS_NOT_PROVISIONED);
    if(fr!=FR_OK){g_settings.state=SETTINGS_STATE_ERROR;return Result(SETTINGS_STORAGE_ERROR);}
    if(!SettingsRecord_Decode(&candidate,readback,length,uid))return Result(SETTINGS_INVALID_RECORD);
    /* A corrupted/unmounted FAT volume is not silently repaired merely because
     * NVM contains a valid marker. Keep writes disabled for explicit recovery. */
    if(!g_storage_service.mounted){g_settings.state=SETTINGS_STATE_ERROR;return Result(SETTINGS_STORAGE_ERROR);}
    if(Bluetooth_ImportKeys(&candidate.keys)!=BLUETOOTH_OK){g_settings.state=SETTINGS_STATE_ERROR;return Result(SETTINGS_BT_IMPORT_ERROR);}
    if(BSP_NOR_EnableProvisionedStorage(BSP_NOR_PROVISIONED_TOKEN)!=BSP_NOR_OK){g_settings.state=SETTINGS_STATE_ERROR;return Result(SETTINGS_DENIED);}
    Publish(&candidate);g_settings.provisioned=1U;g_settings.state=SETTINGS_STATE_READY;return Result(SETTINGS_OK);
}
static Settings_Status Queue(uint32_t kind,const Settings_Values *values,uint32_t *id)
{
    if(!id || !TaskOkay())return SETTINGS_ARGUMENT;
    if(!g_settings.initialized)return SETTINGS_NOT_READY;
    uint32_t mask=Enter();
    if(g_settings.pending_request){Leave(mask);return SETTINGS_BUSY;}
    if(++next_request==0U)++next_request;
    queued.kind=kind;queued.id=next_request;if(values)queued.values=*values;
    g_settings.pending_request=next_request;*id=next_request;Leave(mask);return SETTINGS_OK;
}
Settings_Status SettingsService_RequestProvision(uint32_t token,uint32_t *id)
{
    if(token!=SETTINGS_PROVISION_TOKEN || !BSP_NOR_IsStorageUnlocked() || !g_storage_service.mounted || !g_storage_service.formats)return SETTINGS_DENIED;
    return Queue(REQUEST_PROVISION,NULL,id);
}
Settings_Status SettingsService_RequestPreferences(uint32_t backlight,uint32_t units,uint32_t *id)
{
    if(backlight>100U || units>1U)return SETTINGS_ARGUMENT;
    if(!g_settings.provisioned)return SETTINGS_NOT_PROVISIONED;
    Settings_Values values;memset(&values,0,sizeof(values));values.backlight_percent=backlight;values.units=units;
    return Queue(REQUEST_PREFERENCES,&values,id);
}
Settings_Status SettingsService_RequestRoleBinding(Bluetooth_Role role,const uint8_t address[6],uint8_t channel,uint8_t enabled,uint32_t *id)
{
    if(role!=BLUETOOTH_ELM || enabled>1U || channel>30U || (enabled && !address))return SETTINGS_ARGUMENT;
    if(!g_settings.provisioned)return SETTINGS_NOT_PROVISIONED;
    Settings_Values values;memset(&values,0,sizeof(values));
    /* units temporarily encodes the selected role; worker changes only this
     * binding, so a stale request copy cannot overwrite another saved field. */
    values.units=(uint32_t)role-1U;Settings_RoleBinding *binding=&values.roles[0];
    if(enabled){memcpy(binding->address,address,6U);binding->channel=channel;binding->enabled=1U;}
    Settings_Values validate=values;validate.units=0U;
    if(!SettingsRecord_ValuesValid(&validate))return SETTINGS_ARGUMENT;
    return Queue(REQUEST_ROLE,&values,id);
}
/* Worker merges just usage fields, so simultaneous preference/BT changes
 * cannot be overwritten by a stale full-value snapshot from the I/O task. */
Settings_Status SettingsService_RequestOilUsage(uint64_t on_ms,uint32_t *id)
{
    if(!g_settings.provisioned)return SETTINGS_NOT_PROVISIONED;
    Settings_Values values={0};values.oil_usage_valid=1U;values.oil_on_ms=on_ms;
    return Queue(REQUEST_OIL_USAGE,&values,id);
}
Settings_Status SettingsService_GetValues(Settings_Values *out)
{
    if(!out)return SETTINGS_ARGUMENT;
    uint32_t mask=Enter();*out=current.values;uint32_t valid=g_settings.provisioned;Leave(mask);
    return valid?SETTINGS_OK:SETTINGS_NOT_PROVISIONED;
}
/* Packet length is checked before copying; this request owns its own string
 * until StorageTask commits it. Other queued preference fields are not copied
 * back, so a phone rename cannot undo oil checkpoints or BT key changes. */
Settings_Status SettingsService_RequestRiderName(const char *text,uint32_t bytes,uint32_t *id)
{
    if(!RiderName_Validate(text,bytes))return SETTINGS_ARGUMENT;
    if(!g_settings.provisioned)return SETTINGS_NOT_PROVISIONED;
    Settings_Values values={0};if(bytes)memcpy(values.rider_name,text,bytes);
    return Queue(REQUEST_RIDER_NAME,&values,id);
}
Settings_Status SettingsService_GetRiderName(char *out,uint32_t capacity)
{
    if(!out||capacity<RIDER_NAME_CAPACITY)return SETTINGS_ARGUMENT;
    uint32_t mask=Enter();memcpy(out,current.values.rider_name,RIDER_NAME_CAPACITY);
    uint32_t valid=g_settings.provisioned;Leave(mask);
    return valid?SETTINGS_OK:SETTINGS_NOT_PROVISIONED;
}
void SettingsService_GetDiagnostics(Settings_Diagnostics *out)
{
    if(!out)return;
    uint32_t mask=Enter();memcpy(out,(const void*)&g_settings,sizeof(*out));Leave(mask);
}
/* NVM already verifies physical writes and commits a rotating journal last.
 * This extra copy/format validation checks the service-level record contract
 * before marking the BT generation clean or publishing new settings. */
static Settings_Status Save(void)
{
    candidate.generation=current.generation+1U;
    if(!SettingsRecord_Encode(encoded,&candidate,uid))return SETTINGS_INVALID_RECORD;
    FRESULT fr=StorageService_NVMPut(encoded,sizeof(encoded));g_settings.storage_result=fr;
    if(fr!=FR_OK)return SETTINGS_STORAGE_ERROR;
    uint32_t length=0U;fr=StorageService_NVMGet(readback,sizeof(readback),&length);g_settings.storage_result=fr;
    if(fr!=FR_OK || length!=sizeof(encoded) || memcmp(encoded,readback,sizeof(encoded)) || !SettingsRecord_Decode(&verified,readback,length,uid))return SETTINGS_STORAGE_ERROR;
    Publish(&verified);Bluetooth_MarkKeysPersisted(verified.keys.generation);++g_settings.writes;return SETTINGS_OK;
}
void SettingsService_Process(uint32_t now_ms)
{
    if(!g_settings.initialized || !TaskOkay())return;
    uint32_t mask=Enter();SettingsRequest request=queued;uint32_t pending=g_settings.pending_request;Leave(mask);
    if(!pending && !g_settings.provisioned)return;
    if(!pending && attempted && now_ms-last_attempt<1000U)return;
    candidate=current;
    if(Bluetooth_ExportKeys(&candidate.keys)!=BLUETOOTH_OK){if(!pending)return;candidate.keys=current.keys;}
    if(!pending && candidate.keys.generation==current.keys.generation)return;
    attempted=1U;last_attempt=now_ms;g_settings.state=SETTINGS_STATE_SAVING;
    Settings_Status result=SETTINGS_OK;uint32_t format_scope=0U;
    if(pending && request.kind==REQUEST_PROVISION){
        /* Recheck after queue delay. BeginFormat makes the entire provisioning
         * journal write host-only too; USB reset cannot fall back to an already
         * existing runtime permit halfway through a new marker commit. */
        if(!BSP_NOR_IsStorageUnlocked() || !g_storage_service.mounted || !g_storage_service.formats || BSP_NOR_BeginFormat()!=BSP_NOR_OK)result=SETTINGS_DENIED;
        else format_scope=1U;
    }else if(!g_settings.provisioned || !BSP_NOR_CanWriteStorage())result=SETTINGS_DENIED;
    if(result==SETTINGS_OK && pending){
        if(request.kind==REQUEST_PREFERENCES){candidate.values.backlight_percent=request.values.backlight_percent;candidate.values.units=request.values.units;}
        else if(request.kind==REQUEST_ROLE)candidate.values.roles[request.values.units]=request.values.roles[0];
        else if(request.kind==REQUEST_OIL_USAGE){candidate.values.oil_on_ms=request.values.oil_on_ms;candidate.values.oil_usage_valid=1U;}
        else if(request.kind==REQUEST_RIDER_NAME)memcpy(candidate.values.rider_name,request.values.rider_name,RIDER_NAME_CAPACITY);
    }
    if(result==SETTINGS_OK)result=Save();
    if(format_scope){BSP_NOR_EndFormat();
        if(result==SETTINGS_OK && BSP_NOR_EnableProvisionedStorage(BSP_NOR_PROVISIONED_TOKEN)!=BSP_NOR_OK)result=SETTINGS_DENIED;
        if(result==SETTINGS_OK)g_settings.provisioned=1U;
    }
    if(result!=SETTINGS_OK)++g_settings.write_failures;
    g_settings.last_result=result;g_settings.state=g_settings.provisioned?(result==SETTINGS_OK?SETTINGS_STATE_READY:SETTINGS_STATE_ERROR):SETTINGS_STATE_UNPROVISIONED;
    if(pending){mask=Enter();g_settings.completed_request=request.id;g_settings.completed_result=result;g_settings.pending_request=0U;memset(&queued,0,sizeof(queued));Leave(mask);}
}

#endif
