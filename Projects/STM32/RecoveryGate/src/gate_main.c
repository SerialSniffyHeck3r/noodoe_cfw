#include "gate_board.h"
#include "gate_menu.h"
#include "stm32f4xx.h"
#include "gate_store.h"
#include "gate_policy.h"
#include "gate_engine.h"
#include "Recovery_Core.h"
#include "Update_Metadata.h"
#include "Resources.h"
#include "BSP_Watchdog.h"
#include "event_log.h"
#include <string.h>
__attribute__((section(".gate_mailbox"),used)) volatile GateMailbox g_recovery_mailbox;
/* Fixed feature discovery for the running Product; packaging still binds the
 * complete Gate SHA. This never changes the stock BL or factory addresses. */
__attribute__((section(".gate_features"),used)) const uint32_t gate_features[4]={GATE_FEATURE_MAGIC,1,GATE_FEATURE_DIAGNOSTIC,~GATE_FEATURE_DIAGNOSTIC};
/* All work arenas are internal SRAM. No allocator, IRQ-owned IO, external RAM
 * or application peripheral state participates in the recovery path. */
static GateStore store;
static GateJournalRecord journal;
static GateImageInfo image;
static GateEngine engine;
static RecoveryCore stock;
static uint32_t metadata_scratch[4096],uid[3],journal_index,progress,display_ms,display_ready;
static uint8_t scratch[4096];
static EventLog events;
static uint32_t entry_reason,reset_cause,fault_cause;
volatile uint32_t g_gate_log_error;
static uint32_t LogRead(void *c,uint32_t o,void *p,uint32_t n){(void)c;return GateStore_Read(&store,GATE_FILE_LOG,o,p,n);}
static uint32_t LogGrant(void *c){(void)c;return 0;}
static uint32_t LogErase(void *c,uint32_t o)
{(void)c;uint32_t a;if(GateStore_Address(&store,GATE_FILE_LOG,o,&a)||GateBoard_SetWriteRange(a,4096))return 1;
 uint32_t r=GateBoard_NorErase(a);GateBoard_LockNor();return r;}
static uint32_t LogProgram(void *c,uint32_t o,const void *p,uint32_t n)
{(void)c;uint32_t a;uint8_t swapped[256];const uint8_t *b=p;
 if(n>256||(n&1)||GateStore_Address(&store,GATE_FILE_LOG,o,&a)||GateBoard_SetWriteRange(a&~4095U,4096))return 1;
 for(uint32_t i=0;i<n;i++)swapped[i]=b[i^1U];
 uint32_t r=GateBoard_NorProgram(a,swapped,n);GateBoard_LockNor();return r;}
static uint32_t U(const uint8_t *p){return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void Mailbox(uint32_t reason,uint32_t sequence,uint32_t attempts)
{
 /* This is a new handoff from the audited journal, not a modification of an
  * already sealed request. Request() validates its input CRC and reinitializes
  * invalid records: calling it after changing sequence/attempts silently lost
  * both fields. Seal the complete handoff once, then publish it before jump. */
 GateRetained r;GateRetained_Init(&r);
 r.reason=reason;r.sequence=sequence;r.attempts=attempts;
 r.crc=GatePolicy_Crc(&r,24);r.inverse=~r.crc;
 memcpy((void*)&g_recovery_mailbox.request,&r,sizeof(r));
}
static void Tick(uint32_t state,uint32_t percentage,uint32_t error)
{uint32_t now=GateBoard_Millis();if(!BSP_Watchdog_Checkpoint(now,++progress))for(;;){}
 if(display_ready&&now-display_ms>=200){display_ms=now;GateBoard_Display(state,percentage,error);}}
/* Logs are optional for boot and local restoration. The boot transaction
 * journal remains mandatory; an unavailable event log is separately exposed. */
static void LogBoot(uint32_t reason)
{EventLogIO io={0,LogRead,LogGrant,LogErase,LogProgram};EventLog_Init(&events,&io,uid);
 while(events.phase==LOG_SCAN){EventLog_Process(&events);Tick(1,0,0);}
 if(events.phase!=LOG_READY){g_gate_log_error=events.error;return;}
 DeviceEvent batch[2]={{.code=LOG_BOOT,.boot=journal.sequence,.detail=reason}};uint32_t count=1;
 GateFault fault;memcpy(&fault,(const void*)&g_recovery_mailbox.fault,sizeof(fault));
 if(GateFault_Valid(&fault)){batch[1].code=LOG_FAULT;batch[1].boot=fault.boot;batch[1].detail=fault.code;memcpy(batch[1].data,fault.registers,32);count=2;}
 for(uint32_t i=0;i<count;i++){batch[i].count=1;batch[i].time_ms=GateBoard_Millis();batch[i].first_ms=batch[i].last_ms=batch[i].time_ms;}
 if(!EventLog_Submit(&events,batch,count)){g_gate_log_error=7;return;}
 while(events.phase!=LOG_READY&&events.phase!=LOG_ERROR){EventLog_Process(&events);Tick(1,0,0);}
 g_gate_log_error=events.error;if(!events.error&&count==2){g_recovery_mailbox.fault.magic=0;__asm volatile("dsb":::"memory");}
}
/* Select one completed record only. Equal sequence with different bytes and
 * half-range ambiguous histories fail closed rather than choosing by address. */
static uint32_t JournalScan(void)
{uint32_t found=0;GateJournalRecord r;
 for(uint32_t i=0;i<GATE_JOURNAL_RECORDS;i++){
  if(GateStore_Read(&store,GATE_FILE_BOOT,i*4096,scratch,4096))return 1;
  if(U(scratch)==GATE_JOURNAL_MAGIC&&U(scratch+4)>2&&U(scratch+4092)==GATE_COMMIT&&U(scratch+4088)==GatePolicy_Crc(scratch,4088))return 1;
  if(GateJournal_Decode(scratch,uid,&r)){
   if(found&&r.sequence==journal.sequence&&memcmp(&r,&journal,sizeof(r)))return 1;
   if(found&&(uint32_t)(r.sequence-journal.sequence)==0x80000000U)return 1;
   if(!found||GateSequence_Newer(r.sequence,journal.sequence)){journal=r;journal_index=i;found=1;}}
  Tick(1,i*100/16,0);
 }return !found;}
/* Last valid record is never erased. Complete physical readback precedes the
 * commit word; a final reread decodes the same UID-bound record. */
static uint32_t JournalAppend(void *unused,const GateJournalRecord *r)
{(void)unused;uint32_t index=(journal_index+1)%16,address;
 if(!GateSequence_Newer(r->sequence,journal.sequence)||memcmp(r->uid,uid,12)||GateStore_Address(&store,GATE_FILE_BOOT,index*4096,&address))return 1;
 GateJournal_Encode(scratch,r);uint32_t expected=U(scratch+4088);
 if(GateBoard_SetWriteRange(address,4096)||GateBoard_NorErase(address))goto fail;
 /* Pair swap exactly once and program all bytes except the final commit. */
 for(uint32_t off=0;off<4092;){uint8_t page[256];uint32_t n=4092-off;if(n>256)n=256;
  for(uint32_t i=0;i<n;i++)page[i]=scratch[off+(i^1U)];
  if(GateBoard_NorProgram(address+off,page,n)){goto fail;}off+=n;Tick(2,off*100/4096,0);}
 if(GateStore_Read(&store,GATE_FILE_BOOT,index*4096,scratch,4096)||U(scratch+4092)!=~0U||U(scratch+4088)!=expected||GatePolicy_Crc(scratch,4088)!=expected)goto fail;
 GateJournalRecord readback;
 {const uint8_t marker[4]={0x43,0x4d,0x54,0x31};memcpy(scratch+4092,marker,4);}
 if(!GateJournal_Decode(scratch,uid,&readback)||memcmp(r,&readback,sizeof(readback)))goto fail;
 {uint8_t marker[4]={0x4d,0x43,0x31,0x54};if(GateBoard_NorProgram(address+4092,marker,4))goto fail;}
 GateBoard_LockNor();
 if(GateStore_Read(&store,GATE_FILE_BOOT,index*4096,scratch,4096)||!GateJournal_Decode(scratch,uid,&readback)||memcmp(r,&readback,sizeof(readback)))return 1;
 journal=*r;journal_index=index;return 0;
 fail:GateBoard_LockNor();return 1;}
/* No previous CFW is a real first-install failure. Persist WAIT and the cause
 * before entering the local recovery UI so cold power loss cannot retry it. */
static uint32_t RollbackOrWait(uint32_t why)
{GateJournalRecord next=journal;
 if(!GateJournal_RequestRollback(&next,why)){
  next.state=GATE_J_WAIT;next.flags=GATE_F_RESULT_PENDING;
  next.failure_reason=why;next.candidate=GATE_NO_SLOT;next.sequence++;
 }
 if(JournalAppend(0,&next))return 1;
 if(events.phase==LOG_READY){DeviceEvent e={.code=LOG_ROLLBACK,.detail=why,
   .boot=journal.sequence,.transaction=journal.transaction,.count=1};
  e.time_ms=e.first_ms=e.last_ms=GateBoard_Millis();e.data[0]=journal.failed_version;e.data[1]=journal.restored_version;
  if(EventLog_Submit(&events,&e,1))while(events.phase!=LOG_READY&&events.phase!=LOG_ERROR){EventLog_Process(&events);Tick(1,0,0);}
  g_gate_log_error=events.error;
 }
 return journal.state==GATE_J_WAIT;
}
#ifdef GATE_JOURNAL_TESTING
/* Production journal writer, substituted physical I/O only. */
void GateJournalTest_Seed(const GateJournalRecord *record,uint32_t index)
{journal=*record;journal_index=index;memcpy(uid,record->uid,12);display_ready=0;}
uint32_t GateJournalTest_Append(const GateJournalRecord *record){return JournalAppend(0,record);}
uint32_t GateJournalTest_Scan(GateJournalRecord *record){uint32_t r=JournalScan();*record=journal;return r;}
#endif
static uint32_t Source(void *unused,uint32_t slot,uint32_t offset,void *p,uint32_t n)
{(void)unused;return slot>1?1:GateStore_Read(&store,slot,4096+offset,p,n);}
static uint32_t Erase(void *unused,uint32_t sector){(void)unused;return GateBoard_FlashErase(sector);}
static uint32_t Program(void *unused,uint32_t address,const void *p,uint32_t n){(void)unused;return GateBoard_FlashProgram(address,p,n);}
static uint32_t ImageHeader(uint32_t slot)
{return slot>1||GateStore_Read(&store,slot,0,scratch,4096)||!GateImage_Decode(scratch,uid,&image);}
static uint32_t ResourceMatch(void)
{for(uint32_t slot=0;slot<2;slot++){
  if(GateStore_Read(&store,GATE_FILE_RESOURCES,slot*0x80000,scratch,4096))return 1;
  if(Resources_CheckHeader(scratch,image.requirement+12)!=RESOURCE_OK)continue;
  uint32_t total=U(scratch+8),count=U(scratch+12);UpdateSha256 sha;uint8_t digest[32];
  UpdateSha256_Init(&sha);UpdateSha256_Feed(&sha,scratch+48,count*16);
  for(uint32_t off=0;off<total;){uint32_t n=total-off;if(n>4096)n=4096;
   if(GateStore_Read(&store,GATE_FILE_RESOURCES,slot*0x80000+4096+off,scratch,n))return 1;
   UpdateSha256_Feed(&sha,scratch,n);off+=n;Tick(3,off*100/total,0);}
  UpdateSha256_Final(&sha,digest);if(!memcmp(digest,image.requirement+12,32))return 0;
 }return 1;}
static uint32_t EngineRun(uint32_t slot,uint32_t install)
{if(ImageHeader(slot)||(image.kind==GATE_IMAGE_PRODUCT&&ResourceMatch()))return 10;
 GateEngineIO io={0,Source,GateBoard_FlashRead,Erase,Program,JournalAppend};
 if(!GateEngine_Begin(&engine,&io,&journal,&image,slot,install))return 11;
 while(engine.state!=GATE_ENGINE_READY&&engine.state!=GATE_ENGINE_FAILED){GateEngine_Process(&engine);Tick(4,engine.offset*100/GATE_PRODUCT_BYTES,engine.error);}
 return engine.state==GATE_ENGINE_READY?0:20+engine.error;}
static uint32_t StockSource(void *unused,uint32_t offset,void *p,uint32_t n)
{(void)unused;return GateStore_Read(&store,GATE_FILE_STOCK,4096+offset,p,n);}
static uint32_t StageRead(void *unused,uint32_t offset,void *p,uint32_t n)
{(void)unused;if(offset>=0x70000||n>0x70000-offset||GateBoard_RawRead(0,0x7f90000+offset,p,n))return 1;
 uint8_t *b=p;for(uint32_t i=0;i<n;i+=2){uint8_t a=b[i];b[i]=b[i+1];b[i+1]=a;}return 0;}
static uint32_t StageEnable(void *unused){(void)unused;return GateBoard_SetWriteRange(0x7f90000,0x70000);}
static uint32_t StageErase(void *unused,uint32_t offset){(void)unused;return offset>=0x70000?1:GateBoard_NorErase(0x7f90000+offset);}
static uint32_t StageProgram(void *unused,uint32_t offset,const void *p,uint32_t n)
{(void)unused;uint8_t pair[256];if(offset>=0x70000||n>0x70000-offset||n>256||(n&1))return 1;
 const uint8_t *b=p;for(uint32_t i=0;i<n;i++)pair[i]=b[i^1U];return GateBoard_NorProgram(0x7f90000+offset,pair,n);}
static void StageDisable(void *unused){(void)unused;GateBoard_LockNor();}
static uint32_t MetaRead(void *unused,uint32_t *words){(void)unused;return UpdateMetadata_Read(words);}
static uint32_t MetaCommit(void *unused,uint32_t version,uint32_t crc)
{(void)unused;return UpdateMetadata_Commit(version,crc,UPDATE_METADATA_ARM_TOKEN,metadata_scratch,sizeof(metadata_scratch));}
static void Reset(void *unused){(void)unused;GateBoard_Reset();}
static uint32_t RestoreStock(void)
{if(!RecoveryTarget_Verify((const uint8_t*)0x08000000)||GateStore_Read(&store,GATE_FILE_STOCK,0,scratch,4096)||RecoveryStore_CheckHeader(scratch,uid)||RecoveryStore_CheckTarget(scratch,(const uint8_t*)0x08000000U))return 40;
 RecoveryPlatform io={0,StageRead,StageEnable,StageErase,StageProgram,StageDisable,MetaRead,MetaCommit,Reset};RecoverySource source={0,StockSource};
 RecoveryCore_Init(&stock,&io);if(RecoveryCore_Request(&stock,&source))return 41;
 while(stock.state!=RECOVERY_VERIFIED&&stock.state!=RECOVERY_ERROR){RecoveryCore_Process(&stock);Tick(stock.state==RECOVERY_SOURCE_HASH?8:stock.state==RECOVERY_STAGE_HASH?10:9,stock.offset*100/RECOVERY_STOCK_BYTES,stock.result);}
 if(stock.state!=RECOVERY_VERIFIED)return 42+stock.result;
 Tick(11,100,0);if(RecoveryCore_Commit(&stock,RECOVERY_CONFIRM_TOKEN))return 60;
 return RecoveryCore_Reset(&stock,RECOVERY_CONFIRM_TOKEN);}
/* Verified means the complete UID/BL-bound source was physically read and
 * matched. A header or a filename alone never earns the green label. */
static uint32_t StockVerified(void)
{
 if(store.state!=GATE_STORE_READY||GateStore_Read(&store,GATE_FILE_STOCK,0,scratch,4096)||
  RecoveryStore_CheckHeader(scratch,uid)||RecoveryStore_CheckTarget(scratch,(const uint8_t*)0x08000000))return 0;
 UpdateSha256 sha;uint8_t digest[32];UpdateSha256_Init(&sha);
 for(uint32_t off=0;off<RECOVERY_STOCK_BYTES;off+=4096){
  if(StockSource(0,off,scratch,4096))return 0;
  UpdateSha256_Feed(&sha,scratch,4096);Tick(8,off*100/RECOVERY_STOCK_BYTES,0);
 }UpdateSha256_Final(&sha,digest);return !memcmp(digest,recovery_stock_sha256,32);
}
static void Wait(uint32_t error)
{
 GateBoard_LockNor();Mailbox(GATE_REASON_WAIT,journal.sequence,journal.attempts);
 GateMenuFacts facts={.verified=StockVerified(),.reason=entry_reason,.reset=reset_cause,.fault=fault_cause,
  .failed=journal.failed_version,.code=error,.log=g_gate_log_error};
 (void)BSP_Watchdog_SetPhase(GateBoard_Millis(),WATCHDOG_WAIT,0);
 GateGesture gesture;GateGesture_Init(&gesture,GateBoard_Millis(),GateBoard_Enter());
 GateMenu menu;GateMenu_Init(&menu,GateBoard_Keys(),GateBoard_Millis());
 for(;;){
  uint32_t now=GateBoard_Millis();
  if(!BSP_Watchdog_Checkpoint(now,++progress))for(;;){}
  if(display_ready&&now-display_ms>=200){display_ms=now;GateBoard_Menu(&menu,&facts);}
  uint32_t approved=GateMenu_Process(&menu,GateBoard_Keys(),now);
  approved|=GateGesture_Process(&gesture,now,GateBoard_IgnOn(),GateBoard_Enter());
  if(approved){
   (void)BSP_Watchdog_SetPhase(now,WATCHDOG_RECOVERY,600000);
   facts.code=RestoreStock();facts.verified=0;
   (void)BSP_Watchdog_SetPhase(GateBoard_Millis(),WATCHDOG_WAIT,0);
   GateMenu_Init(&menu,GateBoard_Keys(),GateBoard_Millis());menu.page=2;
   GateGesture_Init(&gesture,GateBoard_Millis(),GateBoard_Enter());
  }
 }
}
void Gate_Main(void)
{GateRetained retained;memcpy(&retained,(const void*)&g_recovery_mailbox.request,sizeof(retained));
 uint32_t reason=GateRetained_Valid(&retained)?retained.reason:GATE_REASON_NORMAL;
 entry_reason=reason;reset_cause=RCC->CSR;
 GateFault prior;memcpy(&prior,(const void*)&g_recovery_mailbox.fault,sizeof(prior));fault_cause=GateFault_Valid(&prior)?prior.code:0;
 /* A fault while the gate is executing returns to waiting, not another erase. */
 Mailbox(GATE_REASON_WAIT,0,0);
 if(GateBoard_Init())for(;;){}
 if(!BSP_Watchdog_Init(GateBoard_Millis(),WATCHDOG_RECOVERY,600000))for(;;){}
 GateBoard_UID(uid);display_ready=!GateBoard_DisplayInit();
 if(GateBoard_NorInit())Wait(1);
 GateStore_Init(&store,GateBoard_RawRead,0,uid);
 while(store.state==GATE_STORE_AUDIT){GateStore_Process(&store);Tick(1,0,store.error);}
 if(store.state!=GATE_STORE_READY||JournalScan())Wait(2);
 LogBoot(reason);
 if(reason==GATE_REASON_STOCK){uint32_t result=RestoreStock();Wait(result);}
 if(reason==GATE_REASON_WAIT||GateBoard_Enter()||journal.state==GATE_J_WAIT||journal.state==GATE_J_STOCK)Wait(0);
 /* Temporary diagnostic is never confirmed or retried after a boot/reset.
  * A copy interrupted BEFORE its first boot still resumes below. No setting
  * reset or fake trial-success marker is written. Local stock rescue remains. */
 if(GateJournal_DiagnosticWait(&journal))Wait(96);
 /* A trial BOOT_PENDING reset is failure evidence even if SRAM was lost.
  * Copy interruption is different: its committed source can be resumed. */
 if((journal.flags&GATE_F_TRIAL)&&(journal.state==GATE_J_BOOT_PENDING||reason==GATE_REASON_FAULT||reason==GATE_REASON_TRIAL_TIMEOUT||reason==GATE_REASON_INIT_FAILED)){
  uint32_t why=reason==GATE_REASON_FAULT?GATE_FAILURE_FAULT:reason==GATE_REASON_TRIAL_TIMEOUT?GATE_FAILURE_APP_TIMEOUT:reason==GATE_REASON_INIT_FAILED?GATE_FAILURE_INIT:GATE_FAILURE_RESET;
  if(RollbackOrWait(why))Wait(70+why);
 }
 else if(reason==GATE_REASON_FAULT||reason==GATE_REASON_TRIAL_TIMEOUT||reason==GATE_REASON_INIT_FAILED)Wait(80+reason);
 if((journal.flags&GATE_F_ROLLED_BACK)&&journal.state==GATE_J_BOOT_PENDING)Wait(89);
 if(journal.state==GATE_J_COPYING||(journal.state==GATE_J_READY&&journal.candidate<2)||reason==GATE_REASON_INSTALL){
  uint32_t r=EngineRun(journal.candidate,1);
  if(r){
   if(RollbackOrWait(GATE_FAILURE_IMAGE))Wait(r);
   r=EngineRun(journal.candidate,1);if(r)Wait(r);
  }
 }
 if(journal.attempts>=GATE_BOOT_FAILURE_LIMIT)Wait(3);
 uint32_t r=EngineRun(journal.active,0);if(r)Wait(r);
 GateJournalRecord boot=journal;boot.state=GATE_J_BOOT_PENDING;boot.attempts++;boot.sequence++;
 if(boot.flags&GATE_F_TRIAL)boot.failed_version=image.version;
 if(JournalAppend(0,&boot))Wait(4);
 Mailbox(GATE_REASON_NORMAL,journal.sequence,journal.attempts);
 GateBootContext context={.sequence=journal.sequence,.flags=journal.flags,.transaction=journal.transaction,.generation=journal.active_generation};
 GateBootContext_Seal(&context);memcpy((void*)&g_recovery_mailbox.boot,&context,sizeof(context));
 Tick(12,100,0);GateBoard_LockNor();GateBoard_Jump(GATE_PRODUCT_BASE);
}
