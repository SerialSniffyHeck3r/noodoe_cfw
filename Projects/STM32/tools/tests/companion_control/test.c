#include "CompanionControl.h"
#include "ProductUI.h"
#include "App_Settings.h"
#include "App_Persistence.h"
#include "PhoneVisual.h"
#include <string.h>
volatile ProductUI_Diagnostics g_product_ui;
volatile PhoneVisualStatus g_phone_visual;
void PhoneVisual_KeepPanels(const uint32_t *keys,uint32_t n){(void)keys;(void)n;}
void PhoneVisual_Session(uint32_t epoch){(void)epoch;}
uint32_t PhoneVisual_Begin(uint32_t e,uint32_t k,uint32_t id,uint32_t n,uint32_t crc){(void)e;(void)k;(void)id;(void)n;(void)crc;return 0;}
uint32_t PhoneVisual_Data(uint32_t e,uint32_t id,uint32_t o,const void *p,uint32_t n){(void)e;(void)id;(void)o;(void)p;g_phone_visual.received+=n;return 0;}
uint32_t PhoneVisual_Finish(uint32_t e,uint32_t id){(void)e;(void)id;return 0;}
uint32_t ProductUI_RideSession(void){return 1234;}
uint32_t PhoneVisual_MusicTrack(uint32_t key,uint32_t width,uint32_t out[7]){(void)key;(void)width;for(uint32_t i=0;i<7;i++)out[i]=0x78563412U+i;return 0;}
uint32_t ProductUI_PublishCalls(const PhoneCallsSnapshot *calls){return calls->generation!=0;}
uint32_t ProductUI_CallTarget(void){return 0;}
static PhoneContent content;static uint32_t publications;
volatile uint32_t failure,checks;
#define CHECK(x) do{checks++;if(!(x)){failure=__LINE__;return;}}while(0)
uint32_t ProductUI_PhoneToken(uint32_t slot){return slot?0:content.slots[0].token;}
uint32_t ProductUI_PublishPhone(uint32_t slot,uint32_t token,const PhoneStatus *v,uint32_t now){return PhoneContent_Status(&content,slot,token,v,now);}
uint32_t ProductUI_PublishMusic(uint32_t slot,uint32_t token,const PhoneMusic *v,uint32_t now){publications++;return PhoneContent_Music(&content,slot,token,v,now);}
uint32_t AppPersistence_FieldAt(uint32_t i){return i?0:0x1002;}
uint32_t AppPersistence_FieldKey(uint32_t i){return i==0x1002?SK_BRIGHTNESS:0;}
static const SettingItem item={.key=SK_BRIGHTNESS,.name="Brightness",.kind=SETTING_NUMBER,.min=0,.max=100,.step=1};
const SettingItem *SettingsCatalog_Find(uint32_t key){return key==SK_BRIGHTNESS?&item:0;}
int32_t AppSettings_Value(uint32_t key){(void)key;return 25;}
uint32_t AppSettings_RequestRemote(uint32_t key,int32_t value,uint32_t *id){*id=7;return key==SK_BRIGHTNESS&&value>=0&&value<=100?0:2;}
uint32_t AppSettings_GetResult(uint32_t id,uint32_t *out){*out=0;return id==7;}
uint32_t AppSettings_GetSaveResult(uint32_t id){return id==7?CFW_PENDING:CFW_EXPIRED;}
static uint8_t in[1024],out[1024];static uint32_t bytes;
static void W(uint8_t *p,uint32_t n){for(uint32_t i=0;i<4;i++)p[i]=n>>(8*i);}
static uint32_t Call(uint32_t op,uint32_t n,uint32_t epoch){return CompanionControl_Handle(op,in,n,epoch,100,out,&bytes);}
void Test(void){
 PhoneCallsSnapshot snapshot={0};uint8_t call[48]={0};W(call,2);W(call+4,1);W(call+8,2);W(call+12,1);W(call+28,11);W(call+32,22);W(call+36,3);W(call+40,11);W(call+44,1);
 CHECK(PhoneCalls_Decode(&snapshot,call,48,99,100)&&snapshot.call_type==3&&snapshot.permissions==2);
 W(call+36,8);CHECK(!PhoneCalls_Decode(&snapshot,call,48,99,100)&&snapshot.call_type==3);
 W(call,1);W(call+36,3);CHECK(!PhoneCalls_Decode(&snapshot,call,48,99,100));W(call+36,0);CHECK(PhoneCalls_Decode(&snapshot,call,48,99,100));
PhoneContent_Init(&content);PhoneContent_Links(&content,1);W(in,99);
 CHECK(Call(0x70,0,99)==0&&bytes==32&&out[0]==2&&out[28]==(1234&255));CHECK(Call(0x70,1,99)==CFW_ARGUMENT);
 W(in,0);CHECK(Call(0x71,4,99)==0&&bytes==32);
 W(in,98);W(in+4,0x1002);W(in+8,42);CHECK(Call(0x72,12,99)==CFW_EXPIRED);
 W(in,99);CHECK(Call(0x72,12,99)==0&&out[0]==7);W(in+4,0x200);CHECK(Call(0x72,12,99)==CFW_ARGUMENT);
 W(in+4,7);CHECK(Call(0x73,8,99)==0&&bytes==16&&out[12]==CFW_PENDING);
 W(in+4,1);W(in+8,288);CHECK(Call(0x95,12,99)==0&&bytes==28&&out[0]==0x12&&out[1]==0x34&&out[2]==0x56&&out[3]==0x78&&out[24]==0x18);
 /* Seven insertions, duplicate update, removal and a new connection. */
 for(uint32_t id=1;id<=11;id++){W(in,99);W(in+4,id);in[8]=1;in[9]=1;in[10]=1;memcpy(in+11,"abc",3);CHECK(Call(0x74,14,99)==0);}
 CHECK(content.slots[0].status.count==10&&content.slots[0].status.notifications[0].id==11&&content.slots[0].status.notifications[9].id==2);
 uint32_t rev=content.slots[0].status_revision;CHECK(Call(0x74,14,99)==0&&content.slots[0].status_revision==rev);
 W(in+4,4);CHECK(Call(0x75,8,99)==0&&content.slots[0].status.count==9);
 W(in+4,0);CHECK(Call(0x75,8,99)==0&&content.slots[0].status.count==0);
 W(in+4,101);W(in+8,0);CHECK(Call(0x78,12,99)==CFW_ARGUMENT);W(in+4,75);CHECK(Call(0x78,12,99)==0&&content.slots[0].status.battery_percent==75);
 W(in+4,200);W(in+8,201);W(in+12,5);W(in+16,7);CHECK(Call(0x7d,20,99)==0&&content.slots[0].status.reply_count==5&&content.slots[0].status.connection_epoch==99);
 W(in+12,6);CHECK(Call(0x7d,20,99)==CFW_ARGUMENT);
 /* Music/art cannot publish a partial image; oversized chunks are rejected. */
 memset(in+4,0,20);in[4]=1;in[16]=1;in[17]=1;in[18]='T';in[19]='A';CHECK(Call(0x76,20,99)==0&&content.slots[0].music.valid);
 uint32_t before=publications;
 for(uint32_t offset=0;offset<2048;offset+=512){W(in+4,offset);memset(in+8,offset/512+1,512);CHECK(Call(0x77,520,99)==0);if(offset<1536)CHECK(publications==before);}
 CHECK(publications==before+1&&content.slots[0].music.art_valid&&content.slots[0].music.art_rgb565[2047]==4);
 CHECK(Call(0x77,521,99)==CFW_ARGUMENT);
 PhoneContent_Links(&content,0);PhoneContent_Links(&content,1);CHECK(Call(0x78,12,100)==CFW_EXPIRED);
 W(in,100);W(in+4,50);W(in+8,0);CHECK(Call(0x78,12,100)==0&&content.slots[0].status.count==0);
}

uint32_t SettingsDevice_Lock(void){return 0;}
void SettingsDevice_Unlock(uint32_t v){(void)v;}

int32_t SettingsRemote_Handle(uint32_t op,const uint8_t *p,uint32_t n,uint32_t epoch,uint8_t *out,uint32_t *bytes){(void)op;(void)p;(void)n;(void)epoch;(void)out;*bytes=0;return CFW_ARGUMENT;}
