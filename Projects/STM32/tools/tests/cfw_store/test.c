/* Actual production audit/journal/config/photo state machines. Only physical
 * NOR, RTOS/UID and SDRAM are replaced. Media obeys erase + 1->0 programming. */
#include <stdint.h>
#include <stddef.h>
#include "cfw_files.c"
#define Lock JournalLock
#define Unlock JournalUnlock
#include "cfw_store.c"
#undef Lock
#undef Unlock

#define Lock AppLock
#define Unlock AppUnlock
#include "app_persistence.c"
#undef Lock
#undef Unlock
#include "trip_computer.c"
/* Real lifetime counter; hardware IRQ/clock is the same bounded test stub. */
#include "UsageCounter.c"
#include "oil_usage_product.c"
static AppSettings preferences;
AppSettings *g_app_settings=&preferences;
const SettingItem *SettingsCatalog_Find(uint32_t key)
{static SettingItem item;item=(SettingItem){.key=key,.name="test",.kind=SETTING_NUMBER,.min=INT32_MIN,.max=INT32_MAX,.step=1};return &item;}
uint32_t SettingsDisplay_Apply(uint32_t key,int32_t value){(void)key;(void)value;return 0;}
uint32_t SettingsPower_EnabledMask(void){return 7;}
uint32_t ShowToastMessages(const char *text,uint32_t seconds){(void)text;(void)seconds;return 1;}
#define Lock ConfigLock
#define Unlock ConfigUnlock
#define U16 ConfigU16
#include "config_store.c"
#undef U16
#undef Lock
#undef Unlock
#define Lock PhotoLock
#define Unlock PhotoUnlock
#include "photo_store.c"
#undef Lock
#undef Unlock

#define Input ServiceInput
#define Output ServiceOutput
#define Fail ServiceFail
#include "PhotoService.c"
#undef Fail
#undef Output
#undef Input
volatile StorageService_Diagnostics g_storage_service;
FRESULT StorageService_FindWallpaper(uint32_t slot,char *path,uint32_t size,uint32_t *length)
{(void)slot;(void)path;(void)size;(void)length;return FR_NO_FILE;}
FRESULT StorageService_FindAlbumPhoto(uint32_t slot,char *path,uint32_t size,uint32_t *length)
{return StorageService_FindWallpaper(slot,path,size,length);}
FRESULT StorageService_ReadFile(const char *path,uint32_t offset,void *out,uint32_t capacity,uint32_t *bytes)
{(void)path;(void)offset;(void)out;(void)capacity;(void)bytes;return FR_NO_FILE;}
uint32_t StorageService_Crc32(const uint8_t *data,uint32_t n,uint32_t seed)
{(void)seed;return ~Cfw_Crc(data,n);}

#define U16 InstallU16
#define P16 InstallP16
#define Fat InstallFat
#define names InstallNames
#define Done InstallDone
#define w installer
#include "cfw_install.c"
#undef w
#undef Done
#undef names
#undef Fat
#undef P16
#undef U16
volatile BSP_Power_Diagnostics g_bsp_power;
#define Lock SettingsLock
#define Unlock SettingsUnlock
#include "settings_product.c"
#undef Lock
#undef Unlock
#include "rider_name.c"
static Bluetooth_KeyStore radio_keys;
static uint32_t persisted_keys;
int Bluetooth_ImportKeys(const Bluetooth_KeyStore *data){radio_keys=*data;return BLUETOOTH_OK;}
int Bluetooth_ExportKeys(Bluetooth_KeyStore *data){*data=radio_keys;return BLUETOOTH_OK;}
void Bluetooth_MarkKeysPersisted(uint32_t gen){persisted_keys=gen;}
static uint32_t install_permit,import_scope;
void StorageDisk_ImportScope(uint32_t enabled){import_scope=enabled;}
BSP_NOR_Status BSP_NOR_UnlockStorage(uint32_t token){install_permit=token==0x42414B32;return install_permit?BSP_NOR_OK:BSP_NOR_LOCKED;}
void BSP_NOR_LockStorage(void){install_permit=0;}

#define NOR ((uint8_t*)0x60000000U)
#define RAM ((uint8_t*)0xC0000000U)
#define BASE 0x9000U
volatile uint32_t g_assertions,g_failure_line,g_mock_error;
static uint32_t tick,arena,cut_at,operations,cut_mode,erase_count[96],granted[3];
static uint32_t fixture_jpeg_bytes;
static uint8_t metadata[0x9000],baseline[131072],buffer[4096];
#define CHECK(x) do{++g_assertions;if(!(x)){g_failure_line=__LINE__;return 1;}}while(0)
void *memcpy(void *dst,const void *src,size_t n){uint8_t *d=dst;const uint8_t *p=src;while(n--)*d++=*p++;return dst;}
void *memset(void *dst,int v,size_t n){uint8_t *d=dst;while(n--)*d++=(uint8_t)v;return dst;}
int memcmp(const void *x,const void *y,size_t n){const uint8_t *a=x,*b=y;while(n--){if(*a!=*b)return *a-*b;++a;++b;}return 0;}
void *memmove(void *dst,const void *src,size_t n){uint8_t *d=dst;const uint8_t *s=src;if(d<s)return memcpy(dst,src,n);while(n){--n;d[n]=s[n];}return dst;}
uint32_t HAL_GetTick(void){return tick;}
void *BSP_RAM_AllocateNamed(uint32_t owner,size_t n){(void)owner;void *p=RAM+arena;arena=(arena+n+31)&~31U;return arena<0x400000?p:NULL;}
void *BSP_RAM_Allocate(size_t n){return BSP_RAM_AllocateNamed(0,n);}
uint32_t StorageDisk_IsStock(void){return 1;}
void BSP_NOR_ClearContainers(void){memset(granted,0,sizeof(granted));}
BSP_NOR_Status BSP_NOR_GrantContainer(uint32_t f,const uint32_t *maps,uint32_t count)
{(void)maps;if(f>=3||count!=(f==0?4U:f==1?8U:32U)){g_mock_error=1;return BSP_NOR_ARGUMENT;}granted[f]=1;return BSP_NOR_OK;}
static uint32_t Allowed(uint32_t f,uint32_t address,uint32_t n)
{uint32_t first=BASE+(f==0?0:f==1?131072:393216);return f<3&&granted[f]&&address>=first&&n<=CfwFiles_Size(f)&&address-first<=CfwFiles_Size(f)-n;}
DRESULT disk_read(BYTE drive,BYTE *dest,DWORD sector,UINT count)
{if(drive||!count||sector>=0x8000||count>0x8000-sector)return RES_ERROR;memcpy(dest,NOR+sector*4096,count*4096);return RES_OK;}
DRESULT disk_write(BYTE drive,const BYTE *data,DWORD sector,UINT count)
{if(drive||count!=1||sector>=CFW_SAFE_END/4096||!install_permit||!import_scope){g_mock_error=5;return RES_ERROR;}
 memcpy(NOR+sector*4096,data,4096);++operations;return RES_OK;}
BSP_NOR_Status BSP_NOR_ContainerErase(uint32_t f,uint32_t address)
{
    if(!Allowed(f,address,4096)||address&4095){g_mock_error=2;return BSP_NOR_PROTECTED;}
    uint32_t cut=++operations==cut_at;if(f<2)++erase_count[(address-BASE)/4096];
    memset(NOR+address,255,cut?(cut_mode?4096:2048):4096);return cut?BSP_NOR_IO:BSP_NOR_OK;
}
uint32_t StorageDisk_ContainerProgram(uint32_t f,uint32_t address,const void *data,uint32_t n)
{
    if(!Allowed(f,address,n)||!n||n>256||(address&255)+n>256){g_mock_error=3;return CFW_IO;}
    uint32_t cut=++operations==cut_at,count=cut&&!cut_mode?n/2:n;const uint8_t *src=data;
    for(uint32_t i=0;i<count;++i){if((NOR[address+i]&src[i])!=src[i]){g_mock_error=4;return CFW_IO;}NOR[address+i]&=src[i];}
    return cut?CFW_IO:0;
}
static void FatSet(uint32_t c,uint32_t v)
{for(uint32_t base=4096;base<=12288;base+=8192){uint8_t *p=NOR+base+c+c/2;uint32_t old=U16(p);p[0]=(uint8_t)(c&1?(old&15)|(v<<4):(old&0xF000)|v);p[1]=(uint8_t)((c&1?(old&15)|(v<<4):(old&0xF000)|v)>>8);}}
static void Initial(void)
{
    memset(NOR,0,0x9000);NOR[11]=0;NOR[12]=16;NOR[13]=8;NOR[14]=1;NOR[16]=2;NOR[18]=2;NOR[21]=0xF8;NOR[22]=2;
    Cfw_Put32(NOR+32,0x7F80);NOR[510]=0x55;NOR[511]=0xAA;FatSet(0,0xFF8);FatSet(1,0xFFF);
    uint32_t cluster=2,offset=BASE;
    for(uint32_t f=0;f<3;++f){uint32_t count=CfwFiles_Size(f)/32768;uint8_t *e=NOR+0x5000+32*f;
        memcpy(e,names[f],11);e[11]=0x20;e[26]=cluster;e[27]=cluster>>8;Cfw_Put32(e+28,CfwFiles_Size(f));
        for(uint32_t i=0;i<count;++i)FatSet(cluster+i,i+1==count?0xFFF:cluster+i+1);
        memset(NOR+offset,255,CfwFiles_Size(f));CfwRecord_Make(NOR+offset,f+1,0,NULL,0);Cfw_Put32(NOR+offset+4092,CFW_RECORD_COMMIT);
        cluster+=count;offset+=CfwFiles_Size(f);
    }
    uint8_t p[8];Cfw_Put32(p,FILE_MAGIC);Cfw_Put32(p+4,1);
    CfwRecord_Make(NOR+BASE+393216,3,0,p,8);Cfw_Put32(NOR+BASE+393216+4092,CFW_RECORD_COMMIT);
    memcpy(metadata,NOR,sizeof(metadata));
}
static uint32_t Reboot(void)
{
    a=NULL;s=NULL;c=NULL;w=NULL;installer=NULL;arena=0;tick=0;trial_view=0;
    g_cfw_quiesce.request=g_cfw_quiesce.ack=0;
    memset((void*)&g_settings,0,sizeof(g_settings));memset(&radio_keys,0,sizeof(radio_keys));
    memset(session_name,0,sizeof(session_name));session_override=0;
    pending_revision=key_revision=key_generation=next_id=request_building=persisted_keys=0;
    memset((void*)&g_cfw_install,0,sizeof(g_cfw_install));
    memset((void*)&g_photos,0,sizeof(g_photos));memset((void*)&g_photo_import,0,sizeof(g_photo_import));
    worker=NULL;import_sequence=import_wallpaper=import_store_request=0;g_storage_service.mounted=1;
    p=NULL;memset((void*)&g_app_persistence,0,sizeof(g_app_persistence));
    loaded=known=0;origin=0;memset(&counter,0,sizeof(counter));memset((void*)&g_oil_usage,0,sizeof(g_oil_usage));
    memset((void*)&g_cfw_store,0,sizeof(g_cfw_store));memset((void*)&g_config_store,0,sizeof(g_config_store));memset((void*)&g_photo_store,0,sizeof(g_photo_store));
    CfwStore_Init();for(uint32_t i=0;i<2000&&!CfwStore_Ready();++i)CfwStore_Process(++tick);
    return CfwStore_Ready()&&!g_cfw_store.error;
}
static uint32_t Submit(uint32_t f,uint32_t value)
{
    uint8_t b[4];Cfw_Put32(b,value);uint32_t id=0;if(CfwStore_Request(f,b,4,&id))return 999;
    for(uint32_t i=0;i<100&&CfwStore_Result(id)==CFW_PENDING;++i)CfwStore_Process(++tick);
    return CfwStore_Result(id);
}
int TestJournal(void)
{
    Initial();cut_at=operations=0;CHECK(Reboot());CHECK(granted[0]&&granted[1]);
    CHECK(Submit(0,11)==0);memcpy(baseline,NOR+BASE,sizeof(baseline));
    /* Every erase/page/commit is interrupted both halfway and after physical
     * completion but before acknowledgement. Recovery chooses only11 or22. */
    for(uint32_t mode=0;mode<2;++mode)for(uint32_t cut=1;cut<=18;++cut){
        memcpy(NOR+BASE,baseline,sizeof(baseline));cut_at=0;CHECK(Reboot());operations=0;cut_at=cut;cut_mode=mode;
        (void)Submit(0,22);cut_at=0;CHECK(Reboot());uint32_t n=0;
        CHECK(CfwStore_Read(0,buffer,4096,&n)==0&&n==4);uint32_t value=Cfw_Get32(buffer);
        CHECK(value==11||value==22);CHECK(!memcmp(NOR,metadata,sizeof(metadata)));
    }
    memcpy(NOR+BASE,baseline,sizeof(baseline));CHECK(Reboot());memset(erase_count,0,sizeof(erase_count));
    for(uint32_t i=0;i<96;++i)CHECK(Submit(0,100+i)==0);
    for(uint32_t i=0;i<32;++i)CHECK(erase_count[i]==3);
    uint32_t before=operations;CHECK(Submit(0,195)==0&&operations==before);
    /* CRC-corrupt latest falls back, all-corrupt does not become valid zero. */
    uint32_t off=BASE+g_cfw_store.files[0].active_sector*4096;NOR[off+64]^=1;CHECK(Reboot());uint32_t n;
    CHECK(CfwStore_Read(0,buffer,4096,&n)==0&&Cfw_Get32(buffer)==194);
    memset(NOR+BASE,0,131072);CHECK(Reboot());CHECK(CfwStore_Read(0,buffer,4096,&n)==CFW_CORRUPT);CHECK(!granted[0]);
    memcpy(NOR+BASE,baseline,sizeof(baseline));Cfw_Put32(NOR+BASE+4,2);Cfw_Put32(NOR+BASE+4088,Cfw_Crc(NOR+BASE,4088));
    CHECK(Reboot());CHECK(g_cfw_store.files[0].status==CFW_VERSION&&!granted[0]);
    CHECK(Submit(0,7)==999);CHECK(!g_mock_error);return 0;
}
int TestRideJournal(void)
{
    Initial();cut_at=operations=0;CHECK(Reboot());CHECK(Submit(1,11)==0);
    uint8_t *copy=(uint8_t*)0xC1000000;
    memcpy(copy,NOR+BASE+131072,262144);
    for(uint32_t mode=0;mode<2;++mode)for(uint32_t cut=1;cut<=18;++cut){
        memcpy(NOR+BASE+131072,copy,262144);cut_at=0;CHECK(Reboot());
        operations=0;cut_at=cut;cut_mode=mode;(void)Submit(1,22);cut_at=0;
        CHECK(Reboot());uint32_t n;
        CHECK(!CfwStore_Read(1,buffer,sizeof(buffer),&n)&&n==4);
        CHECK(Cfw_Get32(buffer)==11||Cfw_Get32(buffer)==22);
        CHECK(!memcmp(NOR,metadata,sizeof(metadata)));
    }
    memcpy(NOR+BASE+131072,copy,262144);CHECK(Reboot());memset(erase_count,0,sizeof(erase_count));
    for(uint32_t i=0;i<192;++i)CHECK(!Submit(1,100+i));
    for(uint32_t i=32;i<96;++i)CHECK(erase_count[i]==3);
    CHECK(!memcmp(NOR,metadata,sizeof(metadata))&&!g_mock_error);return 0;
}
int TestAudit(void)
{
    Initial();CHECK(Reboot());
    /* Cross-link, FREE-in-chain, orphan, mirror mismatch, wrong file size. */
    NOR[0x5020+26]=2;CHECK(!Reboot());Initial();FatSet(3,0);CHECK(!Reboot());
    Initial();FatSet(200,0xFFF);CHECK(!Reboot());Initial();NOR[0x3000+50]^=1;CHECK(!Reboot());
    Initial();Cfw_Put32(NOR+0x5000+28,65536);CHECK(!Reboot());
    Initial();memcpy(NOR+0x5060,NOR+0x5000,32);CHECK(!Reboot());CHECK(!g_mock_error);return 0;
}
int TestConfigBackpressure(void)
{
    Initial();CHECK(Reboot());ConfigStore_Process(tick);
    uint8_t b[4];uint32_t revision;Cfw_Put32(b,35);
    CHECK(!ConfigStore_Set(0x1002,b,4,&revision));
    g_cfw_quiesce.request=1;tick+=2000;ConfigStore_Process(tick);
    CHECK(!g_config_store.last_result&&!g_config_store.failures);
    CHECK(g_config_store.revision!=g_config_store.saved_revision);
    CHECK(ConfigStore_Result(revision)==CFW_PENDING);
    g_cfw_quiesce.request=0;
    for(uint32_t i=0;i<1100;i++){++tick;ConfigStore_Process(tick);CfwStore_Process(tick);}
    CHECK(ConfigStore_Result(revision)==CFW_OK);
    CHECK(!g_config_store.last_result&&!g_config_store.failures);return 0;
}
int TestConfig(void)
{
    Initial();CHECK(Reboot());ConfigStore_Process(tick);CHECK(g_config_store.ready&&!g_config_store.error);
    uint8_t b[4];uint32_t r=0;Cfw_Put32(b,25);CHECK(!ConfigStore_Set(0x1002,b,4,&r));
    for(uint32_t i=0;i<1000;++i){++tick;ConfigStore_Process(tick);CfwStore_Process(tick);}CHECK(ConfigStore_Result(r)==CFW_PENDING);
    for(uint32_t i=0;i<50;++i){++tick;ConfigStore_Process(tick);CfwStore_Process(tick);}CHECK(ConfigStore_Result(r)==0);
    CHECK(Reboot());ConfigStore_Process(tick);uint32_t n;CHECK(!ConfigStore_Get(0x1002,b,4,&n)&&n==4&&Cfw_Get32(b)==25);
    uint32_t writes=g_cfw_store.files[0].writes;CHECK(!ConfigStore_Set(0x1002,b,4,&r));tick+=3000;ConfigStore_Process(tick);CHECK(g_cfw_store.files[0].writes==writes);
    /* Unrecognized IDs survive a known-field update. */
    Cfw_Put32(b,123);CHECK(!ConfigStore_Set(0x7F01,b,4,&r));Cfw_Put32(b,50);CHECK(!ConfigStore_Set(0x1002,b,4,&r));
    tick+=1000;for(uint32_t i=0;i<50;++i){++tick;ConfigStore_Process(tick);CfwStore_Process(tick);}
    CHECK(Reboot());ConfigStore_Process(tick);CHECK(!ConfigStore_Get(0x7F01,b,4,&n)&&Cfw_Get32(b)==123);return 0;
}
/* Python runner writes a real JPEG to mapped fixture memory. */
int TestPhoto(uint32_t length)
{
    fixture_jpeg_bytes=length;Initial();CHECK(Reboot());
    for(uint32_t i=0;i<10&&!g_photo_store.ready;++i)PhotoStore_Process(++tick);
    CHECK(g_photo_store.ready&&!g_photo_store.error);uint32_t id;
    CHECK(!PhotoStore_RequestReplace(0,(void*)0x61000000,length,&id));
    for(uint32_t i=0;i<3000&&PhotoStore_GetResult(id)==CFW_PENDING;++i)PhotoStore_Process(++tick);
    CHECK(PhotoStore_GetResult(id)==0&&g_photo_store.generation[0]==1);
    CHECK(!memcmp(NOR,metadata,sizeof(metadata)));
    CHECK(Reboot());for(uint32_t i=0;i<3000&&!g_photo_store.ready;++i)PhotoStore_Process(++tick);
    CHECK(g_photo_store.ready&&g_photo_store.length[0]==fixture_jpeg_bytes);
    CHECK(!PhotoStore_RequestReplace(0,(void*)0x61000000,length,&id));operations=0;cut_at=45;cut_mode=0;
    for(uint32_t i=0;i<3000&&PhotoStore_GetResult(id)==CFW_PENDING;++i)PhotoStore_Process(++tick);
    CHECK(PhotoStore_GetResult(id)==CFW_IO);cut_at=0;CHECK(Reboot());
    for(uint32_t i=0;i<3000&&!g_photo_store.ready;++i)PhotoStore_Process(++tick);
    CHECK(g_photo_store.generation[0]==1&&g_photo_store.length[0]==length);CHECK(!g_mock_error);return 0;
}
int TestPhotoReset(uint32_t length)
{
    fixture_jpeg_bytes=length;Initial();CHECK(Reboot());
    for(uint32_t i=0;i<3000&&!g_photo_store.ready;i++)PhotoStore_Process(++tick);
    for(uint32_t slot=0;slot<3;slot++){uint32_t id;
        CHECK(!PhotoStore_RequestReplace(slot,(void*)0x61000000,length,&id));
        for(uint32_t i=0;i<3000&&PhotoStore_GetResult(id)==CFW_PENDING;i++)PhotoStore_Process(++tick);
        CHECK(!PhotoStore_GetResult(id));
    }
    /* Power loss between each header erase; durable RESET_PENDING caller will
     * request reset again. No old photo remains after the final completion. */
    uint32_t e=CFW_PENDING;
    for(uint32_t pass=0;pass<12&&e==CFW_PENDING;pass++){
        e=PhotoStore_ResetSlots();CHECK(e==CFW_PENDING||e==CFW_OK);
        CHECK(!memcmp(NOR,metadata,sizeof(metadata)));
        CHECK(Reboot());for(uint32_t i=0;i<3000&&!g_photo_store.ready;i++)PhotoStore_Process(++tick);
    }
    CHECK(e==CFW_OK);uint8_t b[4];uint32_t n;
    for(uint32_t slot=0;slot<3;slot++)CHECK(PhotoStore_Read(slot,b,4,&n)==CFW_MISSING);
    uint32_t old=operations;CHECK(!PhotoStore_ResetSlots()&&operations==old);CHECK(!g_mock_error);return 0;
}
int TestPhotoCuts(uint32_t length)
{
    Initial();CHECK(Reboot());for(uint32_t i=0;i<10&&!g_photo_store.ready;++i)PhotoStore_Process(++tick);
    uint32_t id;CHECK(!PhotoStore_RequestReplace(0,(void*)0x61000000,length,&id));
    for(uint32_t i=0;i<3000&&PhotoStore_GetResult(id)==CFW_PENDING;++i)PhotoStore_Process(++tick);
    CHECK(PhotoStore_GetResult(id)==0);uint8_t *copy=(uint8_t*)0xC1000000;
    memcpy(copy,NOR+BASE+393216+65536+163840,163840);
    uint32_t total=40+16+(length+255)/256+1;
    for(uint32_t mode=0;mode<2;++mode)for(uint32_t cut=1;cut<=total;++cut){
        memcpy(NOR+BASE+393216+65536+163840,copy,163840);cut_at=0;CHECK(Reboot());
        for(uint32_t i=0;i<3000&&!g_photo_store.ready;++i)PhotoStore_Process(++tick);
        CHECK(!PhotoStore_RequestReplace(0,(void*)0x61000000,length,&id));operations=0;cut_at=cut;cut_mode=mode;
        for(uint32_t i=0;i<3000&&PhotoStore_GetResult(id)==CFW_PENDING;++i)PhotoStore_Process(++tick);
        cut_at=0;CHECK(Reboot());for(uint32_t i=0;i<3000&&!g_photo_store.ready;++i)PhotoStore_Process(++tick);
        CHECK(g_photo_store.length[0]==length&&(g_photo_store.generation[0]==1||g_photo_store.generation[0]==2));
        CHECK(!memcmp(NOR,metadata,sizeof(metadata)));
    }
    CHECK(!g_mock_error);return 0;
}
int TestRide(void)
{
    Initial();CHECK(Reboot());memset(&preferences,0,sizeof(preferences));
    AppPersistence_Process(1000);OilUsageService_Process(1000,1,1,1);OilUsageService_Process(2000,1,1,1);
    TripComputer trips;TripComputer_Init(&trips);UiState ui={0};
    trips.date=20260918;trips.records[0]=(TripRecord){.distance_mm=123456,.moving_ms=5000,.stopped_ms=2000,.unknown_ms=3000,.max_kph=120,.valid=1,.partial=1};
    preferences.origins[0]=(SettingsServiceOrigin){7,36475,739000,500};
    AppPersistence_UI(2000,&trips,&ui);
    /* Restore occurred before a user explicitly establishes the service origin. */
    preferences.origins[0]=(SettingsServiceOrigin){7,36475,739000,500};AppPersistence_UI(2001,&trips,&ui);
    uint32_t id;CHECK(!RideStore_RequestCheckpoint(&id));AppPersistence_UI(tick,&trips,&ui);
    for(uint32_t i=0;i<100;++i){tick=2100+i;AppPersistence_Process(tick);CfwStore_Process(tick);}
    CHECK(RideStore_GetResult(id)==0&&g_app_persistence.last_success_ms);CHECK(!memcmp(NOR,metadata,sizeof(metadata)));
    CHECK(Reboot());memset(&preferences,0,sizeof(preferences));TripComputer_Init(&trips);
    trips.records[0].distance_mm=100;trips.records[0].moving_ms=20;
    AppPersistence_Process(1000);OilUsageService_Process(1000,1,1,1);AppPersistence_UI(1001,&trips,&ui);
    CHECK(trips.records[0].distance_mm==123556&&trips.records[0].moving_ms==5020);
    CHECK(trips.records[0].stopped_ms==2000&&trips.records[0].unknown_ms==3000);
    CHECK(preferences.origins[0].odo_km==36475&&preferences.origins[0].on_ms==500);
    AppPersistence_UI(1002,&trips,&ui);CHECK(trips.records[0].distance_mm==123556&&g_app_persistence.restore_count==1);
    CHECK(!trips.have_tick&&!trips.last_ms);CHECK(g_oil_usage.total_ms==1000&&g_oil_usage.boot_on_ms==0);
    OilUsageService_Process(2000,1,1,1);CHECK(g_oil_usage.total_ms==2000);
    /* Raw rapid OFF/ON does not manufacture SESSION_END or write a record. */
    OilUsageService_Process(2100,1,0,1);OilUsageService_Process(2200,1,1,1);CHECK(!g_app_persistence.force);
    CHECK(TripComputer_Reset(&trips,0));AppPersistence_UI(2200,&trips,&ui);CHECK(!RideStore_RequestCheckpoint(&id));AppPersistence_UI(tick,&trips,&ui);
    for(uint32_t i=0;i<100;++i){tick=2300+i;AppPersistence_Process(tick);CfwStore_Process(tick);}
    CHECK(RideStore_GetResult(id)==0);CHECK(Reboot());TripComputer_Init(&trips);AppPersistence_Process(1000);AppPersistence_UI(1001,&trips,&ui);
    CHECK(trips.records[0].valid&&trips.records[0].distance_mm==0);CHECK(preferences.origins[0].odo_km==36475);
    /* UTC/local calendar reconciliation remains the real trip owner's job. */
    trips.records[TRIP_TODAY].distance_mm=42;TripComputer_Tick(&trips,2000,1,1,0,20260919);
    CHECK(trips.date==20260919&&trips.records[TRIP_TODAY].distance_mm==0);return 0;
}
/* Host fixture is a byte-pair-decoded full real volume, not a synthetic FAT. */
int TestHistorical(void)
{
    a=NULL;arena=0;CfwFiles_Begin();uint32_t result=CFW_PENDING;
    for(uint32_t i=0;i<10000&&result==CFW_PENDING;++i)result=CfwFiles_Process();
    CHECK(result==CFW_OK);CHECK(!g_mock_error);return 0;
}
int TestInstall(void)
{
    Initial();memcpy((void*)0xC1000000,NOR+BASE,CFW_INSTALL_BYTES);
    /* Empty clean FAT, with the old free-cluster contents retained as arbitrary bytes. */
    memset(NOR+0x5000,0,16384);for(uint32_t c=2;c<46;++c)FatSet(c,0);
    CHECK(Reboot());CHECK(!CfwFiles_Present(0));
    CfwInstall_Process();CHECK(g_cfw_install.magic==0x31494643);
    memcpy((void*)g_cfw_install.buffer,(void*)0xC1000000,CFW_INSTALL_BYTES);
    g_cfw_install.first[0]=2;g_cfw_install.first[1]=6;g_cfw_install.first[2]=14;
    for(uint32_t i=0;i<3;++i)g_cfw_install.entry[i]=i;
    g_cfw_install.metadata_crc=Cfw_Crc(NOR,0x9000);
    g_cfw_install.payload_crc=Cfw_Crc((void*)g_cfw_install.buffer,CFW_INSTALL_BYTES);
    g_cfw_install.token=0x42414B32;g_bsp_power.ign_valid=g_bsp_power.ign_on=1;
    g_cfw_install.sequence=1;
    for(uint32_t i=0;i<2000&&g_cfw_install.ack!=1;++i)CfwInstall_Process();
    CHECK(g_cfw_install.ack==1&&g_cfw_install.error==0);CHECK(!install_permit&&!import_scope);
    CHECK(!memcmp(NOR,metadata,sizeof(metadata)));CHECK(!memcmp(NOR+BASE,(void*)0xC1000000,CFW_INSTALL_BYTES));
    CHECK(Reboot());CHECK(CfwFiles_Present(0)&&CfwFiles_Present(1)&&CfwFiles_Present(2));
    /* Same-name collision must reject before the first mutation. */
    CfwInstall_Process();g_cfw_install.token=0x42414B32;g_cfw_install.sequence=1;
    uint32_t before=operations;CfwInstall_Process();CHECK(g_cfw_install.error==CFW_BUSY&&operations==before);
    CHECK(!g_mock_error);return 0;
}
int TestSettings(void)
{
    Initial();CHECK(Reboot());ConfigStore_Process(tick);CHECK(SettingsService_Init()==SETTINGS_OK);
    uint32_t id,other;CHECK(SettingsService_RequestRiderName("Rider",5,&id)==SETTINGS_OK);
    CHECK(SettingsService_RequestPreferences(30,1,&other)==SETTINGS_BUSY);
    char name[49];CHECK(RiderName_Get(name,sizeof(name))&&!memcmp(name,"Rider",6));
    CHECK(g_settings.pending_request==id&&g_settings.completed_request!=id);
    for(uint32_t i=0;i<1200;++i){++tick;ConfigStore_Process(tick);CfwStore_Process(tick);SettingsService_Process(tick);}
    CHECK(g_settings.completed_request==id&&g_settings.completed_result==SETTINGS_OK);
    CHECK(Reboot());ConfigStore_Process(tick);CHECK(SettingsService_Init()==SETTINGS_OK);
    CHECK(RiderName_Get(name,sizeof(name))&&!memcmp(name,"Rider",6));
    CHECK(SettingsService_RequestPreferences(30,1,&id)==SETTINGS_OK);
    Settings_Values values;CHECK(SettingsService_GetValues(&values)==SETTINGS_OK&&values.backlight_percent==30&&values.units==1);
    for(uint32_t i=0;i<1200;++i){++tick;ConfigStore_Process(tick);CfwStore_Process(tick);SettingsService_Process(tick);}
    CHECK(g_settings.completed_request==id&&!g_settings.completed_result);
    radio_keys.generation=9;radio_keys.version=1;
    for(uint32_t k=0;k<6;++k){radio_keys.keys[k].valid=1;radio_keys.keys[k].type=4;radio_keys.keys[k].address[0]=k+1;radio_keys.keys[k].key[15]=50+k;}
    for(uint32_t i=0;i<1200;++i){++tick;ConfigStore_Process(tick);CfwStore_Process(tick);SettingsService_Process(tick);}
    CHECK(persisted_keys==9);CHECK(Reboot());ConfigStore_Process(tick);CHECK(SettingsService_Init()==SETTINGS_OK);
    CHECK(radio_keys.generation==9&&radio_keys.keys[5].key[15]==55);
    /* Oversized unknown fields cannot make a two-word request partially apply. */
    uint8_t fill[3800]={0};uint32_t revision;CHECK(ConfigStore_Set(0x7000,fill,sizeof(fill),&revision)==CFW_MEMORY);
    uint8_t invalid[8]={0};CHECK(!ConfigStore_Set(0x1002,invalid,8,&revision));
    const uint16_t ids[2]={0x1011,0x1002};const uint32_t v[2]={0,31};
    CHECK(ConfigStore_SetWords(ids,v,2,&revision)==CFW_CORRUPT);
    uint32_t n;CHECK(!ConfigStore_Get(0x1011,invalid,8,&n)&&Cfw_Get32(invalid)==1);
    return 0;
}
int TestDeadline(void)
{
    Initial();CHECK(Reboot());memset(&preferences,0,sizeof(preferences));
    AppPersistence_Process(tick);TripComputer trips;TripComputer_Init(&trips);UiState ui={0};
    AppPersistence_UI(tick,&trips,&ui);
    uint32_t previous=0,completed=0,allocated=arena;
    for(uint32_t i=0;i<20000;++i){tick+=10;
        OilUsageService_Process(tick,1,1,1);++trips.records[0].distance_mm;
        AppPersistence_UI(tick,&trips,&ui);AppPersistence_Process(tick);CfwStore_Process(tick);
        if(g_app_persistence.last_success_ms!=previous){
            CHECK(g_app_persistence.last_success_ms-previous<=60000);previous=g_app_persistence.last_success_ms;++completed;
        }
    }
    CHECK(completed>=3&&arena==allocated&&!g_app_persistence.overdue);
    /* Request after a failure never inherits the preceding request's result. */
    uint32_t id;CHECK(!RideStore_RequestCheckpoint(&id));CHECK(RideStore_GetResult(id)==CFW_PENDING);
    CHECK(!g_mock_error);return 0;
}
int TestIdleDeadline(void)
{
    Initial();CHECK(Reboot());memset(&preferences,0,sizeof(preferences));
    AppPersistence_Process(tick);TripComputer trips;TripComputer_Init(&trips);UiState ui={0};
    AppPersistence_UI(tick,&trips,&ui);
    uint32_t id;CHECK(!RideStore_RequestCheckpoint(&id));AppPersistence_UI(tick,&trips,&ui);
    for(uint32_t i=0;i<1200;i++){++tick;AppPersistence_Process(tick);CfwStore_Process(tick);}
    CHECK(RideStore_GetResult(id)==CFW_OK);CHECK(!g_app_persistence.dirty);
    tick+=3600000;trips.records[0].distance_mm=1;
    AppPersistence_UI(tick,&trips,&ui);AppPersistence_Process(tick);
    CHECK(g_app_persistence.dirty&&!g_app_persistence.overdue);
    return 0;
}
int TestPhotoMailbox(uint32_t length)
{
    Initial();CHECK(Reboot());for(uint32_t i=0;i<10&&!g_photo_store.ready;++i)PhotoStore_Process(++tick);
    uint32_t id;CHECK(!PhotoStore_RequestReplace(0,(void*)0x61000000,length,&id));
    for(uint32_t i=0;i<3000&&PhotoStore_GetResult(id)==CFW_PENDING;++i)PhotoStore_Process(++tick);
    CHECK(PhotoService_RequestLoad(0));for(uint32_t i=0;i<100;++i)PhotoService_Process();
    PhotoImage first,second;CHECK(PhotoService_Get(0,&first)&&first.width==32&&g_photo_import.version==3);
    CHECK(g_photo_import.buffer&&g_photo_import.sequence==g_photo_import.ack);
    uint32_t allocated=0;
    for(uint32_t seq=1;seq<=10;++seq){
        memcpy((void*)g_photo_import.buffer,(void*)0x61000000,length);
        g_photo_import.slot=0;g_photo_import.length=length;g_photo_import.crc32=Cfw_Crc((void*)0x61000000,length);
        g_photo_import.arm=0x42414B32;g_photo_import.sequence=seq;
        for(uint32_t i=0;i<3000&&(g_photo_import.ack!=seq||!g_photo_import.buffer);++i){PhotoStore_Process(++tick);PhotoService_Process();}
        CHECK(g_photo_import.ack==seq&&!g_photo_import.result);
        CHECK(PhotoService_Get(0,&second)&&second.revision==seq+1+0x10000);
        CHECK(PhotoService_Retiring(0)==first.pixels&&second.pixels!=first.pixels);
        CHECK(!memcmp(first.pixels,second.pixels,second.bytes));
        PhotoService_Release(0,first.pixels);CHECK(!PhotoService_Retiring(0));first=second;
        if(seq==1)allocated=arena;else CHECK(arena==allocated);
        CHECK(!memcmp(NOR,metadata,sizeof(metadata)));
    }
    /* A retired ABI2 WALL flag cannot reach legacy FAT creation in Product. */
    uint32_t before=operations;g_photo_import.slot=0x100;g_photo_import.arm=0x42414B32;g_photo_import.sequence=11;
    PhotoService_Process();CHECK(g_photo_import.ack==11&&g_photo_import.result&&operations==before);
    CHECK(!g_mock_error);return 0;
}
int TestPhotoResetCache(uint32_t length)
{
    Initial();CHECK(Reboot());for(uint32_t i=0;i<10&&!g_photo_store.ready;i++)PhotoStore_Process(++tick);
    uint32_t id;CHECK(!PhotoStore_RequestReplace(0,(void*)0x61000000,length,&id));
    for(uint32_t i=0;i<3000&&PhotoStore_GetResult(id)==CFW_PENDING;i++)PhotoStore_Process(++tick);
    PhotoService_RequestLoad(0);for(uint32_t i=0;i<100;i++)PhotoService_Process();
    PhotoImage before,after;CHECK(PhotoService_Get(0,&before));CHECK(before.revision==0x10001);
    uint32_t reset=CFW_PENDING;for(uint32_t i=0;i<10&&reset==CFW_PENDING;i++)reset=PhotoStore_ResetSlots();CHECK(!reset);
    PhotoService_Process();CHECK(!PhotoService_Get(0,&after));
    CHECK(!PhotoStore_RequestReplace(0,(void*)0x61000000,length,&id));
    for(uint32_t i=0;i<3000&&PhotoStore_GetResult(id)==CFW_PENDING;i++)PhotoStore_Process(++tick);
    for(uint32_t i=0;i<100;i++)PhotoService_Process();
    CHECK(PhotoService_Get(0,&after)&&after.revision==before.revision);
    CHECK(after.pixels!=before.pixels&&PhotoService_Retiring(0)==before.pixels);
    CHECK(!memcmp(NOR,metadata,sizeof(metadata))&&!g_mock_error);return 0;
}
int TestAppSave(void)
{
    Initial();CHECK(Reboot());ConfigStore_Process(tick);AppPersistence_Process(tick);
    memset(&preferences,0,sizeof(preferences));preferences.values[SK_OFF_DISPLAY]=1;
    TripComputer trips;TripComputer_Init(&trips);UiState ui={0};
    AppPersistence_UI(tick,&trips,&ui);CHECK(AppPersistence_SettingsReady());
    preferences.values[SK_BRIGHTNESS]=42;
    AppPersistence_SettingsApplied(1,SK_BRIGHTNESS,APP_SETTINGS_OK);
    CHECK(AppSettings_GetSaveResult(1)==CFW_PENDING);
    for(uint32_t i=0;i<1200;++i){++tick;ConfigStore_Process(tick);CfwStore_Process(tick);}
    CHECK(AppSettings_GetSaveResult(1)==CFW_OK);
    AppPersistence_SettingsApplied(2,SK_BRIGHTNESS,APP_SETTINGS_ARGUMENT);
    CHECK(AppSettings_GetSaveResult(2)==CFW_ARGUMENT);
    AppPersistence_SettingsApplied(3,SK_OIL_RESET,APP_SETTINGS_OK);
    CHECK(AppSettings_GetSaveResult(3)==CFW_PENDING);
    /* A durable completion cannot precede the post-action UI snapshot. */
    for(uint32_t i=0;i<2000;++i){++tick;AppPersistence_Process(tick);CfwStore_Process(tick);}
    CHECK(AppSettings_GetSaveResult(3)==CFW_PENDING);
    AppPersistence_UI(tick,&trips,&ui);
    for(uint32_t i=0;i<1200;++i){++tick;AppPersistence_Process(tick);CfwStore_Process(tick);}
    CHECK(AppSettings_GetSaveResult(3)==CFW_OK);
    preferences.values[SK_BRIGHTNESS]=45;AppPersistence_SettingsApplied(4,SK_BRIGHTNESS,APP_SETTINGS_OK);
    operations=0;cut_at=1;cut_mode=0;
    for(uint32_t i=0;i<1100;++i){++tick;ConfigStore_Process(tick);CfwStore_Process(tick);}
    CHECK(AppSettings_GetSaveResult(4)==CFW_IO);cut_at=0;
    CHECK(!memcmp(NOR,metadata,sizeof(metadata))&&!g_mock_error);return 0;
}

/* Trial reads defaults without mutating durable preferences; reset marker and
 * key preservation share one commit, then repeated recovery is a no-op. */
int TestTrialSettings(void)
{
 Initial();CHECK(Reboot());ConfigStore_Process(tick);uint32_t revision,n;uint8_t value[4];
 Cfw_Put32(value,77);CHECK(!ConfigStore_Set(0x1002,value,4,&revision));
 Cfw_Put32(value,99);CHECK(!ConfigStore_Set(0x0200,value,4,&revision));
 Cfw_Put32(value,101);CHECK(!ConfigStore_Set(0x0204,value,4,&revision));
 for(uint32_t i=0;i<1100;i++){ConfigStore_Process(++tick);CfwStore_Process(tick);}
 CHECK(ConfigStore_Result(revision)==0);
 ConfigStore_SetTrialView(2);CHECK(!ConfigStore_Get(0x1002,value,4,&n)&&Cfw_Get32(value)==77);
 CHECK(ConfigStore_Set(0x1002,value,4,&revision)==CFW_BUSY);
 ConfigStore_SetTrialView(1);CHECK(ConfigStore_Get(0x1002,value,4,&n)==CFW_MISSING);
 CHECK(!ConfigStore_Get(0x0200,value,4,&n)&&Cfw_Get32(value)==99);
 CHECK(!ConfigStore_Get(0x0204,value,4,&n)&&Cfw_Get32(value)==101);
 CHECK(ConfigStore_Set(0x1002,value,4,&revision)==CFW_BUSY);
 CHECK(Reboot());ConfigStore_Process(tick);CHECK(!ConfigStore_Get(0x1002,value,4,&n)&&Cfw_Get32(value)==77);
 CHECK(!ConfigStore_ResetPreferences(42,&revision));
 for(uint32_t i=0;i<1100;i++){ConfigStore_Process(++tick);CfwStore_Process(tick);}
 CHECK(ConfigStore_Result(revision)==0);CHECK(Reboot());ConfigStore_Process(tick);
 CHECK(ConfigStore_Get(0x1002,value,4,&n)==CFW_MISSING);
 CHECK(!ConfigStore_Get(0x0200,value,4,&n)&&Cfw_Get32(value)==99);
 CHECK(!ConfigStore_Get(CONFIG_FIELD_RESET_EPOCH,value,4,&n)&&Cfw_Get32(value)==42);
 CHECK(!ConfigStore_Get(0x0204,value,4,&n)&&Cfw_Get32(value)==101);
 uint32_t writes=g_cfw_store.files[0].writes;
 CHECK(!ConfigStore_ResetPreferences(42,&revision));
 for(uint32_t i=0;i<1100;i++){ConfigStore_Process(++tick);CfwStore_Process(tick);}
 CHECK(g_cfw_store.files[0].writes==writes);return 0;
}
