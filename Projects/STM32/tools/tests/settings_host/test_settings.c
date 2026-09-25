/* Actual settings worker+codec; underlying committed journal behavior is
 * mocked here and independently tested by storage_journal_host power cuts. */
#include <stddef.h>
#include "SettingsService.c"
#include "settings_record.c"
#include "rider_name.c"
volatile uint32_t g_assertions,g_failure_line,g_mock_error;
volatile StorageService_Diagnostics g_storage_service;
static uint8_t media[SETTINGS_RECORD_BYTES],original[SETTINGS_RECORD_BYTES];
static uint32_t media_length,host_gate,runtime_gate,format_scope_mock,puts,imports,marks;
static uint32_t fail_put,key_race,reset_put;
static Bluetooth_KeyStore live_keys;
#define CHECK(x) do{++g_assertions;if(!(x)){g_failure_line=__LINE__;return 1;}}while(0)
void *memcpy(void *dest,const void *src,size_t n){uint8_t *d=dest;const uint8_t *s=src;for(size_t i=0;i<n;++i)d[i]=s[i];return dest;}
void *memset(void *dest,int v,size_t n){uint8_t *d=dest;for(size_t i=0;i<n;++i)d[i]=(uint8_t)v;return dest;}
int memcmp(const void *a,const void *b,size_t n){const uint8_t *x=a,*y=b;for(size_t i=0;i<n;++i)if(x[i]!=y[i])return x[i]-y[i];return 0;}
FRESULT StorageService_Init(void){return g_storage_service.mounted?FR_OK:FR_NO_FILESYSTEM;}
FRESULT StorageService_NVMGet(void *out,uint32_t capacity,uint32_t *received)
{
    *received=0U;if(!media_length)return FR_NO_FILE;
    if(capacity<media_length)return FR_INVALID_PARAMETER;
    memcpy(out,media,media_length);*received=media_length;return FR_OK;
}
FRESULT StorageService_NVMPut(const void *data,uint32_t length)
{
    ++puts;
    if(!BSP_NOR_CanWriteStorage() || length!=sizeof(media)){g_mock_error=1U;return FR_WRITE_PROTECTED;}
    if(reset_put){host_gate=0U;return FR_DISK_ERR;}
    if(fail_put)return FR_DISK_ERR;
    memcpy(media,data,length);media_length=length;
    if(key_race){key_race=0U;++live_keys.generation;}
    return FR_OK;
}
void BSP_NOR_DisableProvisionedStorage(void){runtime_gate=0U;}
BSP_NOR_Status BSP_NOR_EnableProvisionedStorage(uint32_t token){if(token!=BSP_NOR_PROVISIONED_TOKEN)return BSP_NOR_LOCKED;runtime_gate=1U;return BSP_NOR_OK;}
uint32_t BSP_NOR_IsStorageUnlocked(void){return host_gate;}
uint32_t BSP_NOR_CanWriteStorage(void){return format_scope_mock?host_gate:(host_gate || runtime_gate);}
BSP_NOR_Status BSP_NOR_BeginFormat(void){if(!host_gate)return BSP_NOR_LOCKED;format_scope_mock=1U;return BSP_NOR_OK;}
void BSP_NOR_EndFormat(void){format_scope_mock=0U;}
int Bluetooth_ImportKeys(const Bluetooth_KeyStore *keys){++imports;live_keys=*keys;return BLUETOOTH_OK;}
int Bluetooth_ExportKeys(Bluetooth_KeyStore *keys){*keys=live_keys;return BLUETOOTH_OK;}
void Bluetooth_MarkKeysPersisted(uint32_t generation){if(live_keys.generation==generation)marks=generation;}
static Settings_Status Reboot(void)
{
    memset((void*)&g_settings,0,sizeof(g_settings));host_gate=runtime_gate=0U;g_storage_service.formats=0U;
    return SettingsService_Init();
}
unsigned OilUsage_Test(void);
int Settings_TestMain(void)
{
    g_storage_service.mounted=1U;live_keys.version=1U;live_keys.generation=2U;
    CHECK(Reboot()==SETTINGS_NOT_PROVISIONED);CHECK(puts==0U && imports==0U && !runtime_gate);
    SettingsService_Process(0U);CHECK(puts==0U);
    Settings_Values values;CHECK(SettingsService_GetValues(&values)==SETTINGS_NOT_PROVISIONED);CHECK(values.backlight_percent==25U);
    uint32_t id=0U,another=0U;
    CHECK(SettingsService_RequestProvision(SETTINGS_PROVISION_TOKEN,&id)==SETTINGS_DENIED);
    host_gate=1U;CHECK(SettingsService_RequestProvision(SETTINGS_PROVISION_TOKEN,&id)==SETTINGS_DENIED);
    g_storage_service.formats=1U;
    CHECK(SettingsService_RequestProvision(0U,&id)==SETTINGS_DENIED);
    CHECK(SettingsService_RequestProvision(SETTINGS_PROVISION_TOKEN,&id)==SETTINGS_OK);
    CHECK(SettingsService_RequestProvision(SETTINGS_PROVISION_TOKEN,&another)==SETTINGS_BUSY);
    host_gate=0U;SettingsService_Process(1U);CHECK(g_settings.completed_request==id && g_settings.completed_result==SETTINGS_DENIED);CHECK(puts==0U);
    host_gate=1U;CHECK(SettingsService_RequestProvision(SETTINGS_PROVISION_TOKEN,&id)==SETTINGS_OK);
    reset_put=1U;SettingsService_Process(2U);reset_put=0U;
    CHECK(!runtime_gate && !g_settings.provisioned && !media_length && !format_scope_mock);
    host_gate=1U;CHECK(SettingsService_RequestProvision(SETTINGS_PROVISION_TOKEN,&id)==SETTINGS_OK);
    SettingsService_Process(3U);CHECK(g_settings.provisioned && runtime_gate && !format_scope_mock);CHECK(marks==2U);
    CHECK(g_settings.completed_request==id && g_settings.completed_result==SETTINGS_OK);
    memcpy(original,media,sizeof(media));uint32_t put_count=puts;
    CHECK(Reboot()==SETTINGS_OK);CHECK(puts==put_count && imports==1U && runtime_gate && !host_gate);
    CHECK(SettingsService_RequestProvision(SETTINGS_PROVISION_TOKEN,&id)==SETTINGS_DENIED);
    CHECK(SettingsService_RequestPreferences(101U,0U,&id)==SETTINGS_ARGUMENT);
    CHECK(SettingsService_RequestPreferences(40U,1U,&id)==SETTINGS_OK);
    SettingsService_Process(5U);CHECK(g_settings.completed_result==SETTINGS_OK);
    CHECK(SettingsService_GetValues(&values)==SETTINGS_OK && values.backlight_percent==40U && values.units==1U);
    /* Phone buffers are borrowed only until acceptance. Invalid bytes and a
     * failed commit must never corrupt the old name or unrelated settings. */
    char name[RIDER_NAME_CAPACITY],packet[]="Alex";
    CHECK(RiderName_Get(name,sizeof(name))&&!name[0]);
    CHECK(SettingsService_RequestRiderName(packet,4,&id)==SETTINGS_OK);packet[0]='X';
    CHECK(SettingsService_RequestRiderName("Other",5,&another)==SETTINGS_BUSY);
    CHECK(RiderName_Get(name,sizeof(name))&&!name[0]);SettingsService_Process(6U);
    CHECK(g_settings.completed_result==SETTINGS_OK&&RiderName_Get(name,sizeof(name))&&!memcmp(name,"Alex",5));
    CHECK(Reboot()==SETTINGS_OK&&RiderName_Get(name,sizeof(name))&&!memcmp(name,"Alex",5));
    CHECK(SettingsService_RequestRiderName("Next",4,&id)==SETTINGS_OK);
    fail_put=1;SettingsService_Process(6U);fail_put=0;
    CHECK(g_settings.completed_result==SETTINGS_STORAGE_ERROR&&RiderName_Get(name,sizeof(name))&&!memcmp(name,"Alex",5));
    CHECK(SettingsService_RequestRiderName("A\nB",3,&id)==SETTINGS_ARGUMENT);
    CHECK(SettingsService_RequestRiderName("A\0B",3,&id)==SETTINGS_ARGUMENT);
    const char korean[]="\xED\x99\x8D\xEA\xB8\xB8\xEB\x8F\x99";
    CHECK(SettingsService_RequestRiderName(korean,sizeof(korean)-1U,&id)==SETTINGS_OK);
    SettingsService_Process(6U);CHECK(Reboot()==SETTINGS_OK);
    CHECK(RiderName_Get(name,sizeof(name))&&!memcmp(name,korean,sizeof(korean)));
    const char *bad[]={"\xC0\xAF","\xED\xA0\x80","\xF4\x90\x80\x80","\x80","\xE2\x80\xA8","\xC2\x80"};
    const uint32_t lengths[]={2,3,4,1,3,2};
    for(uint32_t i=0;i<6;++i)CHECK(!RiderName_Validate(bad[i],lengths[i]));
    CHECK(!RiderName_Validate(korean,2)&&!RiderName_Validate(NULL,1));
    char maximum[RIDER_NAME_CAPACITY];memset(maximum,'W',sizeof(maximum));
    CHECK(!RiderName_Validate(maximum,sizeof(maximum)));
    CHECK(SettingsService_RequestRiderName(maximum,RIDER_NAME_MAX_BYTES,&id)==SETTINGS_OK);
    SettingsService_Process(6U);CHECK(RiderName_Get(name,sizeof(name))&&!name[RIDER_NAME_MAX_BYTES]);
    CHECK(!memcmp(name,maximum,RIDER_NAME_MAX_BYTES));
    CHECK(SettingsService_RequestRiderName(NULL,0,&id)==SETTINGS_OK);SettingsService_Process(6U);
    CHECK(RiderName_Get(name,sizeof(name))&&!name[0]);
    uint8_t address[6]={1,2,3,4,5,6};
    CHECK(SettingsService_RequestRoleBinding(BLUETOOTH_PHONE,address,0U,1U,&id)==SETTINGS_ARGUMENT);
    CHECK(SettingsService_RequestRoleBinding(BLUETOOTH_ELM,address,31U,1U,&id)==SETTINGS_ARGUMENT);
    CHECK(SettingsService_RequestRoleBinding(BLUETOOTH_ELM,address,0U,1U,&id)==SETTINGS_OK);
    SettingsService_Process(7U);CHECK(SettingsService_GetValues(&values)==SETTINGS_OK);
    CHECK(!memcmp(values.roles[0].address,address,6U) && values.roles[0].enabled && values.backlight_percent==40U);
    /* Failed next journal commit cannot publish settings or mark a newer BT
     * generation saved. Its previously committed record survives reboot. */
    CHECK(SettingsService_RequestPreferences(70U,0U,&id)==SETTINGS_OK);
    fail_put=1U;SettingsService_Process(10U);fail_put=0U;
    CHECK(g_settings.completed_result==SETTINGS_STORAGE_ERROR);
    CHECK(SettingsService_GetValues(&values)==SETTINGS_OK && values.backlight_percent==40U);
    CHECK(Reboot()==SETTINGS_OK);CHECK(SettingsService_GetValues(&values)==SETTINGS_OK && values.backlight_percent==40U);
    live_keys.generation=3U;key_race=1U;SettingsService_Process(20U);
    CHECK(current.keys.generation==3U && live_keys.generation==4U && marks==2U);
    put_count=puts;SettingsService_Process(21U);CHECK(puts==put_count);
    SettingsService_Process(1020U);CHECK(current.keys.generation==4U && marks==4U);
    /* Usage checkpoints preserve preferences/keys; an interrupted write cannot
     * advance the committed hour counter. A v1 record restores with unknown hours. */
    CHECK(SettingsService_RequestOilUsage(3600001ULL,&id)==SETTINGS_OK);
    SettingsService_Process(2020U);CHECK(g_settings.completed_result==SETTINGS_OK);
    CHECK(SettingsService_GetValues(&values)==SETTINGS_OK&&values.oil_usage_valid&&values.oil_on_ms==3600001ULL);
    CHECK(values.backlight_percent==40U&&values.roles[0].enabled);
    CHECK(SettingsService_RequestOilUsage(7200000ULL,&id)==SETTINGS_OK);
    fail_put=1U;SettingsService_Process(3020U);fail_put=0U;
    CHECK(g_settings.completed_result==SETTINGS_STORAGE_ERROR);
    CHECK(Reboot()==SETTINGS_OK);SettingsService_GetValues(&values);CHECK(values.oil_on_ms==3600001ULL);
    CHECK(OilUsage_Test()==0U);
    CHECK(SettingsService_GetValues(&values)==SETTINGS_OK);uint64_t saved_usage=values.oil_on_ms;
    memcpy(original,media,sizeof(media));
    /* Schema2 also loads with empty name and retains usage; no boot rewrite. */
    Put(media+4U,2U);Put(media+8U,SETTINGS_RECORD_V2_BYTES);Put(media+256U,Crc(media,256U));media_length=SETTINGS_RECORD_V2_BYTES;
    put_count=puts;CHECK(Reboot()==SETTINGS_OK);SettingsService_GetValues(&values);
    CHECK(!values.rider_name[0]&&values.oil_on_ms==saved_usage&&puts==put_count);
    memcpy(media,original,sizeof(media));
    Put(media+4U,1U);Put(media+8U,244U);Put(media+240U,Crc(media,240U));media_length=244U;
    put_count=puts;CHECK(Reboot()==SETTINGS_OK);SettingsService_GetValues(&values);
    CHECK(!values.oil_usage_valid&&values.oil_on_ms==0U&&puts==put_count);
    CHECK(values.backlight_percent==40U&&values.roles[0].enabled);
    memcpy(media,original,sizeof(media));media_length=sizeof(media);CHECK(Reboot()==SETTINGS_OK);
    /* Every stored byte is covered by header/CRC validation. Invalid input
     * must leave the output object untouched, not partially import keys. */
    Settings_Record decoded,unchanged;memset(&unchanged,0xA5,sizeof(unchanged));
    uint32_t current_uid[3]={HAL_GetUIDw0(),HAL_GetUIDw1(),HAL_GetUIDw2()};
    for(uint32_t i=0;i<sizeof(media);++i){uint8_t byte=media[i];media[i]^=1U;decoded=unchanged;
        CHECK(!SettingsRecord_Decode(&decoded,media,sizeof(media),current_uid));
        CHECK(!memcmp(&decoded,&unchanged,sizeof(decoded)));media[i]=byte;
    }
    CHECK(!SettingsRecord_Decode(&decoded,media,sizeof(media)-1U,current_uid));
    CHECK(!SettingsRecord_Decode(&decoded,media,sizeof(media)+1U,current_uid));
    ++current_uid[2];CHECK(!SettingsRecord_Decode(&decoded,media,sizeof(media),current_uid));--current_uid[2];
    /* Recomputed CRC cannot authorize another schema, UID, partition layout,
     * flag, invalid key type or out-of-range brightness. */
    const uint32_t bad_offsets[]={4U,12U,16U,28U,32U,36U,40U,44U,48U,52U,56U,64U,68U,88U,118U,119U,248U,252U,256U,304U,305U};
    memcpy(original,media,sizeof(media));
    for(uint32_t i=0;i<sizeof(bad_offsets)/sizeof(bad_offsets[0]);++i){
        memcpy(media,original,sizeof(media));media[bad_offsets[i]]=0xFEU;Put(media+308U,Crc(media,308U));
        CHECK(!SettingsRecord_Decode(&decoded,media,sizeof(media),current_uid));
        uint32_t old_imports=imports;CHECK(Reboot()==SETTINGS_INVALID_RECORD);CHECK(!runtime_gate && imports==old_imports);
    }
    /* Session override is copied, persists through repeated reads and is never
     * changed by rejected requests or an undersized destination. */
    CHECK(RiderName_Set("Sam",3));CHECK(RiderName_Get(name,sizeof(name))&&!memcmp(name,"Sam",4));
    CHECK(!RiderName_Set("\t",1));CHECK(!RiderName_Get(name,2)&&!memcmp(name,"Sam",4));
    CHECK(RiderName_Set(NULL,0));CHECK(RiderName_Get(name,sizeof(name))&&!name[0]);
    memcpy(media,original,sizeof(media));g_storage_service.mounted=0U;
    CHECK(Reboot()==SETTINGS_STORAGE_ERROR && !runtime_gate);CHECK(g_mock_error==0U);return 0;
}
