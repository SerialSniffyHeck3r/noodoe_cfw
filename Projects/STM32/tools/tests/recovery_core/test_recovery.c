#include "Recovery_Core.h"
#include "Recovery_Buttons.h"
#include <stddef.h>
#include <stdint.h>
void *memset(void *p,int v,size_t n){uint8_t *q=p;while(n--)*q++=(uint8_t)v;return p;}
void *memcpy(void *d,const void *s,size_t n){uint8_t *p=d;const uint8_t *q=s;while(n--)*p++=*q++;return d;}
int memcmp(const void *a,const void *b,size_t n){const uint8_t *p=a,*q=b;while(n--){if(*p!=*q)return *p-*q;++p;++q;}return 0;}
volatile uint32_t assertions;
#define CHECK(v) do{++assertions;if(!(v))return __LINE__;}while(0)
static RecoveryCore core;
static uint32_t meta[5],erase_count,program_count,commit_count,reset_count,enabled;
static uint32_t fail_program,fail_commit,flip_source,flip_stage;
static uint8_t * const stage=(uint8_t*)0x12000000U;
static const uint8_t * const stock=(const uint8_t*)0x11000000U;
static uint32_t ReadSource(void *c,uint32_t a,void *d,uint32_t n)
{(void)c;if(a>RECOVERY_STOCK_BYTES||n>RECOVERY_STOCK_BYTES-a)return 1;
 memcpy(d,stock+a,n);if(flip_source&&a==4096)((uint8_t*)d)[42]^=1;return 0;}
static uint32_t ReadStage(void *c,uint32_t a,void *d,uint32_t n)
{(void)c;if(a>RECOVERY_STOCK_BYTES||n>RECOVERY_STOCK_BYTES-a)return 1;
 memcpy(d,stage+a,n);if(flip_stage&&a==4096)((uint8_t*)d)[42]^=1;return 0;}
static uint32_t Enable(void *c){(void)c;enabled=1;return 0;}
static void Disable(void *c){(void)c;enabled=0;}
static uint32_t Erase(void *c,uint32_t a)
{(void)c;if(!enabled||(a&4095)||a>RECOVERY_STOCK_BYTES-4096)return 1;++erase_count;memset(stage+a,255,4096);return 0;}
static uint32_t Program(void *c,uint32_t a,const void *d,uint32_t n)
{(void)c;if(!enabled||n!=256||(a&255)||a>RECOVERY_STOCK_BYTES-n)return 1;
 ++program_count;if(fail_program&&program_count==fail_program)return 1;memcpy(stage+a,d,n);return 0;}
static uint32_t MetadataRead(void *c,uint32_t m[5]){(void)c;memcpy(m,meta,20);return 0;}
static uint32_t Commit(void *c,uint32_t v,uint32_t crc)
{(void)c;++commit_count;if(fail_commit){meta[0]=UINT32_MAX;return 1;}
 meta[1]=v;meta[2]=0x7F90;meta[3]=RECOVERY_STOCK_BYTES;meta[4]=crc;return 0;}
static void Reset(void *c){(void)c;++reset_count;}
static void Init(void)
{RecoveryPlatform p={0,ReadStage,Enable,Erase,Program,Disable,MetadataRead,Commit,Reset};
 RecoveryCore_Init(&core,&p);memset(meta,0,20);meta[0]=0xE0000;erase_count=program_count=commit_count=reset_count=enabled=0;
 fail_program=fail_commit=flip_source=flip_stage=0;}
static uint32_t RunTo(uint32_t state)
{uint32_t n=0;while(core.state!=state&&core.state!=RECOVERY_ERROR&&n++<3000)RecoveryCore_Process(&core);return core.state;}
int RecoveryTest_Main(void)
{
 RecoverySource s={0,ReadSource};Init();
 meta[0]=0x100000;CHECK(RecoveryCore_Request(&core,&s)==RECOVERY_RESIDENT);CHECK(!erase_count);
 Init();meta[4]=1;CHECK(RecoveryCore_Request(&core,&s)==RECOVERY_PENDING);CHECK(!erase_count);
 Init();flip_source=1;CHECK(!RecoveryCore_Request(&core,&s));CHECK(RunTo(RECOVERY_VERIFIED)==RECOVERY_ERROR);
 CHECK(core.result==RECOVERY_HASH&&!erase_count&&!commit_count);
 Init();CHECK(!RecoveryCore_Request(&core,&s));CHECK(RunTo(RECOVERY_ERASE)==RECOVERY_ERASE);
 fail_program=17;CHECK(RunTo(RECOVERY_VERIFIED)==RECOVERY_ERROR);CHECK(program_count==17&&erase_count==2&&!meta[4]&&!commit_count);
 /* A reboot during staging does not install: metadata is still inactive. */
 Init();CHECK(!RecoveryCore_Request(&core,&s));CHECK(RunTo(RECOVERY_ERASE)==RECOVERY_ERASE);
 flip_source=1;CHECK(RunTo(RECOVERY_VERIFIED)==RECOVERY_ERROR);CHECK(core.result==RECOVERY_HASH&&!meta[4]&&!commit_count);
 Init();meta[0]=0xF0000;CHECK(!RecoveryCore_Request(&core,&s));CHECK(RunTo(RECOVERY_VERIFIED)==RECOVERY_VERIFIED);
 CHECK(erase_count==112&&program_count==1792&&!memcmp(stock,stage,RECOVERY_STOCK_BYTES));
 CHECK(!RecoveryCore_Commit(&core,RECOVERY_CONFIRM_TOKEN));CHECK(core.state==RECOVERY_COMMITTED&&commit_count==1&&meta[0]==0xF0000);
 CHECK(RecoveryCore_Commit(&core,RECOVERY_CONFIRM_TOKEN)==RECOVERY_STATE&&commit_count==1);
 CHECK(RecoveryCore_Reset(&core,0)==RECOVERY_STATE&&!reset_count);
 CHECK(RecoveryCore_Reset(&core,RECOVERY_CONFIRM_TOKEN)==RECOVERY_IO&&reset_count==1);
 /* Reuse the previously verified digest to inject writer failure only. */
 meta[4]=0;core.state=RECOVERY_VERIFIED;core.commit_attempted=0;fail_commit=1;
 CHECK(RecoveryCore_Commit(&core,RECOVERY_CONFIRM_TOKEN)==RECOVERY_AMBIGUOUS);
 CHECK(core.state==RECOVERY_ERROR&&reset_count==1);
 CHECK(RecoveryCore_Commit(&core,RECOVERY_CONFIRM_TOKEN)==RECOVERY_STATE&&commit_count==2);
 Init();CHECK(!RecoveryCore_Request(&core,&s));RecoveryCore_Cancel(&core);CHECK(core.state==RECOVERY_ERROR&&!erase_count&&!meta[4]);
 RecoveryButtons b;RecoveryButtons_Init(&b,0,0);
 RecoveryButtons_Process(&b,3,1);RecoveryButtons_Process(&b,3,81);RecoveryButtons_Process(&b,3,3080);CHECK(b.state==0);
 RecoveryButtons_Process(&b,3,3081);CHECK(b.state==1);
 RecoveryButtons_Process(&b,2,3082);RecoveryButtons_Process(&b,2,3162);RecoveryButtons_Process(&b,2,6000);CHECK(b.state==1);
 RecoveryButtons_Process(&b,0,6001);RecoveryButtons_Process(&b,0,6081);CHECK(b.state==2);
 RecoveryButtons_Process(&b,2,6082);RecoveryButtons_Process(&b,2,6162);RecoveryButtons_Process(&b,2,8162);CHECK(b.state==2);
 RecoveryButtons_Process(&b,0,8163);RecoveryButtons_Process(&b,0,8243);CHECK(b.state==3);
 RecoveryButtons_Init(&b,1,UINT32_MAX-100);RecoveryButtons_Process(&b,0,UINT32_MAX-10);CHECK(b.state==2);
 RecoveryButtons_Process(&b,1,10);RecoveryButtons_Process(&b,1,90);CHECK(b.state==4);
 return 0;
}
