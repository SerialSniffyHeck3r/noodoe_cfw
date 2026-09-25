#if NOODOE_PRODUCT
#include "Settings_Remote.h"
#include "App_Settings.h"
#include "App_Persistence.h"
#include "Settings_Motion.h"
#include "SettingsService.h"
#include "Odometer_Guard.h"
#include "PowerService.h"
#include <string.h>
/* Explicit wire IDs remain stable when SettingKey order changes. Debug IDs
 * already have their own stable, read-only namespace. */
static const uint16_t extra[][2]={
 {0x3001,SK_DATE},{0x3002,SK_TIME},{0x3003,SK_OIL_RESET},{0x3004,SK_BELT_RESET},
 {0x3005,SK_SERV_RESET},{0x3006,SK_DEFAULTS},{0x3007,SK_PAIR},{0x3008,SK_BT_CLOSE},
 {0x3009,SK_BT_DISCONNECT},{0x300a,SK_BT_REPAIR},{0x300b,SK_BT_RESTART},{0x300c,SK_SENSOR_RETRY},
 {0x4001,SK_PHONE1},{0x4002,SK_BT_STATUS},{0x4003,SK_VERSION},{0x4004,SK_AUTO_STATUS},{0x4005,SK_OFF_FINAL_WAIT}
};
static SettingsMotion motion;
static uint32_t allowed;
static struct {uint32_t id,key;} results[8];
static uint32_t head;
static uint32_t Key(uint32_t field)
{
 uint32_t key=AppPersistence_FieldKey(field);if(key)return key;
 for(uint32_t i=0;i<sizeof(extra)/sizeof(extra[0]);++i)if(extra[i][0]==field)return extra[i][1];
 return field>=SD_BT&&field<=SD_INPUT_MODE?field:0;
}
static uint32_t Field(uint32_t index)
{
 uint32_t f=AppPersistence_FieldAt(index);if(f)return f;
 index-=AppPersistence_FieldCount();if(index<sizeof(extra)/sizeof(extra[0]))return extra[index][0];
 index-=sizeof(extra)/sizeof(extra[0]);return index<=SD_INPUT_MODE-SD_BT?SD_BT+index:0;
}
void SettingsRemote_Tick(uint32_t now,uint32_t ign,uint32_t valid,uint32_t speed)
{
 SettingsMotion_Tick(&motion,now,ign&&valid,speed,0);
 uint32_t ready=motion.ready&&!PowerService_RunRequired();
 __atomic_store_n(&allowed,ready,__ATOMIC_RELEASE);AppSettings_RemoteAllowed(ready);
}
/* One SPP owner serializes commands. Values are copied; no radio or NOR waits.
 * Read-only text is technical device data; the app localizes descriptors. */
int32_t SettingsRemote_Handle(uint32_t op,const uint8_t *p,uint32_t n,uint32_t epoch,uint8_t *out,uint32_t *bytes)
{
 *bytes=0;uint32_t ready=__atomic_load_n(&allowed,__ATOMIC_ACQUIRE);
 if(op==0x97){
  if(n!=4||Cfw_Get32(p)>128)return CFW_ARGUMENT;
  uint32_t start=Cfw_Get32(p),count=0;Cfw_Put32(out,2);Cfw_Put32(out+8,ready);
  for(uint32_t i=start;i<start+8;i++){
   uint32_t field=Field(i);if(!field)break;uint32_t key=Key(field);
   const SettingItem *item=SettingsCatalog_Find(key);if(!item)return CFW_CORRUPT;
   uint32_t v[6]={field,key<SK_COUNT?(uint32_t)AppSettings_Value(key):0,item->min,item->max,item->step,item->kind};
   memcpy(out+12+count*24,v,24);++count;
  }
  Cfw_Put32(out+4,count);*bytes=12+count*24;return 0;
 }
 if(op==0x98){
  if(n!=4)return CFW_ARGUMENT;uint32_t field=Cfw_Get32(p);char text[96]={0};
  if(field==0x0201){if(SettingsService_GetRiderName(text,sizeof(text)))return CFW_BUSY;}
  else {uint32_t key=Key(field);if(!key)return CFW_ARGUMENT;AppSettings_Format(key,text,sizeof(text));}
  uint32_t size=strlen(text);Cfw_Put32(out,field);memcpy(out+4,text,size);*bytes=4+size;return 0;
 }
 if(op==0x9b&&n==0){
  OdometerStatus s;OdometerGuard_GetStatus(&s);memcpy(out,&s,sizeof(s));*bytes=sizeof(s);return 0;
 }
 if(n<4||!epoch||Cfw_Get32(p)!=epoch)return CFW_EXPIRED;p+=4;n-=4;
 if(op==0x9b){if(n!=8)return CFW_ARGUMENT;return OdometerGuard_RequestDecision(Cfw_Get32(p),Cfw_Get32(p+4));}
 if(op==0x99){
  if(!ready)return CFW_BUSY;
  if(n<4)return CFW_ARGUMENT;uint32_t field=Cfw_Get32(p),id=0,r;
  if(field==0x0201){
   if(n-4>RIDER_NAME_MAX_BYTES)return CFW_ARGUMENT;
   r=SettingsService_RequestRiderName((const char*)p+4,n-4,&id);if(r)return CFW_BUSY;
   id|=0x80000000U;
  }else{
   if(n!=12)return CFW_ARGUMENT;uint32_t key=Key(field);if(!key)return CFW_ARGUMENT;
   uint32_t lock=SettingsDevice_Lock();
   const SettingItem *item=SettingsCatalog_Find(key);
   if(!item||item->kind==SETTING_READONLY||item->kind==SETTING_DISABLED){SettingsDevice_Unlock(lock);return CFW_ARGUMENT;}
   if(item->kind!=SETTING_ACTION&&item->kind!=SETTING_CONFIRM&&(uint32_t)AppSettings_Value(key)!=Cfw_Get32(p+8)){
    SettingsDevice_Unlock(lock);return CFW_EXPIRED;
   }
   r=AppSettings_RequestRemote(key,(int32_t)Cfw_Get32(p+4),&id);SettingsDevice_Unlock(lock);
   if(r)return r==APP_SETTINGS_BUSY?CFW_BUSY:CFW_ARGUMENT;
   uint32_t at=head++%8;results[at].id=id;results[at].key=key;
  }
  Cfw_Put32(out,id);*bytes=4;return 0;
 }
 if(op==0x9a){
  if(n!=4)return CFW_ARGUMENT;uint32_t id=Cfw_Get32(p),done=0,result=CFW_PENDING,durable=CFW_PENDING;
  if(id&0x80000000U){
   Settings_Diagnostics d;SettingsService_GetDiagnostics(&d);done=d.completed_request==(id&0x7fffffffU);
   if(done)result=durable=d.completed_result;
  }else{
   uint32_t key=0;for(uint32_t i=0;i<8;i++)if(results[i].id==id)key=results[i].key;
   if(!key)return CFW_EXPIRED;done=AppSettings_GetResult(id,&result);
   if(done){const SettingItem *item=SettingsCatalog_Find(key);
    durable=(item->kind==SETTING_ACTION||SettingsConnections_IsAction(key)||key==SK_DATE||key==SK_TIME)?result:AppSettings_GetSaveResult(id);
   }
  }
  Cfw_Put32(out,id);Cfw_Put32(out+4,done);Cfw_Put32(out+8,result);Cfw_Put32(out+12,durable);*bytes=16;return 0;
 }
 return CFW_ARGUMENT;
}
#endif
