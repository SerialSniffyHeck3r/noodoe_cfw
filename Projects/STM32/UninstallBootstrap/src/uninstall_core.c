#include "uninstall_core.h"
#include "Noodoe_Crc32.h"
#include "Resources.h"
#include "event_log.h"
#include "Update_Service.h"
#include <string.h>
#define MAGIC 0x31494E55U /* UNI1 */
#define COMMIT 0x31544D43U
#define PROGRESS 0xA000U
static const uint32_t sizes[GATE_FILE_COUNT]={0x80000,0x80000,0x10000,0x80000,0x100000,0x40000,0x20000,0x40000,0x100000,0x20000};
static uint32_t U(const uint8_t *p){return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void W(uint8_t *p,uint32_t v){for(uint32_t i=0;i<4;i++)p[i]=v>>(8*i);}
static uint32_t Fail(UninstallCore *s,uint32_t e){s->error=e;s->state=UNINSTALL_ERROR;return e;}
static void Tick(UninstallCore *s,uint32_t done,uint32_t total){if(s->io.progress)s->io.progress(s->io.context,s->state,done,total);}
static void Swap(void *p,uint32_t n){uint8_t *b=p;for(uint32_t i=0;i<n;i+=2){uint8_t v=b[i];b[i]=b[i+1];b[i+1]=v;}}
static void Hash(const void *p,uint32_t n,uint8_t out[32]){UpdateSha256 h;UpdateSha256_Init(&h);UpdateSha256_Feed(&h,p,n);UpdateSha256_Final(&h,out);}
static uint32_t Logical(UninstallCore *s,uint32_t a,void *p,uint32_t n)
{if((a|n)&1U||!n||s->io.read(s->io.context,a,p,n))return 1;Swap(p,n);Tick(s,0,0);return 0;}
/* The saved allocation graph remains the authority once deletion has begun.
 * Subdirectories are live reads: no CFW container can own their clusters. */
static uint32_t AuditRead(void *c,uint32_t a,void *p,uint32_t n)
{UninstallCore *s=c;if(a<sizeof(s->original)){
 if(n>sizeof(s->original)-a)return 1;memcpy(p,s->original+a,n);Swap(p,n);return 0;}
 uint32_t r=s->io.read(s->io.context,a,p,n);Tick(s,0,0);return r;}
static uint32_t FileRead(UninstallCore *s,uint32_t f,uint32_t off,void *p,uint32_t n)
{return GateStore_Read(&s->audit,f,off,p,n);}
/* Journal programming is append-only and idempotent even after a torn word.
 * Never erase the entry vectors or a partly committed recovery plan. */
static uint32_t JournalWrite(UninstallCore *s,uint32_t off,const uint8_t *p,uint32_t n)
{if((off|n)&3U||off>UNINSTALL_JOURNAL_BYTES||n>UNINSTALL_JOURNAL_BYTES-off)return 1;
 for(uint32_t at=0;at<n;at+=256){uint32_t k=n-at;if(k>256)k=256;
  if(s->io.journal_read(s->io.context,off+at,s->verify,k))return 1;
  for(uint32_t i=0;i<k;i++)if((s->verify[i]&p[at+i])!=p[at+i])return 1;
  if(memcmp(s->verify,p+at,k)&&s->io.journal_program(s->io.context,off+at,p+at,k))return 1;
  if(s->io.journal_read(s->io.context,off+at,s->verify,k)||memcmp(s->verify,p+at,k))return 1;
  Tick(s,at,n);
 }return 0;}
uint32_t Uninstall_Init(UninstallCore *s,const UninstallIO *io,const uint32_t uid[3],const uint8_t identity[32])
{if(!s||!io||!uid||!identity)return UNINSTALL_IDENTITY;
 memset(s,0,sizeof(*s));s->io=*io;memcpy(s->uid,uid,12);memcpy(s->identity,identity,32);
 if(!io->read||!io->erase||!io->program||!io->journal_read||!io->journal_program)return Fail(s,UNINSTALL_IO);
 if(io->journal_read(io->context,0,s->header,4096))return Fail(s,UNINSTALL_IO);
 if(U(s->header+4092)==COMMIT){
  if(U(s->header)!=MAGIC||U(s->header+4)!=1||memcmp(s->header+8,uid,12)||memcmp(s->header+20,identity,32)||
     U(s->header+4088)!=Noodoe_Crc32(s->header,4088))return Fail(s,UNINSTALL_JOURNAL);
  if(io->journal_read(io->context,4096,s->original,sizeof(s->original)))return Fail(s,UNINSTALL_IO);
  uint8_t h[32];Hash(s->original,sizeof(s->original),h);if(memcmp(h,s->header+52,32))return Fail(s,UNINSTALL_JOURNAL);
  s->resuming=s->approved=1;
 }else{
  for(uint32_t a=0;a<sizeof(s->original);a+=4096)if(Logical(s,a,s->original+a,4096))return Fail(s,UNINSTALL_IO);
 }
 return 0;
}
static uint32_t Record(UninstallCore *s,const uint8_t *p,uint32_t purpose)
{return U(p)==0x314a4643U&&U(p+4)==1&&U(p+8)==purpose&&U(p+16)<=3968&&
 !memcmp(p+20,s->uid,12)&&U(p+4092)==COMMIT&&U(p+4088)==Noodoe_Crc32(p,4088);}
/* Accept valid UID-bound records, including a surviving old journal record.
 * A filename, an erased file or a record belonging to another MCU is not proof. */
static uint32_t Owner(UninstallCore *s,uint32_t f)
{uint8_t *p=s->block;
 if(f<2)return !!(s->audit.identity&(1U<<f));
 if(f==GATE_FILE_LOG)return !FileRead(s,f,EVENT_LOG_IDENTITY,p,4096)&&EventLog_CheckIdentity(p,s->uid);
 if(f==GATE_FILE_STOCK)return !FileRead(s,f,0,p,4096)&&!RecoveryStore_CheckHeader(p,s->uid);
 if(f==GATE_FILE_TEXT)return !FileRead(s,f,0x1f000,p,4096)&&Record(s,p,4)&&U(p+16)==8&&U(p+64)==0x31545854U&&U(p+68)==1;
 if(f==GATE_FILE_PHOTO)return !FileRead(s,f,0,p,4096)&&Record(s,p,3)&&U(p+16)==8&&U(p+64)==0x31465043U&&U(p+68)==1;
 if(f==GATE_FILE_RESOURCES){
  for(uint32_t slot=0;slot<2;slot++){
   if(FileRead(s,f,slot*0x80000,p,4096))return 0;
   if(Resources_CheckHeader(p,p+16))continue;
   uint8_t expected[32],h[32];memcpy(expected,p+16,32);uint32_t total=U(p+8),count=U(p+12);
   UpdateSha256 sha;UpdateSha256_Init(&sha);UpdateSha256_Feed(&sha,p+48,count*16);
   for(uint32_t a=0;a<total;){uint32_t n=total-a;if(n>4096)n=4096;
    if(FileRead(s,f,slot*0x80000+4096+a,p,n))return 0;
    UpdateSha256_Feed(&sha,p,n);a+=n;Tick(s,a,total);}
   UpdateSha256_Final(&sha,h);if(!memcmp(h,expected,32))return 1;
  }return 0;
 }
 for(uint32_t off=0;off<sizes[f];off+=4096){
  if(FileRead(s,f,off,p,4096))return 0;
  if(f==GATE_FILE_BOOT){GateJournalRecord j;if(GateJournal_Decode(p,s->uid,&j))return 1;}
  else if(f==GATE_FILE_CONFIG||f==GATE_FILE_RIDE){if(Record(s,p,f==GATE_FILE_CONFIG?1:2))return 1;}
  /* Unknown file types never acquire a write/erase capability. */
  else return 0;
 }return 0;
}
uint32_t Uninstall_Audit(UninstallCore *s)
{if(!s||s->state!=UNINSTALL_NEW)return UNINSTALL_APPROVAL;
 GateStore_Init(&s->audit,AuditRead,s,s->uid);
 while(s->audit.state==GATE_STORE_AUDIT){GateStore_Process(&s->audit);Tick(s,0,0);}
 if(s->audit.state!=GATE_STORE_READY)return Fail(s,UNINSTALL_FAT);
 s->found=s->audit.found;
 /* Resources have no per-device UID. Require the independent UID-bound
  * image identities and recovery container as its provenance anchor. */
 if(!s->found||(s->found&(1U<<GATE_FILE_RESOURCES)&&((s->found&11U)!=11U)))return Fail(s,UNINSTALL_OWNER);
 for(uint32_t f=0;f<GATE_FILE_COUNT;f++)if(s->found&(1U<<f)){
  /* Factory containers have plain 8.3 entries. Do not orphan an unknown
   * long-filename chain associated with a same-name short entry. */
  uint32_t entry=s->audit.root_entry[f];
  if(entry&&s->original[0x5000+(entry-1)*32]!=0xe5&&s->original[0x5000+(entry-1)*32+11]==15)return Fail(s,UNINSTALL_OWNER);
  if(!s->resuming&&!Owner(s,f))return Fail(s,UNINSTALL_OWNER);
  s->bytes+=sizes[f];
 }
 uint8_t h[32];Hash(s->original,sizeof(s->original),h);
 if(s->resuming){
  if(U(s->header+84)!=s->found||U(s->header+88)!=s->bytes)return Fail(s,UNINSTALL_JOURNAL);
  for(uint32_t f=0;f<GATE_FILE_COUNT;f++)if(U(s->header+96+4*f)!=s->audit.root_entry[f])return Fail(s,UNINSTALL_JOURNAL);
  s->audit.identity=3;s->state=UNINSTALL_ERASING;
 }else{
  memset(s->header,255,4096);W(s->header,MAGIC);W(s->header+4,1);memcpy(s->header+8,s->uid,12);
  memcpy(s->header+20,s->identity,32);memcpy(s->header+52,h,32);W(s->header+84,s->found);W(s->header+88,s->bytes);
  for(uint32_t f=0;f<GATE_FILE_COUNT;f++)W(s->header+96+4*f,s->audit.root_entry[f]);
  W(s->header+4088,Noodoe_Crc32(s->header,4088));W(s->header+4092,COMMIT);s->state=UNINSTALL_READY;
 }return 0;
}
uint32_t Uninstall_Approve(UninstallCore *s,uint32_t token)
{if(!s||s->state!=UNINSTALL_READY||token!=UNINSTALL_CONFIRM)return UNINSTALL_APPROVAL;
 /* Fresh physical metadata must still equal the audited graph. */
 for(uint32_t a=0;a<sizeof(s->original);a+=4096)if(Logical(s,a,s->block,4096)||memcmp(s->block,s->original+a,4096))return Fail(s,UNINSTALL_CHANGED);
 if(JournalWrite(s,4096,s->original,sizeof(s->original))||JournalWrite(s,0,s->header,4092)||JournalWrite(s,4092,s->header+4092,4))return Fail(s,UNINSTALL_JOURNAL);
 s->approved=1;s->state=UNINSTALL_ERASING;return 0;
}
static uint32_t Erased(const uint8_t *p){for(uint32_t i=0;i<4096;i++)if(p[i]!=255)return 0;return 1;}
/* Only erase selected ownership entries; preserve directory bytes including
 * timestamps/attributes and every unrelated FAT12 shared nibble. */
static void FinalMetadata(UninstallCore *s,uint8_t *out,uint32_t sector)
{memcpy(out,s->original+sector*4096,4096);
 for(uint32_t f=0;f<GATE_FILE_COUNT;f++)if(s->found&(1U<<f)){
  if(sector>=5){uint32_t entry=s->audit.root_entry[f];if(entry/128==sector-5)out[(entry%128)*32]=0xe5;}
  else if(sector>=1&&sector<=4){uint32_t half=(sector-1)%2;
   for(uint32_t i=0;i<sizes[f]/32768;i++){
    uint32_t c=(s->audit.map[f][i]-0x9000)/32768+2,o=c+c/2;
    /* FAT12 words may cross a4KiB boundary. Apply each nibble separately. */
    for(uint32_t b=0;b<2;b++)if((o+b)/4096==half){uint32_t at=(o+b)%4096;uint8_t mask=(c&1)?(b?0:15):(b?240:0);out[at]&=mask;}
   }
  }
 }
}
uint32_t Uninstall_Process(UninstallCore *s)
{if(!s||!s->approved)return UNINSTALL_APPROVAL;
 if(s->state==UNINSTALL_ERASING){
  /* Walk by file/cluster, one physical4KiB at a time, including slack. */
  while(s->cluster<GATE_FILE_COUNT&&(!(s->found&(1U<<s->cluster))||s->sector==sizes[s->cluster]/4096)){s->cluster++;s->sector=0;}
  if(s->cluster==GATE_FILE_COUNT){s->state=UNINSTALL_METADATA;s->metadata_sector=1;return 0;}
  uint32_t off=s->sector*4096,a=s->audit.map[s->cluster][off/32768]+off%32768;
  if(s->io.read(s->io.context,a,s->block,4096))return Fail(s,UNINSTALL_IO);
  if(!Erased(s->block)&&(s->io.erase(s->io.context,a)||s->io.read(s->io.context,a,s->block,4096)||!Erased(s->block)))return Fail(s,UNINSTALL_IO);
  /* No wear on already erased sectors when a reset repeats the pass. */
  uint32_t completed=off+4096;
  for(uint32_t f=0;f<s->cluster;f++)if(s->found&(1U<<f))completed+=sizes[f];
  s->sector++;Tick(s,completed,s->bytes);return 0;
 }
 if(s->state==UNINSTALL_METADATA){
  uint32_t sector=s->metadata_sector,a=sector*4096;FinalMetadata(s,s->block,sector);
  if(Logical(s,a,s->verify,4096))return Fail(s,UNINSTALL_IO);
  if(memcmp(s->block,s->verify,4096)){
   /* A single journaled intent authorizes finishing this sector after a
    * torn erase/program, never an arbitrary current FAT replacement. */
   uint8_t marker[4]={0,0,0,0};
   uint32_t intent=PROGRESS+4*sector;
   if(s->io.journal_read(s->io.context,intent,s->verify,4))return Fail(s,UNINSTALL_IO);
   if(U(s->verify)!=0){
    if(Logical(s,a,s->verify,4096)||memcmp(s->verify,s->original+a,4096))return Fail(s,UNINSTALL_CHANGED);
    if(JournalWrite(s,intent,marker,4))return Fail(s,UNINSTALL_JOURNAL);
   }
   if(s->io.erase(s->io.context,a))return Fail(s,UNINSTALL_IO);
   Swap(s->block,4096);
   for(uint32_t off=0;off<4096;off+=256){if(s->io.program(s->io.context,a+off,s->block+off,256))return Fail(s,UNINSTALL_IO);Tick(s,(sector-1)*4096+off,8*4096);}
   if(s->io.read(s->io.context,a,s->verify,4096)||memcmp(s->verify,s->block,4096))return Fail(s,UNINSTALL_IO);
  }
  if(++s->metadata_sector==9){
   for(uint32_t i=0;i<9;i++){FinalMetadata(s,s->block,i);if(Logical(s,i*4096,s->verify,4096)||memcmp(s->block,s->verify,4096))return Fail(s,UNINSTALL_CHANGED);}
   s->state=UNINSTALL_CLEAN;
  }return 0;
 }
 return s->state==UNINSTALL_CLEAN?0:s->error;
}
