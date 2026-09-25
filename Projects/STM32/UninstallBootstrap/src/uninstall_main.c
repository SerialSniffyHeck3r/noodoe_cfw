#include "uninstall_core.h"
#include "gate_board.h"
#include "Bootstrap_Image.h"
#include "Recovery_Core.h"
#include "Update_Metadata.h"
#include "BSP_Watchdog.h"
#include "stm32f4xx.h"
#include <string.h>
/* Fixed target descriptor: no host-selected addresses or stock image. */
__attribute__((section(".uninstall_descriptor"),used))
const uint32_t g_uninstall_descriptor[8]={0x31424e55,1,0x08010000,0x70000,UNINSTALL_JOURNAL_BASE,UNINSTALL_JOURNAL_BYTES,0x00100005,0};
static UninstallCore cleanup;
static RecoveryCore stock;
static uint32_t metadata_scratch[4096],progress,display_ms,display_ready,ui_state,ui_error,display_percent,display_state;
extern uint32_t GateBoard_JournalProgram(uint32_t,const void *,uint32_t);
extern void GateBoard_Uninstall(uint32_t,uint32_t,uint32_t,uint32_t);
static void Tick(void *unused,uint32_t state,uint32_t done,uint32_t total)
{(void)unused;(void)state;uint32_t now=GateBoard_Millis();
 if(!BSP_Watchdog_Checkpoint(now,++progress))for(;;){}
 if(ui_state!=display_state){display_state=ui_state;display_percent=0;}
 if(total)display_percent=done*100/total;
 if(display_ready&&now-display_ms>=200){display_ms=now;GateBoard_Uninstall(ui_state,display_percent,cleanup.bytes,ui_error);}}
static uint32_t ReadJournal(void *u,uint32_t o,void *p,uint32_t n)
{(void)u;if(!n||o>=UNINSTALL_JOURNAL_BYTES||n>UNINSTALL_JOURNAL_BYTES-o)return 1;memcpy(p,(const void*)(UNINSTALL_JOURNAL_BASE+o),n);return 0;}
static uint32_t ProgramJournal(void *u,uint32_t o,const void *p,uint32_t n)
{(void)u;return GateBoard_JournalProgram(o,p,n);}
static uint32_t Erase(void *u,uint32_t a)
{(void)u;if(GateBoard_SetWriteRange(a,4096))return 1;uint32_t r=GateBoard_NorErase(a);GateBoard_LockNor();return r;}
static uint32_t Program(void *u,uint32_t a,const void *p,uint32_t n)
{(void)u;if(GateBoard_SetWriteRange(a&~4095U,4096))return 1;uint32_t r=GateBoard_NorProgram(a,p,n);GateBoard_LockNor();return r;}
static uint32_t Source(void *u,uint32_t o,void *p,uint32_t n){return (uint32_t)BootstrapImage_Read(u,o,p,n);}
static uint32_t StageRead(void *u,uint32_t o,void *p,uint32_t n)
{(void)u;if((o|n)&1U||o>=0x70000||n>0x70000-o||GateBoard_RawRead(0,0x7f90000+o,p,n))return 1;
 uint8_t *b=p;for(uint32_t i=0;i<n;i+=2){uint8_t v=b[i];b[i]=b[i+1];b[i+1]=v;}return 0;}
static uint32_t StageEnable(void *u){(void)u;return GateBoard_SetWriteRange(0x7f90000,0x70000);}
static uint32_t StageErase(void *u,uint32_t o){(void)u;return o>=0x70000?1:GateBoard_NorErase(0x7f90000+o);}
static uint32_t StageProgram(void *u,uint32_t o,const void *p,uint32_t n)
{(void)u;uint8_t b[256];if((o|n)&1U||!n||n>256||o>=0x70000||n>0x70000-o)return 1;
 for(uint32_t i=0;i<n;i++)b[i]=((const uint8_t*)p)[i^1U];return GateBoard_NorProgram(0x7f90000+o,b,n);}
static void Lock(void *u){(void)u;GateBoard_LockNor();}
static uint32_t MetaRead(void *u,uint32_t p[5]){(void)u;return UpdateMetadata_Read(p);}
static uint32_t MetaCommit(void *u,uint32_t v,uint32_t crc)
{(void)u;return UpdateMetadata_Commit(v,crc,UPDATE_METADATA_ARM_TOKEN,metadata_scratch,sizeof(metadata_scratch));}
static void Reset(void *u){(void)u;GateBoard_Reset();}
/* Verify the embedded recovery before offering destructive approval. Source
 * verification alone grants a staging capability but does not perform writes. */
static uint32_t StockBegin(void)
{RecoveryPlatform io={0,StageRead,StageEnable,StageErase,StageProgram,Lock,MetaRead,MetaCommit,Reset};
 RecoverySource source={0,Source};RecoveryCore_Init(&stock,&io);if(RecoveryCore_Request(&stock,&source))return 1;
 while(stock.state==RECOVERY_SOURCE_HASH){RecoveryCore_Process(&stock);Tick(0,0,stock.offset,RECOVERY_STOCK_BYTES);}
 GateBoard_LockNor();return stock.state!=RECOVERY_ERASE;}
static uint32_t Restore(void)
{ui_state=5;if(StockBegin()||StageEnable(0))return 20;
 while(stock.state!=RECOVERY_VERIFIED&&stock.state!=RECOVERY_ERROR){
  RecoveryCore_Process(&stock);ui_state=stock.state==RECOVERY_STAGE_HASH?6:5;Tick(0,0,stock.offset,RECOVERY_STOCK_BYTES);}
 if(stock.state!=RECOVERY_VERIFIED)return 21+stock.result;
 ui_state=7;Tick(0,0,1,1);
 if(RecoveryCore_Commit(&stock,RECOVERY_CONFIRM_TOKEN))return 40+stock.result;
 return RecoveryCore_Reset(&stock,RECOVERY_CONFIRM_TOKEN);}
static void Error(uint32_t error)
{ui_state=8;ui_error=error;GateBoard_LockNor();(void)BSP_Watchdog_SetPhase(GateBoard_Millis(),WATCHDOG_WAIT,0);
 uint32_t released=0,start=0;
 for(;;){uint32_t now=GateBoard_Millis();Tick(0,0,0,0);
  if(!GateBoard_Enter()){released=1;start=now;}
  else if(released&&now-start>=2000)GateBoard_Reset();
 }}
/* No IGN-based power policy exists here. Once the local durable approval is
 * sealed, NOR cleanup and stock recovery have a single device-owned lifetime. */
void Uninstall_Main(void)
{if(GateBoard_Init())for(;;){}
 if(!BSP_Watchdog_Init(GateBoard_Millis(),WATCHDOG_RECOVERY,600000))for(;;){}
 display_ready=!GateBoard_DisplayInit();ui_state=0;
 if(!RecoveryTarget_Verify((const uint8_t*)0x08000000)||GateBoard_NorInit())Error(100);
 if(StockBegin())Error(101);
 uint32_t uid[3];uint8_t identity[32];GateBoard_UID(uid);
 UpdateSha256 sha;UpdateSha256_Init(&sha);UpdateSha256_Feed(&sha,(const uint8_t*)0x08000000,32768);
 UpdateSha256_Feed(&sha,(const uint8_t*)0x0800c000,16384);UpdateSha256_Final(&sha,identity);
 UninstallIO io={0,GateBoard_RawRead,Erase,Program,ReadJournal,ProgramJournal,Tick};
 if(Uninstall_Init(&cleanup,&io,uid,identity))Error(200+cleanup.error);
 if(Uninstall_Audit(&cleanup)){
  if(cleanup.approved)Error(200+cleanup.error); /* Never boot stock with partial FAT. */
  ui_state=9;ui_error=200+cleanup.error;
  (void)BSP_Watchdog_SetPhase(GateBoard_Millis(),WATCHDOG_WAIT,0);
  uint32_t released=0,start=0;
  for(;;){uint32_t now=GateBoard_Millis();Tick(0,0,0,0);
   if(!GateBoard_Enter()){released=1;start=now;}
   else if(released&&now-start>=2000)break;
  }
  ui_error=0;(void)BSP_Watchdog_SetPhase(GateBoard_Millis(),WATCHDOG_RECOVERY,600000);
  Error(Restore());
 }
 if(!cleanup.approved){
  (void)BSP_Watchdog_SetPhase(GateBoard_Millis(),WATCHDOG_WAIT,0);
  uint32_t previous=GateBoard_Keys(),released=0,start=0,keep=1; /* KEEP is the default. */
  for(;;){uint32_t now=GateBoard_Millis(),keys=GateBoard_Keys();
   if((keys&5)&~previous){keep=!keep;released=0;}
   if(!(keys&2)){released=1;start=now;}
   ui_state=keep?1:2;Tick(0,0,released&&(keys&2)?now-start:0,2000);previous=keys;
   if(released&&(keys&2)&&now-start>=2000)break;
  }
  (void)BSP_Watchdog_SetPhase(GateBoard_Millis(),WATCHDOG_RECOVERY,600000);
  if(keep)Error(Restore());
  ui_state=10;if(Uninstall_Approve(&cleanup,UNINSTALL_CONFIRM))Error(300+cleanup.error);
 }
 while(cleanup.state!=UNINSTALL_CLEAN){ui_state=cleanup.state==UNINSTALL_METADATA?4:3;
  if(Uninstall_Process(&cleanup))Error(400+cleanup.error);Tick(0,0,0,0);}
 Error(Restore());
}
