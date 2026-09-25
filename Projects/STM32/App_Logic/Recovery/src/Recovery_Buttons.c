#include "Recovery_Buttons.h"
#include <string.h>
void RecoveryButtons_Init(RecoveryButtons *s,uint32_t wait_release,uint32_t now)
{memset(s,0,sizeof(*s));s->state=wait_release?1U:0U;s->changed=now;s->held=now;}
/* Debounced, wrap-safe two-stage gesture. Enter must be released after the
 * chord, then held alone for2sec, then released to perform the operation. */
void RecoveryButtons_Process(RecoveryButtons *s,uint32_t raw,uint32_t now)
{
 raw&=3U;if(raw!=s->raw){s->raw=raw;s->changed=now;}
 if(now-s->changed<80U)return;
 uint32_t old=s->stable;s->stable=raw;
 if(s->state==0){if(raw!=3U)s->held=now;else if(old!=3U)s->held=now;
  else if(now-s->held>=3000U)s->state=1;}
 else if(s->state==1){if(!raw){s->state=2;s->confirm_held=0;}}
 else if(s->state==2){
  if(raw==RECOVERY_BUTTON_DOWN){s->state=4;return;}
  if(raw==RECOVERY_BUTTON_ENTER){if(old!=raw)s->held=now;else if(now-s->held>=2000U)s->confirm_held=1;}
  else if(!raw&&old==RECOVERY_BUTTON_ENTER&&s->confirm_held)s->state=3;
  else if(raw!=RECOVERY_BUTTON_ENTER)s->confirm_held=0;
 }
}
