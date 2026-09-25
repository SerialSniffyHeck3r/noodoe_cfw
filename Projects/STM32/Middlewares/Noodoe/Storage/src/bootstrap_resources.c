#include "Bootstrap_Resources.h"
#include "Resources.h"
#include <string.h>
#define SLOT 524288U
static uint32_t U(const uint8_t *p){return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static uint32_t F(BootstrapStorage *s,uint32_t c){uint8_t *p=s->metadata+4096+c+c/2;uint32_t n=p[0]|((uint32_t)p[1]<<8);return c&1?n>>4:n&4095;}
static uint32_t Read(BootstrapStorage *s,uint32_t a,uint32_t n)
{if(s->io.read_raw(s->io.context,a,s->work,(n+1U)&~1U))return BS_IO;
 for(uint32_t i=0;i<n;i+=2){uint8_t x=s->work[i];s->work[i]=s->work[i+1];s->work[i+1]=x;}return 0;}
/* New table/body SHA is pinned by this Bootstrap build. No old slot is used
 * to manufacture a replacement requirement or weaken the Product check. */
uint32_t BootstrapResources_Content(BootstrapStorage *s)
{if(s->bytes!=SLOT||Resources_CheckHeader(s->arena,(const uint8_t[])RESOURCES_EXPECTED_SHA))return BS_FORMAT;
 UpdateSha256 h;uint8_t digest[32];UpdateSha256_Init(&h);UpdateSha256_Feed(&h,s->arena+48,U(s->arena+12)*16);
 UpdateSha256_Feed(&h,s->arena+4096,U(s->arena+8));UpdateSha256_Final(&h,digest);return memcmp(digest,s->arena+16,32)?BS_HASH:0;}
/* The whole old container was just physically read and matched to the phone's
 * durable preimage. Select a structurally valid slot to preserve; its body is
 * revalidated in bounded reads before any erase. No required-ID bypass. */
uint32_t BootstrapResources_Select(BootstrapStorage *s)
{for(uint32_t slot=0;slot<2;slot++){uint8_t *p=s->arena+slot*SLOT;
  if(Resources_CheckHeader(p,p+16))continue;
  s->resource_preserve_slot=slot;memcpy(s->resource_old_hash,p+16,32);
  UpdateSha256 h;UpdateSha256_Init(&h);UpdateSha256_Feed(&h,s->arena+(1U-slot)*SLOT,SLOT);
  UpdateSha256_Final(&h,s->resource_before_hash);return 0;
 }return BS_FORMAT;}
/* RecoveryStore already audited all ownership. Require exactly the existing
 * contiguous32-cluster file; its FAT/root bytes are never modified. */
uint32_t BootstrapResources_Prepare(BootstrapStorage *s)
{uint32_t first=0,found=0;
 for(uint32_t i=0;i<512;i++){uint8_t *p=s->metadata+0x5000+32*i;if(!p[0])break;
  if(p[0]!=0xe5&&p[11]!=15&&!memcmp(p,"NOODOE  RSC",11)){
   if(found++||(p[11]&0x18)||U(p+28)!=2*SLOT||p[20]||p[21])return BS_FORMAT;
   first=p[26]|((uint32_t)p[27]<<8);s->root_index=i;}}
 if(found!=1||first<2||first+32>4080)return BS_FORMAT;
 for(uint32_t i=0;i<32;i++){uint32_t n=F(s,first+i);if(i==31?n<0xff8:n!=first+i+1)return BS_FORMAT;}
 uint32_t base=0x9000+(first-2)*32768;if(base>RECOVERY_STORE_SAFE_END-2*SLOT)return BS_FORMAT;
 if(s->scoped&&base!=s->scope_first[BS_RESOURCE])return BS_HASH;
 s->first=base+(1U-s->resource_preserve_slot)*SLOT;UpdateSha256_Init(&s->hash);UpdateSha256_Feed(&s->hash,s->metadata,sizeof(s->metadata));UpdateSha256_Final(&s->hash,s->metadata_hash);
 s->phase=2;s->position=0;return 0;}
/* Before PREPARED, physically verify the preserved slot's complete body. The
 * target must match the scoped preimage; legacy SWD still requires erased B.
 * Each call performs only one bounded physical read. */
uint32_t BootstrapResources_Audit(BootstrapStorage *s)
{uint32_t preserved=s->resource_preserve_slot?s->first+SLOT:s->first-SLOT;
 if(s->phase==2){if(Read(s,preserved,4096))return BS_IO;
  if(Resources_CheckHeader(s->work,s->work+16))return BS_FORMAT;
  if(s->reseed&&memcmp(s->work+16,s->resource_old_hash,32))return BS_HASH;
  memcpy(s->resource_old_hash,s->work+16,32);s->resource_total=U(s->work+8);
  UpdateSha256_Init(&s->hash);UpdateSha256_Feed(&s->hash,s->work+48,U(s->work+12)*16);s->phase=3;s->position=0;return 0;}
 if(s->phase==3){uint32_t n=s->resource_total-s->position;if(n>4096)n=4096;
  if(Read(s,preserved+4096+s->position,n))return BS_IO;
  UpdateSha256_Feed(&s->hash,s->work,n);s->position+=n;
  if(s->position==s->resource_total){uint8_t h[32];UpdateSha256_Final(&s->hash,h);if(memcmp(h,s->resource_old_hash,32))return BS_HASH;s->phase=4;s->position=0;UpdateSha256_Init(&s->hash);}return 0;}
 if(s->phase==4){if(Read(s,s->first+s->position,4096))return BS_IO;
  if(s->reseed)UpdateSha256_Feed(&s->hash,s->work,4096);
  else for(uint32_t i=0;i<4096;i++)if(s->work[i]!=255)return BS_COLLISION;
  s->position+=4096;if(s->position==SLOT){
   if(s->reseed){uint8_t h[32];UpdateSha256_Final(&s->hash,h);if(memcmp(h,s->resource_before_hash,32))return BS_HASH;}
   s->state=BS_PREPARED;s->phase=s->position=0;}return 0;}
 return BS_ORDER;}
/* Preserve the selected old slot and FAT throughout. Write the other slot's
 * body, then its incomplete header. Only complete physical SHA readback
 * permits the final commit marker. No active-pointer or FAT write is needed. */
uint32_t BootstrapResources_Write(BootstrapStorage *s)
{if(s->phase==0){if(Read(s,s->position,4096))return BS_IO;
  UpdateSha256_Feed(&s->hash,s->work,4096);s->position+=4096;
  if(s->position==0x9000){uint8_t h[32];UpdateSha256_Final(&s->hash,h);if(memcmp(h,s->metadata_hash,32))return BS_HASH;
   if(s->io.grant(s->io.context,s->first,SLOT,0))return BS_DENIED;
   s->phase=1;s->position=4096;s->page=0;}return 0;}
 if(s->phase==1){uint32_t a=s->first+s->position;
  if(!s->page){if(s->io.erase(s->io.context,a))return BS_IO;s->page=1;return 0;}
  if(s->page<=16){uint8_t wire[256];uint32_t off=s->position+(s->page-1)*256;
   for(uint32_t i=0;i<256;i++)wire[i]=off+i>=4092&&off+i<4096?255:s->arena[off+(i^1U)];
   if(s->io.program(s->io.context,s->first+off,wire,256))return BS_IO;
   ++s->page;return 0;}
  if(Read(s,a,4096))return BS_IO;
  if(!s->position){if(U(s->work+4092)!=UINT32_MAX)return BS_HASH;memcpy(s->work+4092,s->arena+4092,4);}
  if(memcmp(s->work,s->arena+s->position,4096))return BS_HASH;
  s->page=0;
  if(!s->position){s->phase=2;UpdateSha256_Init(&s->hash);}
  else{s->position+=4096;if(s->position==SLOT)s->position=0;}return 0;}
 if(s->phase==2||s->phase==4){if(Read(s,s->first+s->position,4096))return BS_IO;
  if(s->phase==2&&!s->position){if(U(s->work+4092)!=UINT32_MAX)return BS_HASH;memcpy(s->work+4092,s->arena+4092,4);}
  UpdateSha256_Feed(&s->hash,s->work,4096);s->position+=4096;
  if(s->position==SLOT){uint8_t h[32];UpdateSha256_Final(&s->hash,h);if(memcmp(h,s->expected_hash,32))return BS_HASH;
   if(s->phase==2)s->phase=3;else{s->io.lock(s->io.context);s->verified_files|=1;s->state=BS_SAVED;}}return 0;}
 if(s->phase==3){uint8_t marker[4];for(uint32_t i=0;i<4;i++)marker[i]=s->arena[4092+(i^1U)];
  if(s->io.program(s->io.context,s->first+4092,marker,4))return BS_IO;
  s->phase=4;s->position=0;UpdateSha256_Init(&s->hash);return 0;}
 return BS_ORDER;}
