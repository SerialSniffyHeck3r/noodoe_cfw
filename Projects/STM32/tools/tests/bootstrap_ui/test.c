#include "Bootstrap_UI.h"
#include "Bootstrap_Confirm.h"
#include "Update_Service.h"
#include "Bootstrap_Target.h"
#include "gate_policy.h"
#include <string.h>
#define CHECK(x) do{if(!(x))return __LINE__;}while(0)
static BootstrapUI ui;
static UpdateService service;
static uint32_t locks;
static void Lock(void *ctx){(void)ctx;locks++;}
int TestUI(void)
{
 /* The greeting must not defer startup work, hide an error, or turn a dismiss
  * press into install permission. Check the timer across the tick wrap too. */
 BootstrapView welcome;
 BootstrapUI_Init(&ui);BootstrapUI_ShowWelcome(&ui,0xffffff00U);
 BootstrapUI_Context(&ui,1,3,0);
 BootstrapUI_Update(&ui,0xffffff10U,1,0,10,16,100,1,0);
 BootstrapUI_View(&ui,&welcome);
 CHECK(welcome.welcome&&!strcmp(welcome.title,"FuckNudo Bootstrap")&&!strcmp(welcome.line1,"Welcome"));
 CHECK(ui.state==BOOT_UI_CHECK&&ui.checked_files==3&&ui.position==16);
 BootstrapUI_Context(&ui,0,9,0);
 BootstrapUI_Update(&ui,1243U,1,0,8,100,100,0,0);
 BootstrapUI_View(&ui,&welcome);CHECK(ui.state==BOOT_UI_READY&&welcome.welcome);
 BootstrapUI_Update(&ui,1244U,1,0,8,100,100,0,0);
 BootstrapUI_View(&ui,&welcome);CHECK(!welcome.welcome&&!ui.install_allowed);
 BootstrapUI_ShowWelcome(&ui,2000);
 CHECK(BootstrapUI_Input(&ui,0,1,2001)==BOOT_ACTION_NONE&&!ui.welcome&&ui.state==BOOT_UI_READY);
 BootstrapUI_ShowWelcome(&ui,2002);
 BootstrapUI_Update(&ui,2003,1,0,0,0,0,0,BOOT_ERR_STORAGE|4);
 BootstrapUI_View(&ui,&welcome);CHECK(!welcome.welcome&&welcome.error==(BOOT_ERR_STORAGE|4));
 BootstrapUI_Init(&ui);CHECK(!ui.welcome); /* No replay on local cancel/reset. */
 BootstrapUI_Init(&ui);CHECK(ui.state==BOOT_UI_CHECK);
 BootstrapUI_Update(&ui,0,1,0,0,0,0,0,0);CHECK(ui.state==BOOT_UI_READY);
 CHECK(ui.selection==1);ui.selection=0;
 CHECK(BootstrapUI_Input(&ui,0,1,0)==BOOT_ACTION_BT_TEST);
 CHECK(ui.state==BOOT_UI_BT_TEST&&!ui.install_allowed);
 BootstrapUI_Update(&ui,1,1,0,0,0,0,0,1);CHECK(ui.state==BOOT_UI_BT_TEST);
 ui.bt_state=3;ui.bt_error=12;BootstrapView diagnostic;BootstrapUI_View(&ui,&diagnostic);
 CHECK(strcmp(diagnostic.title,"BLUETOOTH TEST")==0);
 CHECK(strcmp(diagnostic.line1,"Controller: no response")==0);
 CHECK(BootstrapUI_Input(&ui,0,1,2)==BOOT_ACTION_NONE&&ui.state==BOOT_UI_READY);
 BootstrapUI_Update(&ui,3,1,0,0,0,0,0,1);CHECK(ui.state==BOOT_UI_READY);
 CHECK(BootstrapUI_Input(&ui,1,1,4)==BOOT_ACTION_RECOVERY);
 CHECK(ui.state==BOOT_UI_RECOVERY);BootstrapUI_Input(&ui,0,1,5);
 CHECK(BootstrapUI_Input(&ui,0,1,6)==BOOT_ACTION_PAIR);
 CHECK(ui.state==BOOT_UI_CONNECT);
 BootstrapUI_Update(&ui,7,1,0,0,0,0,0,1);CHECK(ui.state==BOOT_UI_ERROR);
 BootstrapUI_Init(&ui);BootstrapUI_Update(&ui,0,1,0,0,0,0,0,0);
 CHECK(BootstrapUI_Input(&ui,0,1,0)==BOOT_ACTION_PAIR);
 BootstrapUI_Update(&ui,120000,1,0,0,0,0,0,0);CHECK(ui.state==BOOT_UI_READY);
 BootstrapUI_Update(&ui,120001,1,1,3,99,100,1,0);CHECK(ui.state==BOOT_UI_WORK);
 CHECK(BootstrapUI_Input(&ui,0,1,120002)==0);CHECK(!ui.install_allowed);
 BootstrapUI_Update(&ui,120003,1,1,100,100,100,0,0);CHECK(ui.state==BOOT_UI_INSTALL_READY&&ui.selection==0);
 CHECK(BootstrapUI_Input(&ui,0,1,120004)==BOOT_ACTION_CANCEL);CHECK(!ui.install_allowed);
 BootstrapUI_Update(&ui,120005,1,1,0,0,0,0,0);CHECK(ui.state==BOOT_UI_READY);
 BootstrapUI_Update(&ui,120006,1,1,100,100,100,0,0);
 CHECK(BootstrapUI_Input(&ui,1,1,120007)==BOOT_ACTION_ALLOW_INSTALL);CHECK(ui.install_allowed);
 BootstrapUI_Update(&ui,120008,1,0,100,100,100,0,0);CHECK(ui.state==BOOT_UI_INSTALL_READY&&!ui.install_allowed);
 BootstrapUI_Update(&ui,120009,1,1,100,100,100,0,0);CHECK(ui.selection==0&&!ui.install_allowed);
 BootstrapUI_Input(&ui,1,1,120010);
 BootstrapUI_Update(&ui,120011,0,1,100,100,100,0,0);CHECK(ui.install_allowed);
 BootstrapUI_Update(&ui,120012,1,1,7,0,100,1,1);CHECK(ui.state==BOOT_UI_ERROR);
 CHECK(BootstrapUI_Input(&ui,0,1,120013)==BOOT_ACTION_RECOVERY);
 BootstrapUI_Update(&ui,120014,0,0,7,0,100,1,1);CHECK(ui.state==BOOT_UI_RECOVERY);
 BootstrapView view;BootstrapUI_View(&ui,&view);CHECK(strcmp(view.title,"BACK TO STOCK")==0);
 BootstrapUI_Update(&ui,120015,1,1,101,0,0,1,0);CHECK(ui.state==BOOT_UI_INSTALL);
 CHECK(!BootstrapUI_Input(&ui,1,1,120016));
 BootstrapUI_Update(&ui,120017,0,0,103,55000,60000,1,0);
 BootstrapUI_View(&ui,&view);CHECK(strstr(view.line1,"Preparing")&&strstr(view.line3,"55s"));
 BootstrapUI_Update(&ui,120018,0,0,104,60000,60000,0,BOOT_ERR_RECOVERY|1);
 CHECK(ui.state==BOOT_UI_ERROR&&ui.error==(BOOT_ERR_RECOVERY|1));
 BootstrapUI_View(&ui,&view);CHECK(strstr(view.line2,"could not")&&strstr(view.line3,"unresolved"));
 BootstrapUI_Init(&ui);ui.commit_phase=5;ui.commit_result=6;
 BootstrapUI_Update(&ui,0,1,1,0,0,0,0,BOOT_ERR_UPDATE|12);
 BootstrapUI_View(&ui,&view);CHECK(strstr(view.line2,"before writing")&&strstr(view.line3,"Check 5"));
 return 0;
}
int TestCancel(void)
{
 for(uint32_t state=UPDATE_IDLE;state<=UPDATE_FAILED;state++){
  memset(&service,0,sizeof(service));locks=0;service.platform.disable=Lock;
  service.state=state;service.authorization=1;service.request_write=2;service.reply_write=1;
  uint32_t r=UpdateService_CancelUncommitted(&service);
  if(state==UPDATE_COMMITTED||state==UPDATE_RESET_WAIT){CHECK(r==UPDATE_PENDING);CHECK(service.state==state&&!locks);}
  else{CHECK(!r&&service.state==UPDATE_IDLE&&!service.authorization&&locks==1);CHECK(service.request_read==2&&service.reply_read==1);}
 }
 service.state=UPDATE_FAILED;service.result=UPDATE_COMMIT_AMBIGUOUS;locks=0;
 CHECK(UpdateService_CancelUncommitted(&service)==UPDATE_PENDING&&!locks);
 return 0;
}
int TestInstallLifetime(void)
{
 BootstrapUI_Init(&ui);
 for(uint32_t file=0;file<9;file++){
  BootstrapUI_Context(&ui,1,file,0);
  BootstrapUI_Update(&ui,file*100,1,0,10,1,100,1,0);CHECK(ui.state==BOOT_UI_CHECK);
  CHECK(!BootstrapUI_Input(&ui,0,1,file*100));
  BootstrapUI_Update(&ui,file*100+1,1,0,8,100,100,0,0);CHECK(ui.state==BOOT_UI_CHECK);
 }
 BootstrapUI_Context(&ui,0,9,0);BootstrapUI_Update(&ui,1000,1,0,8,100,100,0,0);CHECK(ui.state==BOOT_UI_READY&&ui.selection==1);
 ui.selection=0;CHECK(BootstrapUI_Input(&ui,0,1,1001)==BOOT_ACTION_BT_TEST);
 BootstrapUI_Update(&ui,1002,1,1,10,0,100,1,0);CHECK(ui.state==BOOT_UI_BT_TEST);
 BootstrapUI_Input(&ui,0,1,1003);CHECK(ui.state==BOOT_UI_READY);
 ui.busy=0;CHECK(BootstrapUI_Input(&ui,0,1,1004)==BOOT_ACTION_PAIR);
 BootstrapUI_Context(&ui,0,9,1);BootstrapUI_WorkStarted(&ui);
 for(uint32_t file=0;file<9;file++){
  BootstrapUI_Update(&ui,2000+file*100,1,1,7,1,100,1,0);CHECK(ui.state==BOOT_UI_WORK);
  for(uint32_t gap=0;gap<100;gap++){
   BootstrapUI_Update(&ui,2001+file*100+gap,1,1,8,100,100,0,0);CHECK(ui.state==BOOT_UI_WORK);
   CHECK(!BootstrapUI_Input(&ui,0,1,0));
  }
 }
 BootstrapUI_Update(&ui,4000,1,1,100,100,100,0,0);CHECK(ui.state==BOOT_UI_INSTALL_READY&&!ui.install_allowed);
 CHECK(BootstrapUI_Input(&ui,0,1,4001)==BOOT_ACTION_CANCEL);
 BootstrapUI_Context(&ui,0,9,0);BootstrapUI_Update(&ui,4002,1,1,0,0,0,0,0);CHECK(ui.state==BOOT_UI_READY&&!ui.work_started);
 /* The phone closes its socket after observing a cancelled confirmation.
  * With no install owner left, PAUSED would have no working long-O handler. */
 BootstrapUI_Update(&ui,4003,1,0,0,0,0,0,0);CHECK(ui.state==BOOT_UI_READY);
 BootstrapUI_Update(&ui,4004,0,0,0,0,0,0,0);CHECK(ui.state==BOOT_UI_READY);
 BootstrapUI_Update(&ui,4005,1,0,0,0,0,0,0);
 CHECK(BootstrapUI_Input(&ui,1,1,4006)==BOOT_ACTION_RECOVERY);
 /* Real installation work must remain paused across repeated loop ticks,
  * except a verified image, which permits a fresh local approval offline. */
 BootstrapUI_Init(&ui);BootstrapUI_Context(&ui,0,9,1);
 BootstrapUI_Update(&ui,5000,1,1,100,100,100,0,0);
 CHECK(BootstrapUI_Input(&ui,1,1,5001)==BOOT_ACTION_ALLOW_INSTALL);
 BootstrapUI_Update(&ui,5002,0,0,100,100,100,0,0);
 CHECK(ui.state==BOOT_UI_INSTALL_READY&&!ui.install_allowed&&ui.can_cancel);
 BootstrapUI_Update(&ui,6002,0,0,100,100,100,0,0);CHECK(ui.state==BOOT_UI_INSTALL_READY);
 BootstrapUI_Update(&ui,6003,0,1,100,100,100,0,0);
 CHECK(ui.state==BOOT_UI_INSTALL_READY&&!ui.install_allowed&&ui.selection==0);
 /* A real install carries progress too: keep both confirmation choices
  * visible rather than replacing the selected action with sector counts. */
 ui.progress[1]=1;ui.progress[5]=4;
 BootstrapView confirm;BootstrapUI_View(&ui,&confirm);
 CHECK(strstr(confirm.line2,"> Back <")&&strstr(confirm.line3,"Stage"));
 CHECK(BootstrapUI_Input(&ui,1,0,6004)==BOOT_ACTION_NONE);
 BootstrapUI_View(&ui,&confirm);
 CHECK(strstr(confirm.line2,"> Install CFW <")&&confirm.has_overall);
 return 0;
}
int TestProgress(void)
{
 BootstrapUI_Init(&ui);
 BootstrapUI_Update(&ui,100,1,1,7,20,100,1,0);BootstrapUI_Track(&ui,100,2,4);
 CHECK(!ui.stalled);BootstrapUI_Track(&ui,30100,2,4);CHECK(ui.stalled);
 BootstrapView v;BootstrapUI_View(&ui,&v);CHECK(v.notice&&v.percent==20);
 /* Successful status/heartbeat traffic must not erase a real storage stall. */
 ui.requests++;ui.replies++;BootstrapUI_Track(&ui,30101,2,4);CHECK(ui.stalled);
 ui.position++;BootstrapUI_Track(&ui,30102,2,4);CHECK(!ui.stalled);
 BootstrapUI_Track(&ui,70102,3,4);CHECK(!ui.stalled);
 BootstrapUI_Track(&ui,100102,3,4);CHECK(ui.stalled);
 BootstrapUI_Update(&ui,100103,1,1,7,21,100,1,BOOT_ERR_STORAGE|4);
 BootstrapUI_View(&ui,&v);CHECK(ui.state==BOOT_UI_ERROR&&v.error==(BOOT_ERR_STORAGE|4));
 CHECK(strcmp(v.line2,"Storage check/write failed.")==0);
 BootstrapUI_Update(&ui,100104,1,1,8,100,100,0,0);CHECK(ui.state==BOOT_UI_ERROR&&ui.error==(BOOT_ERR_STORAGE|4));
 BootstrapUI_Update(&ui,100104,1,1,8,100,100,0,BOOT_ERR_BT|12);CHECK(ui.error==(BOOT_ERR_STORAGE|4));
 CHECK(BootstrapUI_Input(&ui,0,1,100105)==BOOT_ACTION_RECOVERY);
 BootstrapUI_Update(&ui,100106,1,1,8,100,100,0,0);CHECK(ui.state==BOOT_UI_RECOVERY);
 BootstrapUI_Init(&ui);BootstrapUI_Update(&ui,0xfffffff0U,1,1,7,1,100,1,0);
 BootstrapUI_Track(&ui,0xfffffff0U,2,4);BootstrapUI_Track(&ui,29984,2,4);CHECK(ui.stalled);
 BootstrapUI_Update(&ui,30000,1,1,100,100,100,0,0);BootstrapUI_Track(&ui,70000,2,4);CHECK(!ui.stalled);
 return 0;
}
int TestGesture(void)
{
 /* Menu/early Bootstrap confirmation does not depend on IGN. Require a
  * release after entry, cancel partial holds, fire once and handle tick wrap. */
 BootstrapConfirm hold;BootstrapConfirm_Init(&hold);
 CHECK(!BootstrapConfirm_Process(&hold,0,1));
 CHECK(!BootstrapConfirm_Process(&hold,6000,1));
 CHECK(!BootstrapConfirm_Process(&hold,6001,0));
 CHECK(!BootstrapConfirm_Process(&hold,6010,1));
 CHECK(!BootstrapConfirm_Process(&hold,8009,1));
 CHECK(BootstrapConfirm_Process(&hold,8010,1)&&hold.elapsed==2000);
 CHECK(!BootstrapConfirm_Process(&hold,9000,1));
 BootstrapConfirm_Init(&hold);BootstrapConfirm_Process(&hold,0,0);
 BootstrapConfirm_Process(&hold,10,1);BootstrapConfirm_Process(&hold,1900,0);
 CHECK(!BootstrapConfirm_Process(&hold,2000,1));
 CHECK(!BootstrapConfirm_Process(&hold,3999,1));CHECK(BootstrapConfirm_Process(&hold,4000,1));
 BootstrapConfirm_Init(&hold);BootstrapConfirm_Process(&hold,0xfffffff0U,0);
 BootstrapConfirm_Process(&hold,0xfffffff8U,1);
 CHECK(!BootstrapConfirm_Process(&hold,1991,1));CHECK(BootstrapConfirm_Process(&hold,1992,1));
 BootstrapUI_Init(&ui);ui.state=BOOT_UI_RECOVERY;ui.recovery_hold_ms=1000;
 BootstrapView restore;BootstrapUI_View(&ui,&restore);
 CHECK(restore.percent==50&&strcmp(restore.line1,"Hold O for 2 sec to restore.")==0);
 /* The independent Gate emergency key gesture remains unchanged. */
 GateGesture g;GateGesture_Init(&g,0,1);
 for(uint32_t t=0;t<4000;t+=10)CHECK(!GateGesture_Process(&g,t,1,1));
 for(uint32_t t=4000;t<4600;t+=10)CHECK(!GateGesture_Process(&g,t,0,1));
 CHECK(!GateGesture_Process(&g,4600,1,1));
 CHECK(!GateGesture_Process(&g,6599,1,1));
 CHECK(GateGesture_Process(&g,6600,1,1));
 CHECK(!GateGesture_Process(&g,6601,1,1));
 GateGesture_Init(&g,0,0);
 GateGesture_Process(&g,0,0,0);GateGesture_Process(&g,100,0,1);
 GateGesture_Process(&g,600,0,1);GateGesture_Process(&g,800,1,1);
 CHECK(!GateGesture_Process(&g,2799,1,0));CHECK(!GateGesture_Process(&g,2800,1,1));
 return 0;
}
static uint32_t mode,ticks;
static uint32_t Raw(uint32_t a,void *dst,uint32_t n)
{
 if(mode==4)return 1;
 const uint8_t *p=(const uint8_t*)(0x11000000U+a-UPDATE_STAGE_BASE);
 uint8_t *d=dst;for(uint32_t i=0;i<n;i++)d[i]=p[i^1U];return 0;
}
static uint32_t Now(void){if(mode==5)ticks+=1600;return ticks;}
int TestTarget(uint32_t argument)
{
 mode=argument;uint8_t (*stage)[0x70000]=(void*)0x11000000U;
 uint8_t slots[2][32],journal[32];UpdateSha256 hash;
 UpdateSha256_Init(&hash);UpdateSha256_Feed(&hash,*stage+0x10000,0x60000);UpdateSha256_Final(&hash,slots[0]);
 memcpy(slots[1],slots[0],32);memcpy(journal,slots[0],32);
 if(mode==1)slots[0][0]^=1;
 if(mode==2)slots[1][0]^=1;
 if(mode==3)journal[0]^=1;
 if(mode==6)(*stage)[4]=0; /* Non-Thumb gate vector. */
 if(mode==7)(*stage)[0x10004]=0;
 uint8_t requirement[44];memcpy(requirement,*stage+0x10200,44);
 if(mode==8)requirement[15]^=1;
 uint32_t result=BootstrapTarget_Check(Raw,Now,slots,journal,requirement);
 CHECK(result==(mode==0?0:mode==4||mode==5?UPDATE_IO:mode==6||mode==7?UPDATE_VECTOR:UPDATE_HASH));
 return 0;
}
