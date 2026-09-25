#include "Bootstrap_Confirm.h"
void BootstrapConfirm_Init(BootstrapConfirm *s)
{*s=(BootstrapConfirm){0};}
uint32_t BootstrapConfirm_Process(BootstrapConfirm *s,uint32_t now,uint32_t pressed)
{
 if(!pressed){s->released=1;s->holding=s->elapsed=0;return 0;}
 if(!s->released||s->fired)return 0;
 if(!s->holding){s->holding=1;s->since=now;}
 s->elapsed=now-s->since;
 if(s->elapsed<BOOTSTRAP_CONFIRM_MS)return 0;
 s->elapsed=BOOTSTRAP_CONFIRM_MS;s->fired=1;return 1;
}
