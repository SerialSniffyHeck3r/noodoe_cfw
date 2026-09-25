#include "Odometer_Guard.h"
#include <limits.h>
/* Only distinct, fresh UART samples advance confirmation. A gap restarts the
 * three-second observation window. No speed integration manufactures ODO. */
void OdometerGuard_Feed(OdometerGuard *s,uint32_t now,uint32_t sample,uint32_t valid,uint32_t raw)
{
 if(!s)return;
 if(!valid){s->tracking=0;s->have_sample=0;return;}
 if(s->have_sample&&sample==s->last_sample)return;
 uint32_t continuous=s->have_sample&&now-s->last_ms<=2000U,dt=now-s->last_ms;
 s->last_sample=sample;s->last_ms=now;s->have_sample=1;
 /* Raw and offset are bounded to +/-999999, so signed 32-bit arithmetic
  * covers their sum without a 64-bit runtime path. Reject raw before cast. */
 int32_t value=raw<=ODOMETER_MAX_KM?(int32_t)raw+s->offset:-1;
 uint32_t reason=raw>ODOMETER_MAX_KM||value<0||value>ODOMETER_MAX_KM?ODO_RANGE:
   s->valid&&raw<s->raw?ODO_REVERSED:
   s->valid&&raw>s->raw&&raw-s->raw>(continuous?(dt+8999U)/9000U+1U:1U)?ODO_JUMP:ODO_NORMAL;
 /* An already quarantined jump remains quarantined even when time passes.
  * Only returning to the previous neighbourhood can resolve it unaided. */
 if(s->reason==ODO_JUMP&&raw>s->raw+1U)reason=ODO_JUMP;
 if(reason){
  if(!s->tracking||!continuous||reason!=s->reason||raw<s->candidate){
   s->tracking=1;s->since=now;s->count=0;
   if(!s->pending)s->candidate_base=raw;
  }
  s->candidate=raw;s->reason=reason;++s->count;
  if(!s->pending&&s->count>=3&&now-s->since>=ODOMETER_CONFIRM_MS){s->pending=1;s->dirty=1;++s->revision;}
  return;
 }
 if(!s->valid||s->raw!=raw||s->pending){s->dirty=1;++s->revision;}
 s->valid=1;s->raw=raw;s->display=(uint32_t)value;s->pending=s->reason=s->tracking=0;
}
/* Caller has enforced stationary/epoch/revision policy. A KEEP choice anchors
 * the old display at the first anomalous sample, preserving later increments. */
uint32_t OdometerGuard_Choose(OdometerGuard *s,uint32_t decision)
{
 if(!s||!s->pending||decision<ODO_KEEP||decision>ODO_LATER)return 0;
 if(decision==ODO_LATER)return 1;
 if(s->reason==ODO_RANGE||s->candidate>ODOMETER_MAX_KM)return 0;
 int32_t offset=decision==ODO_KEEP?(int32_t)s->display-(int32_t)s->candidate_base:0;
 int32_t value=(int32_t)s->candidate+offset;
 if(value<0||value>ODOMETER_MAX_KM)return 0;
 s->offset=offset;s->display=(uint32_t)value;s->raw=s->candidate;s->corrected=offset!=0;
 s->pending=s->tracking=s->reason=0;s->dirty=1;++s->revision;return 1;
}
