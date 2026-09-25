#include "ui_state_internal.h"
/* Incoming calls may preempt only the normal driving page. Context changes
 * invalidate an already held key; it must be released before answering. */
void UiCalls_Update(UiState *s,const PhoneCallsSnapshot *input,uint32_t allowed)
{
 UiCalls *c=&s->calls;uint32_t target=PhoneCalls_Target(c),old_epoch=c->phone.epoch;
 PhoneCallsSnapshot next={0};if(PhoneCalls_Fresh(input,s->now_ms))next=*input;
 if(s->dashboard.card==UI_CALLS&&(c->phone.epoch!=next.epoch||c->phone.generation!=next.generation||c->phone.state!=next.state||c->phone.active_id!=next.active_id))Ui_ContextChanged(s);
 if(old_epoch!=next.epoch){c->selected=0;c->shown_incoming=0;}
 else if(next.generation!=c->phone.generation){c->selected=0;for(uint32_t i=0;i<next.count;i++)if(next.entries[i].id==target)c->selected=i;}
 c->phone=next;if(c->selected>=next.count)c->selected=0;
 if(!next.state&&c->return_valid){
  if(s->dashboard.card==UI_CALLS&&s->power==IGN_ON){s->dashboard.card=c->return_card;s->dashboard.selection=c->return_selection;Ui_ContextChanged(s);}
  c->return_valid=0;
 }
 if(allowed&&next.state==CALL_RINGING&&next.active_id!=c->shown_incoming){
  c->shown_incoming=next.active_id;
  if(s->dashboard.card!=UI_CALLS){c->return_card=s->dashboard.card;c->return_selection=s->dashboard.selection;c->return_valid=1;}
  s->dashboard.card=UI_CALLS;s->dashboard.selection=0;Ui_ContextChanged(s);
 }
}
uint32_t UiCalls_Navigate(UiState *s,uint32_t button,uint32_t hold)
{
 if(s->dashboard.card!=UI_CALLS)return 0;
 UiCalls *c=&s->calls;PhoneCallsSnapshot *p=&c->phone;
 if(button==UI_ENTER&&!hold)return 0; /* One forward category step. */
 if(!PhoneCalls_Fresh(p,s->now_ms))return 1;
 if(p->state){
  if(hold&&button==UI_ENTER&&p->state==CALL_RINGING&&(p->permissions&CALL_CONTROL_PERMISSION))Ui_Emit(s,UI_FX_CALL,CALL_ANSWER,p->active_id);
  if(hold&&button==UI_DOWN&&(p->permissions&CALL_CONTROL_PERMISSION))Ui_Emit(s,UI_FX_CALL,CALL_END,p->active_id);
 }else if(button==UI_ENTER){
  if(hold&&(p->permissions&CALL_DIAL_PERMISSION)){uint32_t target=PhoneCalls_Target(c);if(target)Ui_Emit(s,UI_FX_CALL,CALL_DIAL,target);}
 }else if(!hold&&p->count){
  c->selected=button==UI_DOWN?(c->selected+1)%p->count:(c->selected+p->count-1)%p->count;
 }
 return 1;
}
