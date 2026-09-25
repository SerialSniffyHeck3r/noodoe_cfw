#include "gate_menu.h"
void GateMenu_Init(GateMenu *s,uint32_t keys,uint32_t now)
{*s=(GateMenu){.down=keys,.edge_ms=now};}
uint32_t GateMenu_Process(GateMenu *s,uint32_t keys,uint32_t now)
{
 uint32_t edge=0;
 if(keys!=s->down&&now-s->edge_ms>=80U){edge=keys&~s->down;s->down=keys;s->edge_ms=now;}
 if(s->page==1){
  if(!(keys&2)){s->released=1;s->held=s->hold_ms=0;}
  if((keys&2)&&s->released){if(!s->held){s->held=1;s->hold_started=now;}
   s->hold_ms=now-s->hold_started;if(s->hold_ms>=2000U){s->released=0;return 1;}}
  if(edge&5){s->page=0;s->released=s->held=s->hold_ms=0;}
 }else if(s->page){
  if(edge&2)s->page=0;
  else if(s->page==3&&(edge&5))s->help=(s->help+((edge&1)?3:1))%4;
 }else{
  if(edge&5)s->selection=(s->selection+((edge&1)?2:1))%3;
  if(edge&2){s->page=s->selection+1;s->released=s->held=s->hold_ms=0;}
 }
 return 0;
}
