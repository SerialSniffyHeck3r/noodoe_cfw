#include "event_log.h"
#include "gate_abi.h"
#include <string.h>
static uint32_t U(const uint8_t *p){return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void W(uint8_t *p,uint32_t v){for(uint32_t i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static uint32_t Header(const uint8_t *p,const uint32_t uid[3],uint32_t magic)
{if(U(p)!=magic||U(p+4)!=1||U(p+4092)!=EVENT_LOG_COMMIT||U(p+4088)!=GatePolicy_Crc(p,4088))return 0;
 for(uint32_t i=0;i<3;i++)if(U(p+16+i*4)!=uid[i])return 0;
 return 1;}
static void Seal(uint8_t *p){W(p+4088,GatePolicy_Crc(p,4088));W(p+4092,EVENT_LOG_COMMIT);}
uint32_t GateFault_Valid(const GateFault *f)
{return f&&f->magic==0x31464C47U&&f->version==1&&f->crc==GatePolicy_Crc(f,56)&&f->inverse==~f->crc;}
void GateFault_Seal(GateFault *f)
{f->magic=0x31464C47U;f->version=1;f->crc=GatePolicy_Crc(f,56);f->inverse=~f->crc;}
uint32_t GateBootContext_Valid(const GateBootContext *b)
{/* Keep the handoff mask in the same ABI as the durable boot journal. A valid
   * temporary Diagnostic handoff must not be mistaken for corrupt Product data. */
 const uint32_t flags=GATE_F_TRIAL|GATE_F_ROLLED_BACK|GATE_F_RESULT_PENDING|GATE_F_RESET_PENDING|GATE_F_DIAGNOSTIC;
 return b&&b->magic==0x32425447U&&b->version==1&&!(b->flags&~flags)&&b->crc==GatePolicy_Crc(b,24)&&b->inverse==~b->crc;}
void GateBootContext_Seal(GateBootContext *b)
{b->magic=0x32425447U;b->version=1;b->crc=GatePolicy_Crc(b,24);b->inverse=~b->crc;}
void EventLog_Identity(uint8_t *p,const uint32_t uid[3])
{memset(p,255,4096);W(p,0x31494C4EU);W(p+4,1);W(p+8,EVENT_LOG_BYTES);W(p+12,64);
 for(uint32_t i=0;i<3;i++)W(p+16+i*4,uid[i]);
 Seal(p);}
uint32_t EventLog_CheckIdentity(const uint8_t *p,const uint32_t uid[3])
{if(!Header(p,uid,0x31494C4EU)||U(p+8)!=EVENT_LOG_BYTES||U(p+12)!=64)return 0;
 for(uint32_t i=28;i<4088;i++)if(p[i]!=255)return 0;
 return 1;}
uint32_t EventLog_CheckRecord(const uint8_t *p,const uint32_t uid[3])
{if(!Header(p,uid,EVENT_LOG_MAGIC)||!U(p+12)||U(p+12)>EVENT_LOG_EVENTS||U(p+28)!=64)return 0;
 for(uint32_t i=32+64*U(p+12);i<4088;i++)if(p[i]!=255)return 0;
 return 1;}
void EventLog_Init(EventLog *s,const EventLogIO *io,const uint32_t uid[3])
{memset(s,0,sizeof(*s));s->io=*io;memcpy(s->uid,uid,12);s->phase=LOG_SCAN;}
static void Fail(EventLog *s,uint32_t e){s->error=e;s->phase=LOG_ERROR;}
uint32_t EventLog_Submit(EventLog *s,const DeviceEvent *events,uint32_t count)
{if(!s||s->phase!=LOG_READY||!events||!count||count>EVENT_LOG_EVENTS)return 0;
 memset(s->block,255,4096);W(s->block,EVENT_LOG_MAGIC);W(s->block+4,1);W(s->block+8,s->sequence+1);W(s->block+12,count);
 for(uint32_t i=0;i<3;i++)W(s->block+16+4*i,s->uid[i]);
 W(s->block+28,64);
 for(uint32_t i=0;i<count;i++){const uint32_t *e=(const uint32_t*)(events+i);for(uint32_t k=0;k<16;k++)W(s->block+32+64*i+4*k,e[k]);}
 Seal(s->block);s->count=count;s->target=s->found?(s->latest+1)%EVENT_LOG_SECTORS:0;s->page=0;s->phase=LOG_ERASE;return 1;}
void EventLog_Process(EventLog *s)
{if(!s||s->phase==LOG_READY||s->phase==LOG_ERROR)return;
 uint32_t off=s->target*4096,e=0;
 switch(s->phase){
 case LOG_SCAN:
  if(s->scan==0){if(s->io.read(s->io.context,EVENT_LOG_IDENTITY,s->block,4096)||!EventLog_CheckIdentity(s->block,s->uid)){Fail(s,1);return;}}
  else if(s->scan<=EVENT_LOG_SECTORS){
   if(s->io.read(s->io.context,(s->scan-1)*4096,s->block,4096)){Fail(s,2);return;}
   /* Unknown committed versions cannot be overwritten by an older writer. */
   if(U(s->block)==EVENT_LOG_MAGIC&&U(s->block+4)!=1&&U(s->block+4092)==EVENT_LOG_COMMIT&&U(s->block+4088)==GatePolicy_Crc(s->block,4088)){Fail(s,3);return;}
   if(EventLog_CheckRecord(s->block,s->uid)){uint32_t seq=U(s->block+8);
    if(s->found&&(seq==s->sequence||seq-s->sequence==0x80000000U)){Fail(s,4);return;}
    if(!s->found||GateSequence_Newer(seq,s->sequence)){s->sequence=seq;s->latest=s->scan-1;s->found=1;}}
  }else{if(s->io.grant(s->io.context)){Fail(s,5);return;}s->phase=LOG_READY;return;}
  s->scan++;return;
 case LOG_ERASE:e=s->io.erase(s->io.context,off);if(!e)s->phase=LOG_PROGRAM;break;
 case LOG_PROGRAM:{uint32_t n=4092-s->page;if(n>256)n=256;e=s->io.program(s->io.context,off+s->page,s->block+s->page,n);
  if(!e){s->page+=n;if(s->page==4092){s->page=0;s->phase=LOG_VERIFY;}}break;}
 case LOG_VERIFY:{uint32_t n=4096-s->page;if(n>256)n=256;e=s->io.read(s->io.context,off+s->page,s->verify,n);
  if(!e){uint32_t compare=s->page+n==4096?n-4:n;e=memcmp(s->verify,s->block+s->page,compare)!=0;
   if(s->page+n==4096&&U(s->verify+n-4)!=UINT32_MAX)e=1;
   if(!e){s->page+=n;if(s->page==4096)s->phase=LOG_COMMIT;}}break;}
 case LOG_COMMIT:e=s->io.program(s->io.context,off+4092,s->block+4092,4);if(!e)s->phase=LOG_FINAL;break;
 case LOG_FINAL:e=s->io.read(s->io.context,off+4092,s->verify,4);if(!e)e=U(s->verify)!=EVENT_LOG_COMMIT;
  if(!e){s->latest=s->target;s->sequence++;s->found=1;s->written+=s->count;s->phase=LOG_READY;}break;
 default:e=1;break;
 }if(e)Fail(s,6);
}
