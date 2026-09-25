#if NOODOE_BOOTSTRAP
#include "Bootstrap_Bond.h"
#include "Bluetooth_KeyCodec.h"
#include "Noodoe_Crc32.h"
#include <string.h>
static uint32_t U16(const uint8_t *p){return p[0]|((uint32_t)p[1]<<8);}
static uint32_t U32(const uint8_t *p){return U16(p)|(U16(p+2)<<16);}
static void P32(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=n>>(8*i);}
/* Inspect already proves FAT ownership. Recheck all committed records and the
 * TLV schema here; future versions must never be silently replaced. */
static uint32_t Latest(BootstrapStorage *s,uint32_t *sector,uint32_t *field)
{
 uint32_t found=0,generation=0;
 for(uint32_t i=0;i<32;i++){uint8_t *p=s->arena+i*4096;
  if(U32(p)!=0x314a4643||U32(p+4092)!=0x31544d43||Noodoe_Crc32(p,4088)!=U32(p+4088))continue;
  if(U32(p+4)!=1||U32(p+8)!=BS_CONFIG||memcmp(p+20,s->uid,12)||U32(p+16)>3968)return BS_FORMAT;
  if(!found||(int32_t)(U32(p+12)-generation)>0){found=1;generation=U32(p+12);*sector=i;}
 }
 if(!found)return BS_FORMAT;
 uint8_t *p=s->arena+*sector*4096+64;uint32_t n=U32(p-48),last=0;*field=0;
 if(!n)return 0;
 if(n<8||U32(p)!=0x31474643||U32(p+4)!=1)return BS_FORMAT;
 for(uint32_t o=8;o<n;){if(n-o<4)return BS_FORMAT;
  uint32_t id=U16(p+o),len=U16(p+o+2);
  if(id<=last||len>n-o-4)return BS_FORMAT;
  if(id==0x0202){Bluetooth_KeyStore keys;if(!Bluetooth_DecodeKeys(&keys,p+o+4,len))return BS_FORMAT;*field=o;}
  last=id;o+=4+len;
 }return 0;
}
uint32_t BootstrapBond_Restore(BootstrapStorage *s)
{
 uint32_t sector,field,e=Latest(s,&sector,&field);if(e||!field)return e;
 Bluetooth_KeyStore keys;
 if(!Bluetooth_DecodeKeys(&keys,s->arena+sector*4096+64+field+4,152))return BS_FORMAT;
 return Bluetooth_RestoreKeys(&keys)?BS_FORMAT:0;
}
uint32_t BootstrapBond_Begin(BootstrapBond *b,BootstrapStorage *s)
{
 if(b->phase)return BS_BUSY;
 uint32_t e=BootstrapStorage_Inspect(s,BS_CONFIG);if(e)return e;
 b->phase=1;b->error=0;return 0;
}
static uint32_t Finish(BootstrapBond *b,BootstrapStorage *s,uint32_t e)
{s->io.lock(s->io.context);s->write_lease=0;s->state=e?BS_FAILED:BS_SAVED;s->error=b->error=e;b->phase=0;return 1;}
static uint32_t Read(BootstrapStorage *s,uint32_t address)
{
 if(s->io.read_raw(s->io.context,address,s->work,4096))return BS_IO;
 for(unsigned i=0;i<4096;i+=2){uint8_t v=s->work[i];s->work[i]=s->work[i+1];s->work[i+1]=v;}return 0;
}
static uint32_t Program(BootstrapStorage *s,uint32_t address,const uint8_t *p,uint32_t n)
{
 uint8_t raw[256];for(unsigned i=0;i<n;i+=2){raw[i]=p[i+1];raw[i+1]=p[i];}
 return s->io.program(s->io.context,address,raw,n)?BS_IO:0;
}
uint32_t BootstrapBond_Process(BootstrapBond *b,BootstrapStorage *s)
{
 if(!b->phase)return 1;
 if(b->phase==1){
  BootstrapStorage_Process(s);
  if(s->state==BS_FAILED)return Finish(b,s,s->error);
  if(s->state!=BS_SAVED)return 0;
  uint32_t sector,field,e=Latest(s,&sector,&field);if(e)return Finish(b,s,e);
  /* Load older peers only into empty slots: a key negotiated in this live
   * Bootstrap session always wins over the old durable key for that peer. */
  if((e=BootstrapBond_Restore(s)))return Finish(b,s,e);
  Bluetooth_KeyStore keys;Bluetooth_ExportKeys(&keys);uint8_t encoded[152];Bluetooth_EncodeKeys(encoded,&keys);
  uint8_t *old=s->arena+sector*4096;uint32_t n=U32(old+16);
  if(field&&!memcmp(old+64+field+4,encoded,4)&&!memcmp(old+64+field+12,encoded+8,144)){
   Bluetooth_MarkKeysPersisted(keys.generation);return Finish(b,s,0);
  }
  uint8_t *p=s->arena+131072;memcpy(p,old,4096);
  if(!n){P32(p+64,0x31474643);P32(p+68,1);n=8;}
  uint32_t at=8,replace=0;
  while(at<n){uint32_t id=U16(p+64+at),len=U16(p+66+at);if(id>=0x0202){if(id==0x0202)replace=len+4;break;}at+=4+len;}
  if(n-replace+156>3968)return Finish(b,s,BS_SPACE);
  memmove(p+64+at+156,p+64+at+replace,n-at-replace);
  p[64+at]=2;p[65+at]=2;p[66+at]=152;p[67+at]=0;memcpy(p+68+at,encoded,152);
  P32(p+16,n-replace+156);P32(p+12,U32(old+12)+1);P32(p+4088,Noodoe_Crc32(p,4088));P32(p+4092,0xffffffff);
  /* Follow the audited FAT chain, including fragmented existing files. The
   * BSP lease covers this CFG cluster; this writer touches only its next
   * journal sector, preserving the latest committed record. */
  uint32_t cluster=0,target=(sector+1)%32;
  for(unsigned i=0;i<512;i++){uint8_t *d=s->metadata+0x5000+i*32;if(!d[0])break;
   if(d[0]!=0xe5&&d[11]!=15&&!memcmp(d,"CFWCFG  DAT",11)){cluster=U16(d+26);break;}}
  for(uint32_t i=0;i<target/8;i++){uint32_t off=cluster+cluster/2,v=U16(s->metadata+0x1000+off);cluster=(cluster&1)?v>>4:v&4095;}
  if(cluster<2||cluster>=4080)return Finish(b,s,BS_FORMAT);
  b->address=0x9000+(cluster-2)*32768+(target%8)*4096;
  if(b->address>RECOVERY_STORE_SAFE_END-4096)return Finish(b,s,BS_DENIED);
  if(s->io.grant(s->io.context,b->address-(target%8)*4096,32768,0))return Finish(b,s,BS_DENIED);
  b->generation=keys.generation;s->write_lease=1;s->state=BS_WRITING;b->phase=2;return 0;
 }
 uint8_t *p=s->arena+131072;uint32_t e=0;
 if(b->phase==2)e=s->io.erase(s->io.context,b->address)?BS_IO:0;
 else if(b->phase<=18)e=Program(s,b->address+(b->phase-3)*256,p+(b->phase-3)*256,256);
 else if(b->phase==19){e=Read(s,b->address);if(!e&&memcmp(s->work,p,4096))e=BS_HASH;}
 else if(b->phase==20){P32(p+4092,0x31544d43);e=Program(s,b->address+4092,p+4092,4);}
 else{e=Read(s,b->address);if(!e&&memcmp(s->work,p,4096))e=BS_HASH;
  if(!e){Bluetooth_KeyStore latest;Bluetooth_ExportKeys(&latest);
   if(latest.generation!=b->generation){Finish(b,s,0);e=BootstrapBond_Begin(b,s);return e?Finish(b,s,e):0;}
   Bluetooth_MarkKeysPersisted(b->generation);
  }return Finish(b,s,e);}
 if(e)return Finish(b,s,e);
 ++b->phase;return 0;
}
#endif
