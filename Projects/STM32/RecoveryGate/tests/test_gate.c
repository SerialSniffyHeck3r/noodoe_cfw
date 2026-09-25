#include "gate_policy.h"
#include "gate_engine.h"
#include <stddef.h>
volatile uint32_t assertions;
void *memcpy(void *d,const void *s,size_t n){uint8_t *a=d;const uint8_t *b=s;while(n--)*a++=*b++;return d;}
void *memset(void *d,int c,size_t n){uint8_t *a=d;while(n--)*a++=c;return d;}
int memcmp(const void *a,const void *b,size_t n){const uint8_t *p=a,*q=b;while(n--){if(*p!=*q)return *p-*q;p++;q++;}return 0;}
#define T(v) do{assertions++;if(!(v))return __LINE__;}while(0)
static uint8_t header[4096];
static const uint32_t uid[3]={1,2,3};
uint32_t TestPolicy(void)
{GateRetained retained;GateRetained_Init(&retained);T(GateRetained_Valid(&retained));
 /* Typed diagnostic must survive the SRAM handoff as well as the NOR journal.
  * Reject unknown bits and a torn handoff; retain every existing Product flag. */
 GateBootContext boot={.sequence=7,.transaction=9,.generation=11};
 for(uint32_t flags=0;flags<=GATE_F_DIAGNOSTIC;flags++){
  boot.flags=flags;GateBootContext_Seal(&boot);T(GateBootContext_Valid(&boot));
 }
 boot.flags=32;GateBootContext_Seal(&boot);T(!GateBootContext_Valid(&boot));
 boot.flags=GATE_F_DIAGNOSTIC;GateBootContext_Seal(&boot);
 boot.sequence++;T(!GateBootContext_Valid(&boot));boot.sequence--;
 boot.inverse^=1;T(!GateBootContext_Valid(&boot));
 for(uint32_t i=0;i<3;i++)T(!GateRetained_BeforeBoot(&retained));T(GateRetained_BeforeBoot(&retained));T(retained.reason==GATE_REASON_WAIT);
 GateRetained_Init(&retained);T(!GateRetained_BeforeBoot(&retained));T(!GateRetained_Confirm(&retained,29999));T(GateRetained_Confirm(&retained,30000));T(!GateRetained_BeforeBoot(&retained));T(retained.attempts==1);
 retained.sequence++;T(!GateRetained_Valid(&retained));
 GateGesture gesture;GateGesture_Init(&gesture,0,1);
 for(uint32_t t=0;t<10000;t+=10)T(!GateGesture_Process(&gesture,t,1,1));
 for(uint32_t t=10000;t<11000;t+=10)T(!GateGesture_Process(&gesture,t,0,1));
 T(!GateGesture_Process(&gesture,11000,1,1));T(!GateGesture_Process(&gesture,12999,1,1));T(GateGesture_Process(&gesture,13000,1,1));T(!GateGesture_Process(&gesture,13010,1,1));
 /* A single raw release cancels even before its debounce interval elapses. */
 GateGesture_Init(&gesture,0,1);T(!GateGesture_Process(&gesture,100,0,1));T(!GateGesture_Process(&gesture,600,0,1));T(!GateGesture_Process(&gesture,601,1,1));
 T(!GateGesture_Process(&gesture,1000,1,0));T(!GateGesture_Process(&gesture,1010,1,1));T(!GateGesture_Process(&gesture,3000,1,1));
 GateGesture_Init(&gesture,0,1);T(!GateGesture_Process(&gesture,100,0,1));T(!GateGesture_Process(&gesture,600,0,1));
 T(!GateGesture_Process(&gesture,30599,0,1));T(!gesture.expired);T(!GateGesture_Process(&gesture,30600,0,1));T(gesture.expired);
 T(!GateGesture_Process(&gesture,31000,1,1));T(!GateGesture_Process(&gesture,34000,1,1));T(!GateGesture_Process(&gesture,35000,0,1));T(!GateGesture_Process(&gesture,38000,1,1));
 T(!GateGesture_Process(&gesture,39000,0,0));T(!GateGesture_Process(&gesture,39100,0,0));T(!GateGesture_Process(&gesture,39200,0,1));T(!GateGesture_Process(&gesture,39300,0,1));T(!GateGesture_Process(&gesture,39800,0,1));T(!GateGesture_Process(&gesture,40000,1,1));T(GateGesture_Process(&gesture,42000,1,1));
 /* Unsigned elapsed comparisons remain valid across tick wrap. */
 GateGesture_Init(&gesture,0xfffff000U,1);T(!GateGesture_Process(&gesture,0xfffff100U,0,1));T(!GateGesture_Process(&gesture,0xfffff400U,0,1));T(!GateGesture_Process(&gesture,0xfffff500U,1,1));T(GateGesture_Process(&gesture,0x00000300U,1,1));
 GateImageInfo image={{1,2,3},1,1,{0},{0x52,0x43,0x53,0x51,1,0,0,0,1}},copy;
 GateImage_Encode(header,&image);T(GateImage_Decode(header,uid,&copy));T(!memcmp(&copy,&image,sizeof(image)));
 header[4092]=255;T(!GateImage_Decode(header,uid,&copy));GateImage_Encode(header,&image);header[100]^=1;T(!GateImage_Decode(header,uid,&copy));
 GateIdentity_Encode(header,uid,0);T(GateIdentity_Decode(header,uid,0));T(!GateIdentity_Decode(header,uid,1));uint32_t foreign[3]={1,2,4};T(!GateIdentity_Decode(header,foreign,0));
 GateJournalRecord r={.sequence=0xffffffff,.state=GATE_J_CONFIRMED,.active=0,.candidate=GATE_NO_SLOT,.uid={1,2,3}},read;
 GateJournal_Encode(header,&r);T(GateJournal_Decode(header,uid,&read));T(!memcmp(&r,&read,sizeof(r)));T(GateSequence_Newer(0,~0U));T(!GateSequence_Newer(~0U,0));header[184]=0;T(!GateJournal_Decode(header,uid,&read));return 0;}
static GateEngine engine;static GateJournalRecord durable;static GateImageInfo image;
static uint32_t diagnostic;
static uint32_t fail_at,operation,writes,erases,cut_after,mutate_source;
#define SOURCE ((uint8_t*)0x11000000U)
#define FLASH_IMAGE ((uint8_t*)0x12000000U)
static uint32_t Source(void *c,uint32_t slot,uint32_t off,void *out,uint32_t n)
{(void)c;if(slot!=1||off>=GATE_PRODUCT_BYTES||n>GATE_PRODUCT_BYTES-off)return 1;memcpy(out,SOURCE+off,n);if(mutate_source&&engine.state==GATE_ENGINE_COPY&&off==4096)((uint8_t*)out)[0]^=1;return 0;}
static uint32_t Read(void *c,uint32_t off,void *out,uint32_t n){(void)c;memcpy(out,FLASH_IMAGE+off,n);return 0;}
static uint32_t Journal(void *c,const GateJournalRecord *r)
{(void)c;uint32_t cut=++operation==fail_at;if(cut&&!cut_after)return 1;durable=*r;return cut;}
static uint32_t Erase(void *c,uint32_t sector)
{(void)c;if(sector<5||sector>7)return 1;erases++;uint32_t cut=++operation==fail_at;
 memset(FLASH_IMAGE+(sector-5)*0x20000,255,cut?0x10000:0x20000);return cut;}
static uint32_t Program(void *c,uint32_t address,const void *data,uint32_t n)
{(void)c;if(address<GATE_PRODUCT_BASE||address>=GATE_PRODUCT_BASE+GATE_PRODUCT_BYTES||n>GATE_PRODUCT_BASE+GATE_PRODUCT_BYTES-address)return 1;
 writes++;uint32_t cut=++operation==fail_at;uint8_t *p=FLASH_IMAGE+address-GATE_PRODUCT_BASE;const uint8_t *v=data;
 for(uint32_t i=0;i<(cut?n/2:n);i++)p[i]&=v[i];return cut;}
static GateEngineIO io={0,Source,Read,Erase,Program,Journal};
static void Run(void){for(uint32_t i=0;i<1000&&engine.state!=GATE_ENGINE_READY&&engine.state!=GATE_ENGINE_FAILED;i++)GateEngine_Process(&engine);}
/* mode0 baseline;1..101 cuts at the COPYING commit, each sector erase,
 * each4KiB program, or READY commit;102 source corruption;103 changing source. */
uint32_t TestEngine(uint32_t mode)
{memset(&durable,0,sizeof(durable));durable.state=GATE_J_READY;durable.active=0;durable.candidate=1;durable.uid[0]=1;durable.uid[1]=2;durable.uid[2]=3;durable.candidate_generation=2;durable.flags=diagnostic?GATE_F_DIAGNOSTIC:0;
 memset(&image,0,sizeof(image));memcpy(image.uid,uid,12);image.generation=2;image.kind=diagnostic?GATE_IMAGE_DIAGNOSTIC:0;memcpy(image.requirement,SOURCE+0x200,44);
 UpdateSha256 sha;UpdateSha256_Init(&sha);UpdateSha256_Feed(&sha,SOURCE,GATE_PRODUCT_BYTES);UpdateSha256_Final(&sha,image.sha256);memcpy(durable.candidate_sha,image.sha256,32);
 memset(FLASH_IMAGE,0x55,GATE_PRODUCT_BYTES);fail_at=mode<=101?mode:0;cut_after=mode==101;mutate_source=mode==103;
 if(mode==102)SOURCE[8192]^=1;
 T(GateEngine_Begin(&engine,&io,&durable,&image,1,1));Run();
 if(mode==102){T(engine.state==GATE_ENGINE_FAILED);T(engine.error==GATE_E_HASH);T(!writes&&!erases);return 0;}
 if(mode==103){T(engine.state==GATE_ENGINE_FAILED);T(engine.error==GATE_E_HASH);T(durable.state==GATE_J_COPYING);return 0;}
 if(mode){T(engine.state==GATE_ENGINE_FAILED);
  if(mode==1){T(!writes&&!erases);T(durable.state==GATE_J_READY);return 0;}
  fail_at=0;operation=0;
  if(durable.state==GATE_J_READY){T(mode==101);T(!memcmp(FLASH_IMAGE,SOURCE,GATE_PRODUCT_BYTES));return 0;}
  T(durable.state==GATE_J_COPYING);T(GateEngine_Begin(&engine,&io,&durable,&image,1,1));Run();}
 T(engine.state==GATE_ENGINE_READY);T(durable.state==GATE_J_READY);T(durable.active==1);T(durable.candidate==GATE_NO_SLOT);T(!memcmp(FLASH_IMAGE,SOURCE,GATE_PRODUCT_BYTES));
 T(GateEngine_Begin(&engine,&io,&durable,&image,1,0));Run();T(engine.state==GATE_ENGINE_READY);return 0;}

/* Same copy engine and cut/restart cases, with the typed temporary role. */
uint32_t TestDiagnostic(uint32_t cut){
 diagnostic=1;memset(SOURCE+0x208,0,36);SOURCE[0x238]=3;
 uint32_t r=TestEngine(cut);if(r)return r;
 GateJournalRecord j={.active=0,.state=GATE_J_READY,.flags=GATE_F_DIAGNOSTIC,.uid={1,2,3}},copy;
 GateJournal_Encode(header,&j);T(GateJournal_Decode(header,uid,&copy));T(!GateJournal_DiagnosticWait(&copy));
 j.state=GATE_J_BOOT_PENDING;T(GateJournal_DiagnosticWait(&j));T(!GateJournal_RequestRollback(&j,GATE_FAILURE_RESET));
 j.state=GATE_J_CONFIRMED;GateJournal_Encode(header,&j);T(!GateJournal_Decode(header,uid,&copy));
 j.state=GATE_J_READY;j.flags|=GATE_F_TRIAL;GateJournal_Encode(header,&j);T(!GateJournal_Decode(header,uid,&copy));
 image.kind=GATE_IMAGE_DIAGNOSTIC;memset(image.requirement+8,0,36);GateImage_Encode(header,&image);
 GateImageInfo decoded;T(GateImage_Decode(header,uid,&decoded));T(decoded.kind==GATE_IMAGE_DIAGNOSTIC);
 image.requirement[8]=1;GateImage_Encode(header,&image);T(!GateImage_Decode(header,uid,&decoded));
 image.kind=GATE_IMAGE_PRODUCT;image.requirement[8]=0;GateImage_Encode(header,&image);T(!GateImage_Decode(header,uid,&decoded));
 return 0;
}
