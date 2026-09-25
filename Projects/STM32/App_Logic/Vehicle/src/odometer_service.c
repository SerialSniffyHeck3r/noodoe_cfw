#if NOODOE_PRODUCT
#include "Odometer_Guard.h"
#include "Config_Store.h"
#include "App_Settings.h"
#include "Settings_Motion.h"
#include "PowerService.h"
#include <string.h>
#define ODO_FIELD 0x0204U
#define ODO_MAGIC 0x314F444FU
static OdometerGuard guard;
static SettingsMotion motion;
static OdometerStatus published;
static uint32_t loaded,load_error,stored_revision,write_revision,last_save,flush,request,request_revision,retry_at;
/* Explicit little-endian stable record; no C struct, timer or button state is
 * stored. Non-preference field survives current and legacy preference resets. */
static void Load(void)
{
 uint8_t b[48];uint32_t n=0,e=ConfigStore_Get(ODO_FIELD,b,sizeof(b),&n);
 if(e==CFW_PENDING)return;
 loaded=1;if(e==CFW_MISSING)return;
 if(e||n!=sizeof(b)||Cfw_Get32(b)!=ODO_MAGIC||Cfw_Get32(b+4)!=1){load_error=CFW_CORRUPT;return;}
 guard.valid=Cfw_Get32(b+8);guard.raw=Cfw_Get32(b+12);guard.display=Cfw_Get32(b+16);
 guard.pending=Cfw_Get32(b+20);guard.reason=Cfw_Get32(b+24);guard.candidate_base=Cfw_Get32(b+28);
 guard.candidate=Cfw_Get32(b+32);guard.revision=Cfw_Get32(b+36);
 if(guard.valid>1||guard.pending>1||guard.reason>ODO_RANGE||guard.raw>ODOMETER_MAX_KM||guard.display>ODOMETER_MAX_KM||
    (guard.pending&&guard.reason!=ODO_RANGE&&(guard.candidate_base>ODOMETER_MAX_KM||guard.candidate>ODOMETER_MAX_KM))||Cfw_Get32(b+40)||Cfw_Get32(b+44)){
  memset(&guard,0,sizeof(guard));load_error=CFW_CORRUPT;
 }else {guard.offset=(int32_t)guard.display-(int32_t)guard.raw;guard.corrected=guard.offset!=0;}
 stored_revision=guard.revision;
}
void OdometerGuard_GetStatus(OdometerStatus *out)
{if(!out)return;uint32_t m=SettingsDevice_Lock();*out=published;SettingsDevice_Unlock(m);}
uint32_t OdometerGuard_RequestDecision(uint32_t revision,uint32_t decision)
{
 uint32_t m=SettingsDevice_Lock(),e=CFW_OK;
 if(decision<ODO_KEEP||decision>ODO_LATER)e=CFW_ARGUMENT;
 else if(!published.ready||!published.pending||request)e=CFW_BUSY;
 else if(revision!=published.revision)e=CFW_EXPIRED;
 else if(published.reason==ODO_RANGE&&decision!=ODO_LATER)e=CFW_ARGUMENT;
 else{request=decision;request_revision=revision;}
 SettingsDevice_Unlock(m);return e;
}
/* Normal UI tick performs only bounded RAM operations. ConfigStore's existing
 * StorageTask commits and physically verifies the record asynchronously. */
void OdometerService_Tick(uint32_t now,uint32_t sample,uint32_t valid,uint32_t raw,uint32_t ign,uint32_t speed_valid,uint32_t speed)
{
 if(!loaded)Load();
 SettingsMotion_Tick(&motion,now,ign&&speed_valid,speed,0);
 uint32_t before=guard.pending;
 if(loaded&&!load_error)OdometerGuard_Feed(&guard,now,sample,valid&&ign,raw);
 uint32_t m=SettingsDevice_Lock(),choice=request,rev=request_revision;request=0;SettingsDevice_Unlock(m);
 uint32_t immediate=(!stored_revision&&guard.valid)||(!before&&guard.pending)||flush;
 uint32_t ready=motion.ready&&!PowerService_RunRequired();
 if(choice&&ready&&rev==guard.revision&&OdometerGuard_Choose(&guard,choice))immediate=choice!=ODO_LATER;
 if(write_revision){uint32_t e=ConfigStore_Result(write_revision);published.save_result=e;
  if(e!=CFW_PENDING&&e!=CFW_BUSY){write_revision=0;if(!e){last_save=now;}else{guard.dirty=1;flush=1;retry_at=now+1000U;}}}
 if(guard.dirty&&!load_error&&!write_revision&&(!retry_at||(int32_t)(now-retry_at)>=0)&&(immediate||now-last_save>=50000U)){
  uint8_t b[48]={0};Cfw_Put32(b,ODO_MAGIC);Cfw_Put32(b+4,1);Cfw_Put32(b+8,guard.valid);
  Cfw_Put32(b+12,guard.raw);Cfw_Put32(b+16,guard.display);Cfw_Put32(b+20,guard.pending);
  Cfw_Put32(b+24,guard.reason);Cfw_Put32(b+28,guard.candidate_base);Cfw_Put32(b+32,guard.candidate);
  Cfw_Put32(b+36,guard.revision);uint32_t e=ConfigStore_Set(ODO_FIELD,b,sizeof(b),&write_revision);
  published.save_result=e?e:CFW_PENDING;if(!e){guard.dirty=0;stored_revision=guard.revision;flush=0;}else {retry_at=now+1000U;if(immediate)flush=1;}
 }
 m=SettingsDevice_Lock();
 published.valid=guard.valid;published.raw=guard.pending?guard.candidate:guard.raw;published.display=guard.display;
 published.corrected=guard.corrected;published.pending=guard.pending;published.reason=guard.reason;
 published.ready=ready&&!load_error;published.revision=guard.revision;if(load_error)published.save_result=load_error;
 SettingsDevice_Unlock(m);
}
uint32_t OdometerService_TripValid(void){return guard.valid&&!guard.pending&&!guard.tracking&&!load_error;}
/* Called only for the debounced, official session end; brief key flicks do
 * not create a persistence event. StorageTask still owns the actual write. */
void OdometerService_Checkpoint(void){flush=1;}
#endif
