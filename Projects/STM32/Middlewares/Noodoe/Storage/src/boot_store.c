#if NOODOE_PRODUCT
#include "BootStore.h"
#include "Cfw_Files.h"
#include "BSP_RAM.h"
#include "stm32f4xx_hal.h"
#include <string.h>

typedef struct {
 uint8_t block[4096],verify[256];
 GateJournalRecord journal;
 GateImageInfo image[2];
 uint32_t uid[3],scan,found,latest,target,transaction,confirm_request,transfer_position;
} BootStore;
static BootStore *s;
static uint32_t confirm_requested;
static volatile uint32_t result_ack;
volatile BootStoreDiagnostics g_boot_store;
enum {BOOT_SCAN=1,BOOT_READY,BOOT_ERROR};
static uint32_t Fail(uint32_t e){g_boot_store.error=e;g_boot_store.state=BOOT_ERROR;return e;}
uint32_t BootStore_Ready(void){return s&&g_boot_store.state==BOOT_READY;}
uint32_t BootStore_GetBootInfo(GateJournalRecord *out)
{if(!out||!BootStore_Ready())return 0;uint32_t irq=__get_PRIMASK();__disable_irq();*out=s->journal;__set_PRIMASK(irq);return 1;}
uint32_t BootStore_MatchTrial(uint32_t sequence,const uint8_t sha[32])
{GateJournalRecord j;return sha&&BootStore_GetBootInfo(&j)&&j.state==GATE_J_BOOT_PENDING&&j.sequence==sequence&&!memcmp(j.active_sha,sha,32);}
/* A health request may precede the asynchronous journal scan. Keep the intent;
 * validate its boot/diagnostic context only once that context is available. */
void BootStore_RequestConfirm(void){confirm_requested=1;}
uint32_t BootStore_RequestResultAck(uint32_t transaction)
{GateJournalRecord j;if(!BootStore_GetBootInfo(&j)||!(j.flags&GATE_F_RESULT_PENDING)||!transaction||j.transaction!=transaction)return 0;
 __atomic_store_n(&result_ack,transaction,__ATOMIC_RELEASE);return 1;}
static uint32_t File(uint32_t slot){return slot?CFW_BOOT_B:CFW_BOOT_A;}

/* Full physical readback precedes the final4-byte commit. The previous record
 * is never erased, and the caller's RAM image is never used as readback. */
static uint32_t WriteRecord(uint32_t file,uint32_t off)
{
 if(CfwFiles_Erase(file,off))return CFW_IO;
 for(uint32_t at=0;at<4092;at+=256){uint32_t n=4092-at;if(n>256)n=256;
  if(CfwFiles_Program(file,off+at,s->block+at,n)||CfwFiles_Read(file,off+at,s->verify,n)||memcmp(s->verify,s->block+at,n))return CFW_IO;
 }
 if(CfwFiles_Read(file,off+4092,s->verify,4)||Cfw_Get32(s->verify)!=UINT32_MAX)return CFW_CORRUPT;
 if(CfwFiles_Program(file,off+4092,s->block+4092,4)||CfwFiles_Read(file,off+4092,s->verify,4)||memcmp(s->verify,s->block+4092,4))return CFW_IO;
 ++g_boot_store.writes;return CFW_OK;
}
static uint32_t Append(const GateJournalRecord *j)
{
 uint32_t next=(s->latest+1U)%GATE_JOURNAL_RECORDS;
 GateJournal_Encode(s->block,j);
 if(WriteRecord(CFW_BOOT_JOURNAL,next*4096))return Fail(CFW_IO);
 uint32_t irq=__get_PRIMASK();__disable_irq();s->journal=*j;__set_PRIMASK(irq);s->latest=next;g_boot_store.sequence=j->sequence;
 g_boot_store.active=j->active;g_boot_store.candidate=j->candidate;return 0;
}
/* A blank/corrupt/foreign container is never granted write access by name.
 * Initial provisioning creates both valid image identities and a journal. */
uint32_t BootStore_FinishSettingsReset(uint32_t epoch)
{if(!BootStore_Ready()||s->journal.reset_epoch!=epoch||!(s->journal.flags&GATE_F_RESET_PENDING))return CFW_ARGUMENT;
 GateJournalRecord j=s->journal;j.flags&=~GATE_F_RESET_PENDING;j.state=GATE_J_CONFIRMED;j.attempts=0;j.sequence++;
 uint32_t e=Append(&j);if(!e)g_boot_store.confirmed=1;return e;}
static uint32_t Prepare(void)
{
 GateJournalRecord *j=&s->journal;
 if(j->active>1||j->candidate!=GATE_NO_SLOT)return Fail(CFW_CORRUPT);
 for(uint32_t i=0;i<2;i++){
  if(CfwFiles_Read(File(i),GATE_IDENTITY_OFFSET,s->block,4096)||!GateIdentity_Decode(s->block,s->uid,i))return Fail(CFW_CORRUPT);
  if(i==j->active&&(CfwFiles_Read(File(i),0,s->block,4096)||!GateImage_Decode(s->block,s->uid,&s->image[i])))return Fail(CFW_CORRUPT);
 }
 if(j->active>1||memcmp(j->active_sha,s->image[j->active].sha256,32)||j->active_generation!=s->image[j->active].generation)return Fail(CFW_CORRUPT);
 if(CfwFiles_Grant(CFW_BOOT_JOURNAL))return Fail(CFW_CORRUPT);
 g_boot_store.state=BOOT_READY;g_boot_store.sequence=j->sequence;g_boot_store.active=j->active;g_boot_store.candidate=j->candidate;return 0;
}
void BootStore_Process(void)
{
 if(!s){if(CfwFiles_Status()!=CFW_OK)return;
  s=BSP_RAM_AllocateNamed(BSP_RAM_BOOT_STORE,sizeof(*s));if(!s){Fail(CFW_MEMORY);return;}
  memset(s,0,sizeof(*s));s->target=GATE_NO_SLOT;s->uid[0]=HAL_GetUIDw0();s->uid[1]=HAL_GetUIDw1();s->uid[2]=HAL_GetUIDw2();g_boot_store.state=BOOT_SCAN;
 }
 if(g_boot_store.state==BOOT_SCAN){GateJournalRecord j;
  if(CfwFiles_Read(CFW_BOOT_JOURNAL,s->scan*4096,s->block,4096)){Fail(CFW_IO);return;}
  if(Cfw_Get32(s->block)==GATE_JOURNAL_MAGIC&&Cfw_Get32(s->block+4092)==GATE_COMMIT&&
     Cfw_Get32(s->block+4088)==Cfw_Crc(s->block,4088)&&(Cfw_Get32(s->block+4)<1||Cfw_Get32(s->block+4)>2)){Fail(CFW_VERSION);return;}
  if(GateJournal_Decode(s->block,s->uid,&j)){
   if(s->found&&((j.sequence==s->journal.sequence&&memcmp(&j,&s->journal,sizeof(j)))||
      (uint32_t)(j.sequence-s->journal.sequence)==0x80000000U)){Fail(CFW_CORRUPT);return;}
   if(!s->found||GateSequence_Newer(j.sequence,s->journal.sequence)){s->journal=j;s->latest=s->scan;s->found=1;}
  }
  if(++s->scan==GATE_JOURNAL_RECORDS){if(!s->found)Fail(CFW_CORRUPT);else (void)Prepare();}return;
 }
 if(!BootStore_Ready()||s->transaction)return;
 /* Accept O during fallback health checks; consume only once confirmation
  * is durable. Clearing the request before then silently lost the press. */
 uint32_t ack=g_boot_store.confirmed?__atomic_exchange_n(&result_ack,0,__ATOMIC_ACQ_REL):0;
 if(ack&&g_boot_store.confirmed&&s->journal.transaction==ack&&(s->journal.flags&GATE_F_RESULT_PENDING)){
  GateJournalRecord j=s->journal;j.flags&=~GATE_F_RESULT_PENDING;j.sequence++;(void)Append(&j);return;
 }
 if(!confirm_requested)return;
 confirm_requested=0;
 if(s->journal.flags&GATE_F_DIAGNOSTIC)return;
 GateRetained r;memcpy(&r,(const void*)&g_recovery_mailbox.request,sizeof(r));
 if(!GateRetained_Valid(&r)||r.sequence!=s->journal.sequence||s->journal.state!=GATE_J_BOOT_PENDING){Fail(CFW_CORRUPT);return;}
 GateJournalRecord j=s->journal;j.state=GATE_J_CONFIRMED;j.attempts=0;j.sequence++;
 /* A successfully running fallback is now normal; retain its visible result
  * until explicit acknowledgement, but do not re-arm the one-shot fallback. */
 j.flags&=~GATE_F_ROLLED_BACK;
 if(j.flags&GATE_F_TRIAL){
  j.flags&=~GATE_F_TRIAL;
  if(j.transaction==GATE_RESTORE_RETAINED&&j.reset_epoch==GATE_RESTORE_RETAINED&&j.active_generation==1&&j.previous==GATE_NO_SLOT){j.transaction=0;j.reset_epoch=0;}
  else{j.flags|=GATE_F_RESET_PENDING;j.reset_epoch=j.active_generation;}
 }
 if(!Append(&j)){g_boot_store.confirmed=!(j.flags&GATE_F_RESET_PENDING);(void)GateRetained_Confirm(&r,GATE_CONFIRM_MS);memcpy((void*)&g_recovery_mailbox.request,&r,sizeof(r));__DSB();}
}
uint32_t BootStore_BeginUpdate(uint32_t transaction)
{
 if(!BootStore_Ready()||!g_boot_store.confirmed||!transaction||s->transaction||s->journal.candidate!=GATE_NO_SLOT)return CFW_BUSY;
 uint32_t target=1U-s->journal.active;
 if(CfwFiles_Grant(File(target)))return CFW_CORRUPT;
 /* Invalidate destination before any payload changes; previous active source
  * and journal stay committed throughout the incoming transfer. */
 if(CfwFiles_Erase(File(target),0))return CFW_IO;
 s->target=target;s->transaction=transaction;return 0;
}
/* Reserved tail of the INACTIVE 512KiB file. The immutable identity remains
 * at7F000; executable data ends61000. A manifest commits before checkpoints.
 * No FAT entry, active image, resource bank or boot journal is modified. */
#define TRANSFER_MANIFEST 0x61000U
#define TRANSFER_CHECKPOINTS 0x62000U
#define TRANSFER_MAGIC 0x31524654U
#define CHECKPOINT_MAGIC 0x31504354U
uint32_t BootStore_Resume(uint32_t version,uint32_t crc,const uint8_t sha[32],uint32_t *bytes)
{
 if(!s||!s->transaction||s->target>1||!sha||!bytes)return CFW_ARGUMENT;
 uint32_t file=File(s->target);uint8_t expected[80];memset(expected,255,sizeof(expected));
 Cfw_Put32(expected,TRANSFER_MAGIC);Cfw_Put32(expected+4,1);memcpy(expected+8,s->uid,12);
 Cfw_Put32(expected+20,s->transaction);Cfw_Put32(expected+24,version);Cfw_Put32(expected+28,crc);
 memcpy(expected+32,sha,32);Cfw_Put32(expected+64,s->journal.active_generation);Cfw_Put32(expected+68,s->target);
 if(CfwFiles_Read(file,TRANSFER_MANIFEST,s->block,4096))return CFW_IO;
 uint32_t valid=Cfw_Get32(s->block)==TRANSFER_MAGIC&&Cfw_Get32(s->block+4092)==GATE_COMMIT&&Cfw_Crc(s->block,4088)==Cfw_Get32(s->block+4088);
 if(valid&&Cfw_Get32(s->block+4)!=1)return CFW_VERSION;
 if(!valid||memcmp(s->block,expected,sizeof(expected))){
  /* Invalidate the old manifest BEFORE erasing its progress. A cut at any
   * step yields no resumable claim rather than old progress/new payload. */
  if(CfwFiles_Erase(file,TRANSFER_MANIFEST)||CfwFiles_Erase(file,TRANSFER_CHECKPOINTS))return CFW_IO;
  memset(s->block,255,4096);memcpy(s->block,expected,sizeof(expected));
  Cfw_Put32(s->block+4088,Cfw_Crc(s->block,4088));Cfw_Put32(s->block+4092,GATE_COMMIT);
  if(WriteRecord(file,TRANSFER_MANIFEST))return CFW_IO;
 }
 s->transfer_position=0;
 for(uint32_t sector=0;sector<GATE_PRODUCT_BYTES/4096;sector++){
  uint8_t record[32];if(CfwFiles_Read(file,TRANSFER_CHECKPOINTS+sector*32,record,32))return CFW_IO;
  if(Cfw_Get32(record)!=CHECKPOINT_MAGIC||Cfw_Get32(record+4)!=sector||Cfw_Get32(record+16)!=s->transaction||
     Cfw_Get32(record+20)!=~sector||Cfw_Get32(record+28)!=GATE_COMMIT||Cfw_Get32(record+24)!=Cfw_Crc(record,24)||
     Cfw_Get32(record+12)!=~Cfw_Get32(record+8))break;
  if(CfwFiles_Read(file,4096+sector*4096,s->block,4096))return CFW_IO;
  if(Cfw_Crc(s->block,4096)!=Cfw_Get32(record+8))return CFW_CORRUPT;
  s->transfer_position+=4096;
 }
 *bytes=s->transfer_position;return 0;
}
/* Every completed erase-sector is physically reread and journaled before
 * DATA success. A torn final record is safely reprogrammed with identical
 * bytes on retry; final whole-image SHA remains mandatory before COMMIT. */
uint32_t BootStore_Checkpoint(uint32_t bytes)
{
 if(!s||!s->transaction||s->target>1||bytes>GATE_PRODUCT_BYTES)return CFW_ARGUMENT;
 uint32_t end=bytes&~4095U,file=File(s->target);
 while(s->transfer_position<end){uint32_t sector=s->transfer_position/4096;uint8_t record[32],check[32];
  if(CfwFiles_Read(file,4096+s->transfer_position,s->block,4096))return CFW_IO;
  uint32_t crc=Cfw_Crc(s->block,4096),off=TRANSFER_CHECKPOINTS+sector*32;
  Cfw_Put32(record,CHECKPOINT_MAGIC);Cfw_Put32(record+4,sector);Cfw_Put32(record+8,crc);Cfw_Put32(record+12,~crc);
  Cfw_Put32(record+16,s->transaction);Cfw_Put32(record+20,~sector);Cfw_Put32(record+24,Cfw_Crc(record,24));Cfw_Put32(record+28,GATE_COMMIT);
  if(CfwFiles_Program(file,off,record,28)||CfwFiles_Read(file,off,check,28)||memcmp(record,check,28)||
     CfwFiles_Program(file,off+28,record+28,4)||CfwFiles_Read(file,off,check,32)||memcmp(record,check,32))return CFW_IO;
  s->transfer_position+=4096;
 }
 return 0;
}
static uint32_t Range(uint32_t off,uint32_t n)
{return BootStore_Ready()&&s->target<2&&n&&off<GATE_PRODUCT_BYTES&&n<=GATE_PRODUCT_BYTES-off;}
uint32_t BootStore_Read(uint32_t off,void *data,uint32_t n)
{return Range(off,n)?CfwFiles_Read(File(s->target),4096+off,data,n):CFW_ARGUMENT;}
uint32_t BootStore_Erase(uint32_t off)
{return s&&s->transaction&&Range(off,4096)&&!(off&4095)?CfwFiles_Erase(File(s->target),4096+off):CFW_ARGUMENT;}
uint32_t BootStore_Program(uint32_t off,const void *data,uint32_t n)
{return s&&s->transaction&&Range(off,n)?CfwFiles_Program(File(s->target),4096+off,data,n):CFW_ARGUMENT;}
/* Called only after UpdateService's full physical384KiB SHA/CRC pass and the
 * resource compatibility proof. Gate independently validates them again. */
static uint32_t CommitImage(uint32_t version,const uint8_t sha[32],const uint8_t requirement[44],uint32_t kind)
{
 if(!BootStore_Ready()||!s->transaction||s->target>1||!sha||!requirement||s->journal.active_generation==UINT32_MAX)return CFW_ARGUMENT;
 GateImageInfo info={0};info.kind=kind;memcpy(info.uid,s->uid,12);info.generation=s->journal.active_generation+1;info.version=version;memcpy(info.sha256,sha,32);memcpy(info.requirement,requirement,44);
 GateImage_Encode(s->block,&info);if(WriteRecord(File(s->target),0))return Fail(CFW_IO);
 GateJournalRecord j=s->journal;j.sequence++;j.state=GATE_J_READY;j.candidate=s->target;j.candidate_generation=info.generation;memcpy(j.candidate_sha,sha,32);
 j.flags=kind==GATE_IMAGE_DIAGNOSTIC?GATE_F_DIAGNOSTIC:GATE_F_TRIAL;j.previous=j.active;j.previous_generation=j.active_generation;memcpy(j.previous_sha,j.active_sha,32);
 j.failed_version=version;j.restored_version=s->image[j.active].version;j.failure_reason=0;j.transaction=s->transaction;
 if(Append(&j))return CFW_IO;
 s->image[s->target]=info;return 0;
}
uint32_t BootStore_Commit(uint32_t version,const uint8_t sha[32],const uint8_t requirement[44])
{return CommitImage(version,sha,requirement,GATE_IMAGE_PRODUCT);}
uint32_t BootStore_CommitDiagnostic(uint32_t version,const uint8_t sha[32],const uint8_t requirement[44])
{return CommitImage(version,sha,requirement,GATE_IMAGE_DIAGNOSTIC);}
uint32_t BootStore_Committed(const uint8_t sha[32])
{return BootStore_Ready()&&s->journal.state==GATE_J_READY&&s->journal.candidate<2&&sha&&!memcmp(sha,s->journal.candidate_sha,32);}
/* Verified staging retains its transaction for explicit COMMIT. Cancellation
 * prevents new payload writes; a subsequent BEGIN must choose a fresh slot. */
void BootStore_EndUpdate(void){if(s)s->transaction=0;}
#endif
