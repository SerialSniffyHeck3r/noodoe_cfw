#include "Settings_Remote.h"
#include "Cfw_Files.h"
#include "App_Settings.h"
#include "SettingsService.h"
#include "Odometer_Guard.h"
#include <string.h>
volatile uint32_t assertions;
#define CHECK(x) do{assertions++;if(!(x))return __LINE__;}while(0)
static uint32_t ready,requests,run_required,done,durable=CFW_PENDING,name_id;
static int32_t brightness=25,bias=-3;
static OdometerStatus odo={1,0,13500,0,1,1,1,42,CFW_PENDING};
uint32_t Cfw_Get32(const uint8_t *p){uint32_t v;memcpy(&v,p,4);return v;}
void Cfw_Put32(uint8_t *p,uint32_t v){memcpy(p,&v,4);}
uint32_t SettingsDevice_Lock(void){return 0;}void SettingsDevice_Unlock(uint32_t m){(void)m;}
uint32_t PowerService_RunRequired(void){return run_required;}
void AppSettings_RemoteAllowed(uint32_t value){ready=value;}
uint32_t AppPersistence_FieldAt(uint32_t i){return i==0?0x1002:i==1?0x1008:0;}
uint32_t AppPersistence_FieldCount(void){return 2;}
uint32_t AppPersistence_FieldKey(uint32_t f){return f==0x1002?SK_BRIGHTNESS:f==0x1008?SK_DASH_BIAS:0;}
static const SettingItem light={.key=SK_BRIGHTNESS,.name="Brightness",.kind=SETTING_NUMBER,.min=5,.max=100,.step=5};
static const SettingItem offset={.key=SK_DASH_BIAS,.name="Dashboard offset",.kind=SETTING_NUMBER,.min=-5,.max=5,.step=1};
static const SettingItem ro={.key=SK_VERSION,.name="Read only",.kind=SETTING_READONLY,.min=0,.max=0,.step=1};
const SettingItem *SettingsCatalog_Find(uint32_t key){return key==SK_BRIGHTNESS?&light:key==SK_DASH_BIAS?&offset:&ro;}
int32_t AppSettings_Value(uint32_t key){return key==SK_BRIGHTNESS?brightness:key==SK_DASH_BIAS?bias:0;}
void AppSettings_Format(uint32_t key,char *out,uint32_t size){(void)key;if(size>=5)memcpy(out,"data",5);}
uint32_t AppSettings_RequestRemote(uint32_t key,int32_t value,uint32_t *id){
 if(!ready)return APP_SETTINGS_BUSY;
 const SettingItem *item=SettingsCatalog_Find(key);if(value<item->min||value>item->max)return APP_SETTINGS_ARGUMENT;
 if(key==SK_BRIGHTNESS)brightness=value;else bias=value;*id=++requests;return 0;
}
uint32_t AppSettings_GetResult(uint32_t id,uint32_t *result){(void)id;*result=0;return done;}
uint32_t AppSettings_GetSaveResult(uint32_t id){(void)id;return durable;}
Settings_Status SettingsService_GetRiderName(char *out,uint32_t size){if(size<6)return SETTINGS_ARGUMENT;memcpy(out,"Rider",6);return 0;}
Settings_Status SettingsService_RequestRiderName(const char *p,uint32_t n,uint32_t *id){(void)p;if(n>48)return SETTINGS_ARGUMENT;*id=++name_id;return 0;}
void SettingsService_GetDiagnostics(Settings_Diagnostics *d){memset(d,0,sizeof(*d));d->completed_request=name_id;d->completed_result=0;}
void OdometerGuard_GetStatus(OdometerStatus *out){*out=odo;}
uint32_t OdometerGuard_RequestDecision(uint32_t revision,uint32_t decision){return revision!=42?CFW_EXPIRED:decision==ODO_KEEP?0:CFW_ARGUMENT;}
static uint8_t in[80],out[400];static uint32_t bytes;
static int32_t Call(uint32_t op,uint32_t n){return SettingsRemote_Handle(op,in,n,7,out,&bytes);}
uint32_t Test(void){
 SettingsRemote_Tick(0,1,1,0);Cfw_Put32(in,0);CHECK(!Call(0x97,4));CHECK(bytes==204&&Cfw_Get32(out)==2&&Cfw_Get32(out+4)==8&&!Cfw_Get32(out+8));
 CHECK(Cfw_Get32(out+36)==0x1008&&(int32_t)Cfw_Get32(out+40)==-3&&(int32_t)Cfw_Get32(out+44)==-5);
 CHECK(Call(0x97,3)==CFW_ARGUMENT);Cfw_Put32(in,129);CHECK(Call(0x97,4)==CFW_ARGUMENT);
 Cfw_Put32(in,7);Cfw_Put32(in+4,0x1008);Cfw_Put32(in+8,4);Cfw_Put32(in+12,(uint32_t)-3);
 CHECK(Call(0x99,16)==CFW_BUSY&&!requests);SettingsRemote_Tick(5000,1,1,0);CHECK(ready);
 Cfw_Put32(in,6);CHECK(Call(0x99,16)==CFW_EXPIRED&&!requests);Cfw_Put32(in,7);
 Cfw_Put32(in+12,0);CHECK(Call(0x99,16)==CFW_EXPIRED&&!requests);Cfw_Put32(in+12,(uint32_t)-3);
 Cfw_Put32(in+8,6);CHECK(Call(0x99,16)==CFW_ARGUMENT&&!requests);Cfw_Put32(in+8,4);
 CHECK(!Call(0x99,16)&&bias==4&&requests==1&&Cfw_Get32(out)==1);
 Cfw_Put32(in+4,1);CHECK(!Call(0x9a,8)&&!Cfw_Get32(out+4));done=1;CHECK(!Call(0x9a,8)&&Cfw_Get32(out+12)==CFW_PENDING);
 durable=CFW_IO;CHECK(!Call(0x9a,8)&&Cfw_Get32(out+12)==CFW_IO);durable=0;CHECK(!Call(0x9a,8)&&!Cfw_Get32(out+12));
 Cfw_Put32(in+4,0x4003);CHECK(Call(0x99,16)==CFW_ARGUMENT&&requests==1);
 Cfw_Put32(in,0x201);CHECK(!Call(0x98,4)&&bytes==9&&!memcmp(out+4,"Rider",5));
 Cfw_Put32(in,7);Cfw_Put32(in+4,0x201);memset(in+8,'a',49);CHECK(Call(0x99,57)==CFW_ARGUMENT);
 CHECK(!Call(0x99,56)&&Cfw_Get32(out)==0x80000001U);Cfw_Put32(in+4,0x80000001U);CHECK(!Call(0x9a,8)&&Cfw_Get32(out+4)&&!Cfw_Get32(out+12));
 CHECK(!Call(0x9b,0)&&bytes==36&&Cfw_Get32(out+8)==13500);
 Cfw_Put32(in+4,41);Cfw_Put32(in+8,ODO_KEEP);CHECK(Call(0x9b,12)==CFW_EXPIRED);Cfw_Put32(in+4,42);CHECK(!Call(0x9b,12));
 SettingsRemote_Tick(5100,1,1,10);CHECK(!ready);Cfw_Put32(in+4,0x1008);CHECK(Call(0x99,16)==CFW_BUSY);
 SettingsRemote_Tick(6000,1,1,0);SettingsRemote_Tick(11000,1,1,0);CHECK(ready);run_required=1;SettingsRemote_Tick(11001,1,1,0);CHECK(!ready);
 return 0;
}
