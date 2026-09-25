#include "Bootstrap_UI.h"
#include "MaintenanceCopy.h"
#include <string.h>
#include <stdio.h>
void BootstrapUI_Init(BootstrapUI *s)
{memset(s,0,sizeof(*s));s->state=BOOT_UI_CHECK;s->selection=1;}
/* Explicit boot-only opt-in: returning from cancellation must not replay it.
 * Initialisation and storage checks continue; no task delay or new wire state. */
void BootstrapUI_ShowWelcome(BootstrapUI *s,uint32_t now)
{s->welcome=1;s->welcome_started=now;s->now=now;}
void BootstrapUI_Context(BootstrapUI *s,uint32_t checking,uint32_t checked_files,uint32_t install_session)
{s->checking=checking;s->checked_files=checked_files;s->install_session=install_session;if(!install_session)s->work_started=0;}
void BootstrapUI_WorkStarted(BootstrapUI *s){s->work_started=1;}
/* Observe real byte/subphase changes; a live heartbeat alone is not progress.
 * A warning cannot authorize resets, retries or abandoning a physical write. */
void BootstrapUI_Track(BootstrapUI *s,uint32_t now,uint32_t subphase,uint32_t kind)
{
 s->now=now;s->subphase=subphase;s->kind=kind;
 if(!s->tracked||s->phase!=s->old_phase||s->position!=s->old_position||subphase!=s->old_subphase||kind!=s->old_kind||!s->busy){
  s->mark=now;s->tracked=1;
 }
 s->old_phase=s->phase;s->old_position=s->position;s->old_subphase=subphase;s->old_kind=kind;
 s->stalled=s->busy&&s->phase<100&&now-s->mark>=30000U;
}
uint32_t BootstrapUI_Input(BootstrapUI *s,int delta,uint32_t select,uint32_t now)
{
 /* A press dismisses the greeting; it cannot also approve a hidden menu item. */
 if(s->welcome&&!s->error&&(s->state==BOOT_UI_CHECK||s->state==BOOT_UI_READY)){
  if(delta||select)s->welcome=0;
  return BOOT_ACTION_NONE;
 }
 if(s->state==BOOT_UI_CHECK||(s->busy&&s->state!=BOOT_UI_ERROR&&s->state!=BOOT_UI_RECOVERY&&s->state!=BOOT_UI_BT_TEST)||s->state==BOOT_UI_INSTALL)return BOOT_ACTION_NONE;
 /* Only the root menu cycles five entries. Install confirmation remains a
  * separate two-choice dialog, always entered with Back selected. */
 if(delta){
  if(s->state==BOOT_UI_HELP)s->help_page=(s->help_page+(delta>0?1U:3U))%4U;
  if(s->state==BOOT_UI_READY)s->selection=(s->selection+(delta>0?1U:4U))%5U;
  else if(s->state==BOOT_UI_INSTALL_READY)s->selection^=1U;
 }
 if(!select)return BOOT_ACTION_NONE;
 if(s->state==BOOT_UI_HELP||s->state==BOOT_UI_AMBIENT){s->state=BOOT_UI_READY;return 0;}
 if(s->state==BOOT_UI_INSTALL_READY){
  if(!s->selection){s->state=BOOT_UI_READY;s->selection=1;s->install_allowed=0;s->work_started=0;return BOOT_ACTION_CANCEL;}
  s->install_allowed=1;return BOOT_ACTION_ALLOW_INSTALL;
 }
 if(s->state==BOOT_UI_CONNECT||s->state==BOOT_UI_PAUSED)return BOOT_ACTION_NONE;
 if(s->state==BOOT_UI_RECOVERY||s->state==BOOT_UI_BT_TEST){s->state=BOOT_UI_READY;s->selection=1;s->work_started=0;return 0;}
 if(s->state==BOOT_UI_ERROR){s->state=BOOT_UI_RECOVERY;return BOOT_ACTION_RECOVERY;}
 if(s->state!=BOOT_UI_READY)return BOOT_ACTION_NONE;
 if(s->selection==4){s->state=BOOT_UI_AMBIENT;return 0;}
 if(s->selection==3){s->state=BOOT_UI_HELP;return 0;}
 if(s->selection==2){s->state=BOOT_UI_RECOVERY;return BOOT_ACTION_RECOVERY;}
 s->permission_until=now+120000U;
 if(!s->selection){s->state=BOOT_UI_BT_TEST;return BOOT_ACTION_BT_TEST;}
 s->state=BOOT_UI_CONNECT;return BOOT_ACTION_PAIR;
}
/* A completed device verification, not an uploaded byte count, exposes Install.
 * phase 100=verified,101=committed,102=reset; other values are storage states. */
void BootstrapUI_Update(BootstrapUI *s,uint32_t now,uint32_t ign,uint32_t connected,
 uint32_t phase,uint32_t position,uint32_t total,uint32_t busy,uint32_t error)
{
 uint32_t before=s->state,was_connected=s->connected;
 uint32_t pending=s->install_session||s->work_started||busy||phase==100;
 s->now=now;
 if(now-s->welcome_started>=1500U||error||s->install_session||phase>=100U||
    (before!=BOOT_UI_CHECK&&before!=BOOT_UI_READY))s->welcome=0;
 s->connected=connected;s->busy=busy;s->phase=phase;s->position=position;s->total=total;
 if(before!=BOOT_UI_ERROR)s->error=error; /* Preserve the first cause until acknowledged. */
 s->can_cancel=s->install_session&&phase<101;
 if(phase==104){s->state=BOOT_UI_ERROR;s->error=error;s->install_allowed=0;return;}
 if(phase>=101){s->state=BOOT_UI_INSTALL;return;}
 /* A failed radio stays visible in its diagnostic page and does not remove
  * the offline restore menu. Trying to install with that fault still errors.
  * Storage/memory/display errors retain the ordinary system error path. */
 if(error&&before!=BOOT_UI_RECOVERY&&!((error==1||(error&0xffff0000U)==BOOT_ERR_BT)&&(before==BOOT_UI_BT_TEST||before==BOOT_UI_AMBIENT||before==BOOT_UI_HELP||before==BOOT_UI_READY||before==BOOT_UI_CHECK))){s->state=BOOT_UI_ERROR;s->install_allowed=0;return;}
 if(before==BOOT_UI_ERROR)return; /* Keep the original failure until O acknowledges it. */
 /* A socket closing is not an installation. After safe cancellation there
  * is no session to consume long-O, so never trap the root menu in PAUSED. */
 if(pending&&phase!=100&&((!ign&&!s->install_session)||(was_connected&&!connected))&&before!=BOOT_UI_RECOVERY&&before!=BOOT_UI_BT_TEST&&before!=BOOT_UI_AMBIENT&&before!=BOOT_UI_HELP){s->state=BOOT_UI_PAUSED;s->install_allowed=0;return;}
 if(before==BOOT_UI_RECOVERY||before==BOOT_UI_HELP||before==BOOT_UI_AMBIENT)return;
 /* Nine background inspections form ONE startup check. Neither their busy
  * edges nor harmless read requests may replace a deliberate menu choice. */
 if(before==BOOT_UI_BT_TEST)return;
 if(s->checking){s->state=BOOT_UI_CHECK;return;}
 if(before==BOOT_UI_PAUSED&&pending&&!connected&&phase!=100)return;
 if(phase==100){if(before!=BOOT_UI_INSTALL_READY||(was_connected&&!connected)){s->selection=0;s->install_allowed=0;}s->state=BOOT_UI_INSTALL_READY;return;}
 if(phase!=100)s->install_allowed=0;
 if(busy||s->work_started){s->state=BOOT_UI_WORK;return;}
 if(before==BOOT_UI_WORK&&s->install_session){s->state=BOOT_UI_CONNECT;return;}
 if(before==BOOT_UI_WORK||before==BOOT_UI_CHECK||before==BOOT_UI_PAUSED){s->state=BOOT_UI_READY;s->selection=1;}
 if(before==BOOT_UI_CONNECT&&!s->install_session&&!connected&&(int32_t)(s->permission_until-now)<=0)s->state=BOOT_UI_READY;
}
/* English ROM-font copy: no assets or allocation in an error path. Machine
 * codes remain separate; a failed link is not described as a successful boot. */
void BootstrapUI_View(const BootstrapUI *s,BootstrapView *v)
{
 *v=(BootstrapView){.title=COPY_BOOTSTRAP,.line1="Ready when you are.",.line2="",
  .hint="O: choose   UP/DOWN: move",.percent=101,.error=s->state==BOOT_UI_ERROR?s->error:0};
 /* Centre just the two requested lines; errors/recovery/progress take priority. */
 if(s->welcome&&!s->error&&(s->state==BOOT_UI_CHECK||s->state==BOOT_UI_READY)){
  v->line1=COPY_WELCOME;v->hint="";v->welcome=1;return;
 }
 switch(s->state){
 case BOOT_UI_CHECK:v->line1="Checking the install files...";v->line2="Checking only.";
  snprintf(v->detail,sizeof(v->detail),"File check %lu / 9",(unsigned long)(s->checked_files>9?9:s->checked_files));v->line3=v->detail;v->hint="The menu opens next.";break;
 case BOOT_UI_READY:{
  static const char *const menu[]={"Bluetooth test","Install CFW","Back to stock","Help","Ambient sensor"};
  for(uint32_t i=0;i<3;++i){uint32_t n=(s->selection+i+4U)%5U;
   snprintf(v->extra[i],sizeof(v->extra[i]),i==1?"> %s <":"%s",menu[n]);}
  v->line1=v->extra[0];v->line2=v->extra[1];v->line3=v->extra[2];break;}
 case BOOT_UI_AMBIENT:
  v->title="AMBIENT SENSOR";
  snprintf(v->extra[0],64,"ID %04lX:%04lX / RAW %04lX",(unsigned long)s->ambient[0],(unsigned long)s->ambient[1],(unsigned long)s->ambient[2]);
  if(s->ambient[4])snprintf(v->extra[1],64,"Light %lu.%03lu lux / age %lums",(unsigned long)(s->ambient[3]/1000),(unsigned long)(s->ambient[3]%1000),(unsigned long)s->ambient[5]);
  else snprintf(v->extra[1],64,"No valid light sample / error %lu",(unsigned long)s->ambient[6]);
  snprintf(v->extra[2],64,"Phase %lu / PWM %lu/%lu / %lu%%",(unsigned long)s->ambient[7],(unsigned long)s->ambient[9],(unsigned long)s->ambient[10],(unsigned long)s->ambient[8]);
  v->line1=v->extra[0];v->line2=v->extra[1];v->line3=v->extra[2];v->hint="O: back to menu";break;
 case BOOT_UI_CONNECT:v->title="INSTALL CFW";v->line1=s->connected?COPY_CONNECTED:"Open the installer on your phone.";v->line2="Tap Continue on your phone.";v->hint="Hold O 3 sec: cancel";break;
 case BOOT_UI_BT_TEST:
  v->title="BLUETOOTH TEST";
  v->line1=s->bt_state==2?"Controller: ready":s->bt_state==3?"Controller: no response":s->bt_state==1?"Controller: starting...":"Controller: off";
  v->line2=s->connected?(s->bt_secure?"SPP: connected + encrypted":"SPP: waiting for encryption"):"SPP: no phone connected";
  snprintf(v->detail,sizeof(v->detail),"RX %lu / TX %lu   Error %lu",(unsigned long)s->requests,(unsigned long)s->replies,(unsigned long)s->bt_error);
  v->line3=v->detail;
  snprintf(v->extra[0],64,s->radio[3]?"8 KiB echo OK / %lums / errors %lu":"Echo %lu/8192 B / errors %lu",(unsigned long)(s->radio[3]?s->radio[2]:s->radio[0]),(unsigned long)s->radio[4]);
  v->notice=v->extra[0];v->hint="Phone: Radio self-test / O: menu";break;
 case BOOT_UI_WORK:
  v->line1=s->phase==1?"Saving your backup: 1 of 2":s->phase==2?"Checking your backup: 2 of 2":s->phase==3?"Getting the files...":s->phase==7?"Making room for your CFW...":s->phase==12?"Finding space for your CFW...":s->phase==13?"Checking your original files...":"Checking everything matches...";
  v->hint="Hold O 3 sec: safely cancel";
  v->line2="Current step, not total install";
  {static const char *const file[]={"Resources","Settings","Ride records","Photo slots","Stock recovery","CFW slot A","CFW slot B","Boot journal","Device log"};
   snprintf(v->detail,sizeof(v->detail),"%s / idle %lus",s->kind<9?file[s->kind]:"Working",(unsigned long)((s->now-s->mark)/1000));}
  v->line3=v->detail;
  if(!s->busy){v->line1="Waiting for the next file...";v->line2="Your installation is still active.";v->percent=100;}
  if(s->stalled){v->notice="No progress. Check phone/logs.";v->hint="Keep main power. Do not resend.";}
  if(s->busy&&s->total){v->percent=(uint32_t)((uint64_t)s->position*100/s->total);}
  break;
 case BOOT_UI_INSTALL_READY:v->line1="Everything's checked. Ready?";v->line2=s->selection?"Back      > Install CFW <":"> Back <      Install CFW";v->line3="O: install, even without the phone.";break;
 case BOOT_UI_INSTALL:v->line1=s->phase==103?"Preparing stock recovery...":"Installing your CFW...";
  v->line2=s->phase==103?"Finishing the current storage job.":"It'll restart when it's ready.";
  if(s->phase==103){snprintf(v->detail,sizeof(v->detail),"Waiting %lus / 60s",(unsigned long)(s->position/1000));v->line3=v->detail;}
  v->hint="Please keep main power on.";break;
 case BOOT_UI_PAUSED:v->line1="Connection lost. Hang tight.";v->line2="Reconnect to check the actual state.";v->hint="Hold O 3 sec: safely cancel";break;
 case BOOT_UI_RECOVERY:v->title="BACK TO STOCK";v->line1="Hold O for 2 sec to restore.";v->line2="Restore original V5.16.";
  v->hint="Release early to cancel. Tap O: back";
  if(s->recovery_hold_ms)v->percent=s->recovery_hold_ms>=2000U?100U:s->recovery_hold_ms/20U;
  break;
 case BOOT_UI_ERROR:v->title="SYSTEM ERROR";v->line1="Well, shit. Check the details.";
  v->line2=(s->error&0xffff0000U)==BOOT_ERR_BT?"Bluetooth isn't answering.":(s->error&0xffff0000U)==BOOT_ERR_NOR?"Storage isn't answering.":(s->error&0xffff0000U)==BOOT_ERR_RAM?"Memory isn't ready.":(s->error&0xffff0000U)==BOOT_ERR_DISPLAY?"The screen isn't ready.":(s->error&0xffff0000U)==BOOT_ERR_STORAGE?"Storage check/write failed.":(s->error&0xffff0000U)==BOOT_ERR_UPDATE?"Update wasn't confirmed.":"Something didn't check out.";
  v->hint="Press O to get back to stock.";
  if((s->error&0xffff0000U)==BOOT_ERR_STORAGE){
   snprintf(v->detail,sizeof(v->detail),"File %lu / phase %lu.%lu",(unsigned long)s->kind,(unsigned long)s->phase,(unsigned long)s->subphase);v->line3=v->detail;
  }
  if((s->error&0xffff0000U)==BOOT_ERR_UPDATE){
   if((s->error&65535U)==12)v->line2="Install stopped before writing.";
   snprintf(v->detail,sizeof(v->detail),"Check %lu / result %lu",(unsigned long)s->commit_phase,(unsigned long)s->commit_result);v->line3=v->detail;
  }
  if((s->error&0xffff0000U)==BOOT_ERR_RECOVERY){
   v->line2="Recovery could not start.";v->line3=(s->error&65535U)==1?"An install request is unresolved.":"The storage job did not finish.";
   v->hint="Keep main power. Save phone logs.";
  }
  break;
 case BOOT_UI_HELP:{
  static const char *const a[]={"Install CFW: open the phone app.","Key OFF keeps installation alive.","Lost connection? Reconnect first.","Back to stock works without BT."};
  static const char *const b[]={"Use the ZIP for this Bootstrap.","Hold O for 3 sec to cancel.","An unclear result isn't a failure.","Release O, then hold it for 2 sec."};
  v->title="HELP";v->line1=a[s->help_page];v->line2=b[s->help_page];
  v->line3="Keep main power during writes.";v->hint="UP/DOWN: scroll    O: menu";break;}
 default:break;
 }
 if(s->cancel_pending){v->line1="Finishing the current write.";v->line2="One sec. Safe cancellation next.";v->hint="Keep main power on.";}
 if(s->install_session&&s->progress[1]==1&&s->state!=BOOT_UI_ERROR&&s->state!=BOOT_UI_RECOVERY){
  v->has_overall=1;v->overall=s->progress[5]*100/8;
  if(v->overall>=100)v->overall=99; /* Only confirmed Product may say done. */
  snprintf(v->counts,sizeof(v->counts),"Stage %lu/8  File %lu/%lu  Sector %lu/%lu",
   (unsigned long)s->progress[5],(unsigned long)(s->progress[8]<s->progress[9]?s->progress[8]+1:s->progress[9]),
   (unsigned long)s->progress[9],(unsigned long)s->progress[13],(unsigned long)s->progress[14]);
  /* Progress must never cover the selected Back/Install action. */
  if(s->state==BOOT_UI_INSTALL_READY)v->line3=v->counts;
  else v->line2=v->counts;
 }
 if(v->percent!=101&&v->percent>100)v->percent=100;
}
