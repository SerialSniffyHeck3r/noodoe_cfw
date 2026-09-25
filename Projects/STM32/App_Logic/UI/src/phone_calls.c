#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "RAM codec requires little-endian target"
#endif
#include "Phone_Calls.h"
#include <string.h>
static uint32_t Word(const uint8_t *p){uint32_t v;memcpy(&v,p,4);return v;}
uint32_t PhoneCalls_Decode(PhoneCallsSnapshot *out,const uint8_t *p,uint32_t n,uint32_t epoch,uint32_t now)
{
 if(!out||!p||!epoch||n<40||(Word(p)!=1&&Word(p)!=2))return 0;
 PhoneCallsSnapshot v={.epoch=epoch,.generation=Word(p+4),.permissions=Word(p+8),.count=Word(p+12),
  .active_id=Word(p+16),.state=Word(p+20),.elapsed_s=Word(p+24),.visual_target=Word(p+28),.visual_key=Word(p+32),.received_ms=now,.call_type=Word(p+36)};
 if(!v.generation||v.permissions>15||v.count>PHONE_CALLS_MAX||v.state>CALL_ENDING||(Word(p)==1?v.call_type!=0:v.call_type>7)||n!=40+v.count*8||
    (!v.state&&v.active_id)||(v.state&&!v.active_id)||!!v.visual_target!=!!v.visual_key)return 0;
 uint32_t groups[2]={0};
 for(uint32_t i=0;i<v.count;i++){
  PhoneCallEntry *e=&v.entries[i];e->id=Word(p+40+8*i);e->group=Word(p+44+8*i);
  if(!e->id||e->id>=0x80000000U||e->group>1||++groups[e->group]>10||(i&&e->group<v.entries[i-1].group))return 0;
  for(uint32_t j=0;j<i;j++)if(v.entries[j].id==e->id)return 0;
 }
 if(v.active_id&&v.active_id<0x80000000U)return 0;
 if(v.visual_target){uint32_t found=v.visual_target==v.active_id;
  for(uint32_t i=0;i<v.count;i++)found|=v.visual_target==v.entries[i].id;
  if(!found)return 0;
 }
 *out=v;return 1;
}
uint32_t PhoneCalls_Fresh(const PhoneCallsSnapshot *p,uint32_t now)
{return p&&p->epoch&&p->generation&&(uint32_t)(now-p->received_ms)<=5000U;}
uint32_t PhoneCalls_Target(const UiCalls *c)
{if(!c)return 0;if(c->phone.state)return c->phone.active_id;return c->selected<c->phone.count?c->phone.entries[c->selected].id:0;}
