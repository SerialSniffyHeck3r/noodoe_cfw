#include "Noodoe_Crc32.h"
#include "Recovery_Core.h"
#include <string.h>
/* V5.16 APP-only image padded with FF to 448KiB. This is deliberately not
 * the 512KiB donor dump, and cannot select the BL slot or another revision. */
const uint8_t recovery_stock_sha256[32]={0x16,0x2f,0xae,0xec,0xc6,0x4b,0xf5,0x6d,0x82,0x85,0x70,0x16,0x87,0x85,0xe6,0x6d,0x1a,0x7d,0xa0,0x15,0x2e,0xf9,0x12,0xbf,0x83,0xf9,0xa1,0x90,0x94,0xd9,0x1e,0xdf};
uint32_t RecoveryStock_Matches(uint32_t version,const uint8_t sha[32])
{return sha&&version==RECOVERY_STOCK_VERSION&&!memcmp(sha,recovery_stock_sha256,32);}
uint32_t RecoveryResident_Verify(const uint8_t code[32768])
{
 static const uint8_t approved[32]={0xf8,0xb3,0x79,0xc3,0xfa,0xc0,0x78,0xa8,0xe0,0x1d,0x8b,0x6c,0x36,0xfc,0xcc,0x5b,0xb5,0xea,0x58,0x52,0xa0,0xdb,0x00,0x88,0x22,0xef,0x68,0x71,0xe0,0xf3,0x8d,0xf5};
 if(!code)return 0;
 uint8_t digest[32];RecoveryTarget_Hash(code,digest);
 return !memcmp(digest,approved,32);
}
static uint32_t Word(const uint8_t *p)
{uint32_t v;memcpy(&v,p,4);return v;}
static uint32_t Crc(uint32_t c,const uint8_t *p,uint32_t n)
{return Noodoe_Crc32Feed(c,p,n);}
static void Fail(RecoveryCore *c,uint32_t result)
{c->state=RECOVERY_ERROR;c->result=result;if(c->platform.disable)c->platform.disable(c->platform.context);}
static uint32_t EmptyMetadata(RecoveryCore *c)
{uint32_t m[5];if(c->platform.metadata_read(c->platform.context,m))return RECOVERY_IO;
 if(!RECOVERY_VERSION_SUPPORTED(m[0])||(c->resident_version&&m[0]!=c->resident_version))return RECOVERY_RESIDENT;
 c->resident_version=m[0];
 return m[4]?RECOVERY_PENDING:RECOVERY_OK;}
void RecoveryCore_Init(RecoveryCore *c,const RecoveryPlatform *p)
{if(c){memset(c,0,sizeof(*c));if(p)c->platform=*p;}}
/* Validate metadata before even reading source; a pending prior installation
 * must never have its staging payload replaced by a second transaction. */
uint32_t RecoveryCore_Request(RecoveryCore *c,const RecoverySource *s)
{
 if(!c||!s||!s->read||!c->platform.read_stage||!c->platform.enable||
    !c->platform.erase4k||!c->platform.program||!c->platform.disable||
    !c->platform.metadata_read||!c->platform.metadata_commit)return RECOVERY_ARGUMENT;
 if(c->state!=RECOVERY_IDLE||c->commit_attempted)return RECOVERY_STATE;
 uint32_t r=EmptyMetadata(c);if(r){Fail(c,r);return r;}
 c->source=*s;c->offset=0;c->crc=UINT32_MAX;c->state=RECOVERY_SOURCE_HASH;
 UpdateSha256_Init(&c->sha);return RECOVERY_OK;
}
/* The source is reread during upload and the entire stage is verified again.
 * Changing source bytes after the first pass cannot cause an unverified commit. */
void RecoveryCore_Process(RecoveryCore *c)
{
 if(!c)return;
 uint32_t r=0;
 switch(c->state){
 case RECOVERY_SOURCE_HASH:case RECOVERY_STAGE_HASH:{
  uint32_t source=c->state==RECOVERY_SOURCE_HASH;
  r=source?c->source.read(c->source.context,c->offset,c->buffer,4096):
    c->platform.read_stage(c->platform.context,c->offset,c->buffer,4096);
  if(r){Fail(c,RECOVERY_IO);break;}
  if(!c->offset){uint32_t msp=Word(c->buffer),pc=Word(c->buffer+4);
   if(msp<=0x20000000U||msp>0x20030000U||(msp&7U)||!(pc&1U)||
      (pc&~1U)<UPDATE_APP_BASE||(pc&~1U)>=UPDATE_APP_BASE+RECOVERY_STOCK_BYTES){Fail(c,RECOVERY_VECTOR);break;}}
  UpdateSha256_Feed(&c->sha,c->buffer,4096);c->crc=Crc(c->crc,c->buffer,4096);c->offset+=4096;
  if(c->offset==RECOVERY_STOCK_BYTES){
   UpdateSha256_Final(&c->sha,c->digest);c->crc^=UINT32_MAX;
   if(!RecoveryStock_Matches(RECOVERY_STOCK_VERSION,c->digest)||!c->crc||c->crc==UINT32_MAX){Fail(c,RECOVERY_HASH);break;}
   if(source){c->stock_crc=c->crc;r=EmptyMetadata(c);if(r){Fail(c,r);break;}
    if(c->platform.enable(c->platform.context)){Fail(c,RECOVERY_IO);break;}
    c->offset=0;c->state=RECOVERY_ERASE;
   }else if(c->crc!=c->stock_crc)Fail(c,RECOVERY_HASH);
   else{c->platform.disable(c->platform.context);c->state=RECOVERY_VERIFIED;}
  }break;}
 case RECOVERY_ERASE:
  r=EmptyMetadata(c);if(r){Fail(c,r);break;}
  c->destructive=1;if(c->platform.erase4k(c->platform.context,c->offset)){Fail(c,RECOVERY_IO);break;}
  c->state=RECOVERY_SOURCE_READ;break;
 case RECOVERY_SOURCE_READ:
  if(c->source.read(c->source.context,c->offset,c->buffer,4096)){Fail(c,RECOVERY_IO);break;}
  c->page=0;c->state=RECOVERY_PROGRAM;break;
 case RECOVERY_PROGRAM:
  r=EmptyMetadata(c);if(r){Fail(c,r);break;}
  if(c->platform.program(c->platform.context,c->offset+c->page,c->buffer+c->page,256)){Fail(c,RECOVERY_IO);break;}
  c->page+=256;if(c->page==4096){c->offset+=4096;c->state=RECOVERY_ERASE;
   if(c->offset==RECOVERY_STOCK_BYTES){c->state=RECOVERY_STAGE_HASH;c->offset=0;c->crc=UINT32_MAX;UpdateSha256_Init(&c->sha);}}
  break;
 default:break;
 }
}
/* Commit executes only after two complete trusted hashes. Any writer error is
 * ambiguous: no reset, no second erase, no pretending old metadata survived. */
uint32_t RecoveryCore_Commit(RecoveryCore *c,uint32_t token)
{
 if(!c||token!=RECOVERY_CONFIRM_TOKEN)return RECOVERY_ARGUMENT;
 if(c->state!=RECOVERY_VERIFIED||c->commit_attempted||!RecoveryStock_Matches(RECOVERY_STOCK_VERSION,c->digest))return RECOVERY_STATE;
 uint32_t r=EmptyMetadata(c);if(r){Fail(c,r);return r;}c->commit_attempted=1;
 if(c->platform.metadata_commit(c->platform.context,RECOVERY_STOCK_VERSION,c->stock_crc)){Fail(c,RECOVERY_AMBIGUOUS);return RECOVERY_AMBIGUOUS;}
 uint32_t m[5];if(c->platform.metadata_read(c->platform.context,m)||m[0]!=c->resident_version||m[1]!=RECOVERY_STOCK_VERSION||
   m[2]!=0x7F90U||m[3]!=RECOVERY_STOCK_BYTES||m[4]!=c->stock_crc){Fail(c,RECOVERY_AMBIGUOUS);return RECOVERY_AMBIGUOUS;}
 c->state=RECOVERY_COMMITTED;return RECOVERY_OK;
}
uint32_t RecoveryCore_Reset(RecoveryCore *c,uint32_t token)
{
 if(!c||token!=RECOVERY_CONFIRM_TOKEN||c->state!=RECOVERY_COMMITTED||!c->platform.reset)return RECOVERY_STATE;
 uint32_t m[5];if(c->platform.metadata_read(c->platform.context,m)||m[0]!=c->resident_version||m[1]!=RECOVERY_STOCK_VERSION||
   m[2]!=0x7F90U||m[3]!=RECOVERY_STOCK_BYTES||m[4]!=c->stock_crc){Fail(c,RECOVERY_AMBIGUOUS);return RECOVERY_AMBIGUOUS;}
 c->platform.reset(c->platform.context);return RECOVERY_IO;
}
void RecoveryCore_Cancel(RecoveryCore *c)
{if(c&&c->state!=RECOVERY_COMMITTED&&!c->commit_attempted)Fail(c,RECOVERY_CANCELLED);}

/* The exported range has no write operation and excludes APP/option bytes.
 * Bounds use subtraction, so wraparound cannot escape the lower64KiB. */
uint32_t RecoveryTarget_Export(const uint8_t lower[65536],uint32_t offset,uint32_t bytes,uint8_t *out)
{if(!lower||!out||!bytes||bytes>480||offset>=65536||bytes>65536-offset)return 0;
 memcpy(out,lower+offset,bytes);return 1;}
void RecoveryTarget_Hash(const uint8_t lower[32768],uint8_t sha[32])
{UpdateSha256 h;UpdateSha256_Init(&h);UpdateSha256_Feed(&h,lower,32768);UpdateSha256_Final(&h,sha);}
uint32_t RecoveryTarget_Verify(const uint8_t lower[65536])
{
 if(!lower)return 0;
 uint32_t version=Word(lower+0x8000),sp=Word(lower),pc=Word(lower+4);
 if(version==0x000E0000U)return RecoveryResident_Verify(lower);
 if(version!=0x000F0000U||(sp-0x20000001U)>=0x30000U||(sp&7)||!(pc&1)||
    (pc-0x08000001U)>=0x7fffU)return 0;
 /* Match the observed factory tuple, not arbitrary devices claiming0.15. */
 const uint8_t *f=lower+0xc000;
 static const uint8_t options[]={2,1,0,1,1};
 return !memcmp(f+34,"SR0701",6)&&!memcmp(f+52,"SAA1AA(KR)",10)&&
    f[128]==4&&f[129]==0&&!memcmp(f+132,options,sizeof(options));
}
