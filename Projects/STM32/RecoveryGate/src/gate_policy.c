#include "Noodoe_Crc32.h"
#include "gate_policy.h"
#include <string.h>
#define MAGIC 0x31455447U
uint32_t GatePolicy_Crc(const void *data,uint32_t n)
{return Noodoe_Crc32(data,n);}
static void Seal(GateRetained *s){s->crc=GatePolicy_Crc(s,24);s->inverse=~s->crc;}
uint32_t GateRetained_Valid(const GateRetained *s)
{return s&&s->magic==MAGIC&&s->version==GATE_ABI_VERSION&&s->reason<=GATE_REASON_INIT_FAILED&&s->attempts<=GATE_BOOT_FAILURE_LIMIT&&s->crc==~s->inverse&&s->crc==GatePolicy_Crc(s,24);}
void GateRetained_Init(GateRetained *s)
{memset(s,0,sizeof(*s));s->magic=MAGIC;s->version=GATE_ABI_VERSION;Seal(s);}
void GateRetained_Request(GateRetained *s,uint32_t reason)
{if(!s||reason>GATE_REASON_INIT_FAILED)return;if(!GateRetained_Valid(s))GateRetained_Init(s);s->reason=reason;Seal(s);}
uint32_t GateRetained_BeforeBoot(GateRetained *s)
{if(!GateRetained_Valid(s))GateRetained_Init(s);
 if(s->confirmed==s->sequence)s->attempts=0;
 if(s->attempts>=GATE_BOOT_FAILURE_LIMIT){s->reason=GATE_REASON_WAIT;Seal(s);return 1;}
 ++s->attempts;++s->sequence;if(!s->sequence)++s->sequence;s->confirmed=0;s->reason=GATE_REASON_NORMAL;Seal(s);return 0;}
uint32_t GateRetained_Confirm(GateRetained *s,uint32_t healthy_ms)
{if(!GateRetained_Valid(s)||healthy_ms<GATE_CONFIRM_MS||!s->sequence||s->reason!=GATE_REASON_NORMAL)return 0;
 s->confirmed=s->sequence;s->attempts=0;Seal(s);return 1;}
void GateGesture_Init(GateGesture *s,uint32_t now,uint32_t enter)
{memset(s,0,sizeof(*s));s->raw_enter=!!enter;s->changed=now;s->pressed=now;}
uint32_t GateGesture_Process(GateGesture *s,uint32_t now,uint32_t ign_on,uint32_t enter)
{
 enter=!!enter;ign_on=!!ign_on;
 if(enter!=s->raw_enter){s->raw_enter=enter;s->changed=now;}
 if(s->raw_enter!=s->enter&&now-s->changed>=80U){s->enter=s->raw_enter;if(s->enter)s->pressed=s->changed;}
 /* Any raw release cancels authorization immediately; debounce can only arm. */
 if(!enter){s->armed=0;s->on_pending=0;s->qualified=0;s->expired=0;}
 if(enter&&s->qualified&&now-s->armed_since>=30000U){s->expired=1;s->armed=0;s->on_pending=0;}
 if(s->expired)return 0;
 if(!ign_on){if(!s->off_known){s->off_since=now;s->off_known=1;}s->on_pending=0;
  if(s->enter&&enter&&now-s->off_since>=200U&&now-s->pressed>=500U){
   if(!s->qualified){s->qualified=1;s->armed_since=now;}s->armed=1;}
 }else{if(s->off_known&&s->armed&&s->enter&&enter){s->on_since=now;s->on_pending=1;}
  s->off_known=0;s->armed=0;
  if(s->on_pending&&s->enter&&enter&&now-s->on_since>=2000U){s->on_pending=0;return 1;}}
 return 0;
}
