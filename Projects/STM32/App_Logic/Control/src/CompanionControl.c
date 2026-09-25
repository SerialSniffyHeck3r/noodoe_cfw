#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "RAM codec requires little-endian target"
#endif
#if NOODOE_PRODUCT
#include "CompanionControl.h"
#include "ProductUI.h"
#include "Phone_Indicators.h"
#include "App_Settings.h"
#include "Settings_Remote.h"
#include "App_Persistence.h"
#include "SettingsService.h"
#include "Cfw_Store.h"
#include "PhoneVisual.h"
#include <string.h>
static uint32_t generation,token,art_offset,call_panel;
static PhoneStatus phone;
static PhoneMusic music;
static uint32_t U(const uint8_t *p){ uint32_t v;memcpy(&v,p,4);return v;}
/* Preserve the explicit LE32 wire bytes without a byte-store loop at every
 * call site. memcpy also preserves the contract for unaligned RAM buffers. */
#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "This scalar codec requires a little-endian target"
#endif
static void W(uint8_t *p,uint32_t n){memcpy(p,&n,4);}
/* Bound legacy text/fallback fields. Music CJK uses the separate visual-key
 * and A4 transfer; notification/rider text still uses this adapter. */
static uint32_t Text(char *out,uint32_t capacity,const uint8_t *in,uint32_t n)
{if(n>=capacity||memchr(in,0,n))return 0;memset(out,0,capacity);memcpy(out,in,n);return 1;}
static void KeepPanels(void){uint32_t keys[13]={phone.header_key,phone.reply_key,call_panel};for(uint32_t i=0;i<phone.count;i++)keys[3+i]=phone.notifications[i].visual_key;PhoneVisual_KeepPanels(keys,3+phone.count);}
static uint32_t Publish(uint32_t now){KeepPanels();return ProductUI_PublishPhone(0,token,&phone,now)?0:CFW_BUSY;}
/* Keep this protocol boundary out of the install/update dispatcher under LTO.
 * Combining both large switches increases register spills and duplicated exit
 * paths; one bounded call retains exactly the same byte-level interface. */
__attribute__((noinline)) int32_t CompanionControl_Handle(uint32_t op,const uint8_t *p,uint32_t n,uint32_t epoch,uint32_t now,uint8_t *out,uint32_t *bytes)
{
 *bytes=0;
 if(op>=0x97&&op<=0x9b)return SettingsRemote_Handle(op,p,n,epoch,out,bytes);
 PhoneVisual_Session(epoch);
 uint32_t current=ProductUI_PhoneToken(0);
 if(generation!=epoch||token!=current){generation=epoch;token=current;art_offset=0;call_panel=0;memset(&phone,0,sizeof(phone));memset(&music,0,sizeof(music));}
 if(op==0x70){if(n)return CFW_ARGUMENT;
  W(out,2);W(out+4,epoch);W(out+8,g_product_ui.ign_valid);W(out+12,g_product_ui.ign_on);
  W(out+16,g_product_ui.speed_valid);W(out+20,g_product_ui.speed_kph);W(out+24,g_product_ui.power);W(out+28,ProductUI_RideSession());*bytes=32;return 0;}
 if(op==0x71){if(n!=4)return CFW_ARGUMENT;uint32_t start=U(p);if(start>64)return CFW_ARGUMENT;
  uint32_t count=0;
  for(uint32_t i=start;i<start+8;i++){uint32_t field=AppPersistence_FieldAt(i);if(!field)break;
   uint32_t key=AppPersistence_FieldKey(field);const SettingItem *item=SettingsCatalog_Find(key);if(!item)return CFW_CORRUPT;
   uint8_t *r=out+8+count*24;W(r,field);W(r+4,AppSettings_Value(key));W(r+8,item->min);W(r+12,item->max);W(r+16,item->step);W(r+20,item->kind);count++;}
  W(out,1);W(out+4,count);*bytes=8+count*24;return 0;}
 if(n<4||!epoch||U(p)!=epoch)return CFW_EXPIRED;
 p+=4;n-=4;
 if(op==0x95){
  if(n!=8)return CFW_ARGUMENT;uint32_t status[7],r=PhoneVisual_MusicTrack(U(p),U(p+4),status);
  if(r)return r;
  memcpy(out,status,sizeof(status));*bytes=sizeof(status);return 0;
 }
 if(op==0x94){
  PhoneCallsSnapshot calls;if(!token||!PhoneCalls_Decode(&calls,p,n,epoch,now))return CFW_ARGUMENT;
  if(!ProductUI_PublishCalls(&calls))return CFW_EXPIRED;
  call_panel=calls.visual_key;KeepPanels();W(out,1);W(out+4,calls.generation);W(out+8,ProductUI_CallTarget());*bytes=12;return 0;
 }
 if(op==0x7e){
  if(n<12||U(p+4)>1||U(p+8)>10||n!=12+U(p+8)*16)return CFW_ARGUMENT;
  uint32_t count=U(p+8);PhoneIndicatorEntry entries[10];
  for(uint32_t i=0;i<count;i++){const uint8_t *r=p+12+16*i;entries[i]=(PhoneIndicatorEntry){U(r),U(r+4),U(r+8),U(r+12)};}
  uint32_t lock=SettingsDevice_Lock(),ok=PhoneIndicators_Update(epoch,U(p),now,U(p+4),entries,count);SettingsDevice_Unlock(lock);
  if(!ok)return CFW_ARGUMENT;W(out,1);W(out+4,count);
  for(uint32_t i=0;i<count;i++){W(out+8+12*i,entries[i].id);W(out+12+12*i,entries[i].revision);W(out+16+12*i,entries[i].read);}
  *bytes=8+count*12;return 0;
 }
 if(op==0x72){if(n!=8)return CFW_ARGUMENT;uint32_t key=AppPersistence_FieldKey(U(p)),id=0;
  if(!key)return CFW_ARGUMENT;
  uint32_t r=AppSettings_RequestRemote(key,(int32_t)U(p+4),&id);
  W(out,id);*bytes=4;return r;}
 if(op==0x73){if(n!=4)return CFW_ARGUMENT;uint32_t result=CFW_PENDING,id=U(p),done=AppSettings_GetResult(id,&result);
  W(out,id);W(out+4,done);W(out+8,result);W(out+12,AppSettings_GetSaveResult(id));*bytes=16;return 0;}
 if(op==0x79){if(n!=16)return CFW_ARGUMENT;return PhoneVisual_Begin(epoch,U(p),U(p+4),U(p+8),U(p+12));}
 if(op==0x7a){if(n<9)return CFW_ARGUMENT;uint32_t r=PhoneVisual_Data(epoch,U(p),U(p+4),p+8,n-8);W(out,g_phone_visual.received);*bytes=4;return r;}
 if(op==0x7b){if(n!=4)return CFW_ARGUMENT;return PhoneVisual_Finish(epoch,U(p));}
 if(op==0x7c){if(n)return CFW_ARGUMENT;W(out,1);W(out+4,g_phone_visual.key);W(out+8,g_phone_visual.state);W(out+12,g_phone_visual.result);W(out+16,g_phone_visual.received);*bytes=20;return 0;}
 if(!token)return CFW_BUSY;
 if(op==0x7d){if(n!=16||U(p+8)>5)return CFW_ARGUMENT;
  phone.header_key=U(p);phone.reply_key=U(p+4);phone.reply_count=U(p+8);phone.reply_revision=U(p+12);phone.connection_epoch=epoch;return Publish(now);}

 if(op==0x74){/* Upsert to newest-first six entries, ID stable across edits. */
  if(n<7)return CFW_ARGUMENT;
  uint32_t id=U(p),a=p[4],t=p[5],b=p[6];
  if(!id||(n!=7+a+t+b&&n!=19+a+t+b)||a>=24||t>=48||b>=96)return CFW_ARGUMENT;
  PhoneNotification value={.id=id};
  if(n==19+a+t+b){value.revision=U(p+7+a+t+b);value.visual_key=U(p+11+a+t+b);value.replyable=!!U(p+15+a+t+b);}
  if(!Text(value.app,24,p+7,a)||!Text(value.title,48,p+7+a,t)||!Text(value.body,96,p+7+a+t,b))return CFW_ARGUMENT;
  uint32_t at=phone.count;for(uint32_t i=0;i<phone.count;i++)if(phone.notifications[i].id==id){at=i;break;}
  if(at<phone.count&&!memcmp(&value,&phone.notifications[at],sizeof(value)))return 0;
  if(at==phone.count&&phone.count<PHONE_NOTIFICATION_CAPACITY)phone.count++;
  if(at>=PHONE_NOTIFICATION_CAPACITY)at=PHONE_NOTIFICATION_CAPACITY-1;
  memmove(phone.notifications+1,phone.notifications,at*sizeof(PhoneNotification));
  phone.notifications[0]=value;
  phone.notifications_valid=1;return Publish(now);
 }
 if(op==0x75){if(n!=4)return CFW_ARGUMENT;uint32_t id=U(p);
  if(!id)phone.count=0;else for(uint32_t i=0;i<phone.count;i++)if(phone.notifications[i].id==id){phone.count--;memmove(phone.notifications+i,phone.notifications+i+1,(phone.count-i)*sizeof(PhoneNotification));break;}
  phone.notifications_valid=1;return Publish(now);}
 if(op==0x76){if(n<14)return CFW_ARGUMENT;uint32_t t=p[12],a=p[13];
  if((n!=14+t+a&&n!=18+t+a)||p[0]>1||p[1]>1||p[2]||p[3])return CFW_ARGUMENT;
  char title[64],artist[48];if(!Text(title,64,p+14,t)||!Text(artist,48,p+14+t,a))return CFW_ARGUMENT;
  uint32_t position=U(p+4),duration=U(p+8);if(duration&&position>duration)return CFW_ARGUMENT;
  /* A new track must never show the previous track's artwork. */
  if(strcmp(title,music.title)||strcmp(artist,music.artist)){music.art_valid=0;art_offset=0;}
  music.visual_key=n==18+t+a?U(p+14+t+a):0;
  memcpy(music.title,title,64);memcpy(music.artist,artist,48);music.valid=p[0];music.playing=p[1];music.position_ms=position;music.duration_ms=duration;return ProductUI_PublishMusic(0,token,&music,now)?0:CFW_BUSY;}
 if(op==0x77){/* Existing RGB56532x32 art, four bounded chunks, RAM only. */
  if(n<5)return CFW_ARGUMENT;
  uint32_t offset=U(p),size=n-4;
  if(!offset){art_offset=0;music.art_valid=0;}
  if(offset!=art_offset||size>512||offset>PHONE_ART_BYTES||size>PHONE_ART_BYTES-offset)return CFW_ARGUMENT;
  memcpy(music.art_rgb565+offset,p+4,size);art_offset+=size;
  if(art_offset==PHONE_ART_BYTES){music.art_valid=1;if(!ProductUI_PublishMusic(0,token,&music,now))return CFW_BUSY;}
  W(out,art_offset);*bytes=4;return 0;}
 if(op==0x78){if(n!=8||U(p)>100||U(p+4)>1)return CFW_ARGUMENT;
  phone.battery_valid=1;phone.battery_percent=U(p);phone.charging=U(p+4);return Publish(now);}
 return CFW_ARGUMENT;
}
#endif
