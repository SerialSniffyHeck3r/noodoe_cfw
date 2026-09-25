#include "Bootstrap_Target.h"
#include "Update_Service.h"
#include <string.h>
static void Swap(uint8_t *p,uint32_t n)
{for(uint32_t i=0;i<n;i+=2){uint8_t t=p[i];p[i]=p[i+1];p[i+1]=t;}}
static uint32_t Word(const uint8_t *p)
{uint32_t v;memcpy(&v,p,4);return v;}
uint32_t BootstrapTarget_Check(uint32_t (*read)(uint32_t,void *,uint32_t),
 uint32_t (*now)(void),const uint8_t slot_sha[2][32],const uint8_t journal_sha[32],
 const uint8_t requirement[44])
{
 uint8_t buffer[1024],digest[32];UpdateSha256 hash;
 if(!read||!now||!slot_sha||!journal_sha||!requirement)return UPDATE_ARGUMENT;
 if(read(UPDATE_STAGE_BASE,buffer,8))return UPDATE_IO;
 Swap(buffer,8);uint32_t pc=Word(buffer+4);
 if(Word(buffer)!=0x2002ff00U||!(pc&1U)||pc<0x08010000U||pc>=0x08020000U)return UPDATE_VECTOR;
 UpdateSha256_Init(&hash);uint32_t start=now();
 for(uint32_t off=0;off<0x60000U;off+=sizeof(buffer)){
  if(now()-start>1500U||read(UPDATE_STAGE_BASE+0x10000U+off,buffer,sizeof(buffer)))return UPDATE_IO;
  Swap(buffer,sizeof(buffer));
  if(!off){pc=Word(buffer+4);
   if(Word(buffer)!=0x2002ff00U||!(pc&1U)||pc<0x08020000U||pc>=0x08080000U)return UPDATE_VECTOR;
   if(memcmp(buffer+0x200,requirement,44))return UPDATE_HASH;
  }
  UpdateSha256_Feed(&hash,buffer,sizeof(buffer));
 }
 UpdateSha256_Final(&hash,digest);
 return memcmp(digest,slot_sha[0],32)||memcmp(digest,slot_sha[1],32)||memcmp(digest,journal_sha,32)?UPDATE_HASH:0;
}
