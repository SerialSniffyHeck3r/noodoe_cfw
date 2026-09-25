#include "gate_abi.h"
#include <string.h>
#include <stddef.h>
/* This codec is built for the STM32 little-endian target. Only packed
 * uint32_t UID arrays use bulk copies; records never use a structure dump. */
#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "Gate UID encoding requires a little-endian target"
#endif
static uint32_t U(const uint8_t *p){return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void W(uint8_t *p,uint32_t v){for(uint32_t i=0;i<4;i++)p[i]=v>>(8*i);}
static uint32_t Valid(const uint8_t *p,uint32_t magic,uint32_t reserved,uint32_t version)
{if(!p||U(p)!=magic||U(p+4)!=version||U(p+4092)!=GATE_COMMIT||U(p+4088)!=GatePolicy_Crc(p,4088))return 0;
 for(uint32_t i=reserved;i<4088;i++){if(p[i]!=255)return 0;}return 1;}
static void Finish(uint8_t *p){W(p+4088,GatePolicy_Crc(p,4088));W(p+4092,GATE_COMMIT);}
uint32_t GateImage_Decode(const uint8_t p[4096],const uint32_t uid[3],GateImageInfo *v)
{if(!uid||!v||!Valid(p,GATE_IMAGE_MAGIC,128,1)||U(p+8)!=4096||U(p+12)!=GATE_IMAGE_CONTAINER_BYTES||
 U(p+28)!=GATE_PRODUCT_BASE||U(p+32)!=GATE_PRODUCT_BYTES||!U(p+36)||U(p+116)!=GATE_ABI_VERSION||U(p+124)>GATE_IMAGE_DIAGNOSTIC)return 0;
 if(memcmp(p+16,uid,12))return 0;
 /* Diagnostic explicitly has no external font/asset dependency. Old Gates
  * reject its nonzero kind rather than treating it as an ordinary Product. */
 uint32_t kind=U(p+124);
 if(U(p+72)!=0x51534352U||U(p+76)!=1||U(p+80)!=(1U-kind))return 0;
 if(kind==GATE_IMAGE_DIAGNOSTIC){uint32_t any=0;
  for(uint32_t i=84;i<116;i+=4)any|=U(p+i);
  if(any)return 0;
 }
 /* Every output member is assigned below; no padding is serialized. */
 v->kind=kind;
 memcpy(v->uid,uid,12);v->generation=U(p+36);v->version=U(p+120);memcpy(v->sha256,p+40,32);memcpy(v->requirement,p+72,44);return 1;}
void GateImage_Encode(uint8_t p[4096],const GateImageInfo *v)
{memset(p,255,4096);W(p,GATE_IMAGE_MAGIC);W(p+4,1);W(p+8,4096);W(p+12,GATE_IMAGE_CONTAINER_BYTES);
 memcpy(p+16,v->uid,12);W(p+28,GATE_PRODUCT_BASE);W(p+32,GATE_PRODUCT_BYTES);W(p+36,v->generation);
 memcpy(p+40,v->sha256,32);memcpy(p+72,v->requirement,44);W(p+116,GATE_ABI_VERSION);W(p+120,v->version);W(p+124,v->kind);Finish(p);}
/* Explicit wire/native offset pairs replace repeated scalar marshaling.
 * This is not a structure dump: wire offsets and LE conversion remain fixed
 * if the compiler changes native padding or the in-memory record evolves. */
#define FIELD(wire,member) {wire,offsetof(GateJournalRecord,member)}
static const uint8_t journal_fields[][2]={
 FIELD(8,sequence),FIELD(12,state),FIELD(16,active),FIELD(20,candidate),FIELD(24,attempts),
 FIELD(108,active_generation),FIELD(112,candidate_generation),
 FIELD(120,flags),FIELD(124,previous),FIELD(128,previous_generation),
 FIELD(164,failed_version),FIELD(168,restored_version),FIELD(172,failure_reason),FIELD(176,transaction),FIELD(180,reset_epoch)};
#undef FIELD
_Static_assert(sizeof(GateJournalRecord)<256,"journal native offset bound");
static void JournalScalars(uint8_t *wire,void *native,uint32_t version,uint32_t encode)
{for(uint32_t i=0;i<sizeof(journal_fields)/sizeof(journal_fields[0]);i++){
 uint32_t offset=journal_fields[i][0],value;uint8_t *field=(uint8_t*)native+journal_fields[i][1];
 if(version==1&&offset>=120)continue;
 if(encode){memcpy(&value,field,4);W(wire+offset,value);}else{value=U(wire+offset);memcpy(field,&value,4);}
}}
uint32_t GateJournal_Decode(const uint8_t p[4096],const uint32_t uid[3],GateJournalRecord *v)
{if(!p||!uid||!v)return 0;
 uint32_t version=U(p+4);
 if(version<1||version>2||!Valid(p,GATE_JOURNAL_MAGIC,version==1?120U:184U,version)||
    U(p+12)<GATE_J_READY||U(p+12)>GATE_J_STOCK||U(p+28)||U(p+116)!=1)return 0;
 memset(v,0,sizeof(*v));v->previous=GATE_NO_SLOT;
 uint32_t active=U(p+16),candidate=U(p+20);if((active>1&&active!=GATE_NO_SLOT)||(candidate>1&&candidate!=GATE_NO_SLOT)||U(p+24)>3)return 0;
 if(U(p+12)==GATE_J_COPYING&&candidate>1)return 0;
 if((U(p+12)==GATE_J_READY||U(p+12)==GATE_J_BOOT_PENDING||U(p+12)==GATE_J_CONFIRMED)&&active>1)return 0;
 if(memcmp(p+96,uid,12))return 0;
 memcpy(v->uid,uid,12);
 JournalScalars((uint8_t*)p,v,version,0);
 memcpy(v->active_sha,p+32,32);memcpy(v->candidate_sha,p+64,32);
 if(version==2){memcpy(v->previous_sha,p+132,32);
  if((v->flags&~31U)||(v->previous>1&&v->previous!=GATE_NO_SLOT)||v->failure_reason>GATE_FAILURE_IMAGE)return 0;
  if((v->flags&(GATE_F_TRIAL|GATE_F_ROLLED_BACK))==(GATE_F_TRIAL|GATE_F_ROLLED_BACK))return 0;
  if((v->flags&GATE_F_DIAGNOSTIC)&&((v->flags&(GATE_F_TRIAL|GATE_F_ROLLED_BACK|GATE_F_RESET_PENDING))||v->state==GATE_J_CONFIRMED))return 0;
 }
 return 1;}
void GateJournal_Encode(uint8_t p[4096],const GateJournalRecord *v)
{memset(p,255,4096);W(p,GATE_JOURNAL_MAGIC);W(p+4,2);W(p+28,0);W(p+116,1);
 JournalScalars(p,(void*)v,2,1);memcpy(p+96,v->uid,12);memcpy(p+32,v->active_sha,32);memcpy(p+64,v->candidate_sha,32);memcpy(p+132,v->previous_sha,32);Finish(p);}

uint32_t GateJournal_DiagnosticWait(const GateJournalRecord *j)
{return j&&(j->flags&GATE_F_DIAGNOSTIC)&&j->state==GATE_J_BOOT_PENDING;}
uint32_t GateJournal_RequestRollback(GateJournalRecord *j,uint32_t failure)
{if(!j||!(j->flags&GATE_F_TRIAL)||(j->flags&GATE_F_ROLLED_BACK)||j->previous>1||!j->previous_generation||!failure||failure>GATE_FAILURE_IMAGE)return 0;
 j->candidate=j->previous;j->candidate_generation=j->previous_generation;memcpy(j->candidate_sha,j->previous_sha,32);
 j->flags=GATE_F_ROLLED_BACK|GATE_F_RESULT_PENDING;j->failure_reason=failure;j->state=GATE_J_READY;j->attempts=0;j->sequence++;return 1;}
uint32_t GateSequence_Newer(uint32_t a,uint32_t b){return (int32_t)(a-b)>0;}
uint32_t GateIdentity_Decode(const uint8_t p[4096],const uint32_t uid[3],uint32_t slot)
{if(!uid||slot>1||!Valid(p,GATE_IDENTITY_MAGIC,32,1)||U(p+8)!=slot||U(p+12)!=GATE_IMAGE_CONTAINER_BYTES||U(p+28)!=GATE_ABI_VERSION)return 0;
 return !memcmp(p+16,uid,12);}
void GateIdentity_Encode(uint8_t p[4096],const uint32_t uid[3],uint32_t slot)
{memset(p,255,4096);W(p,GATE_IDENTITY_MAGIC);W(p+4,1);W(p+8,slot);W(p+12,GATE_IMAGE_CONTAINER_BYTES);
 memcpy(p+16,uid,12);W(p+28,GATE_ABI_VERSION);Finish(p);}
