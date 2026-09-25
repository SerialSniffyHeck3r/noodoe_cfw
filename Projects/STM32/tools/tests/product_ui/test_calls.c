#include "Ui_State.h"
#include "Phone_Calls.h"
#include <string.h>
extern void UiCalls_Update(UiState*,const PhoneCallsSnapshot*,uint32_t);
extern uint32_t UiCalls_Navigate(UiState*,uint32_t,uint32_t);
#define CHECK(x) do{if(!(x))return __LINE__;}while(0)
static unsigned same(const void *a,const void *b,unsigned size){const unsigned char *x=a,*y=b;for(unsigned i=0;i<size;i++)if(x[i]!=y[i])return 0;return 1;}
unsigned test_calls(void)
{
 UiState s;UiConfig config=Ui_DefaultConfig();Ui_Init(&s,&config,0);s.power=IGN_ON;s.now_ms=100;s.dashboard.card=UI_MUSIC;
 PhoneCallsSnapshot p={.epoch=7,.generation=2,.permissions=15,.count=2,.received_ms=100,.entries={{10,0},{20,1}}};
 UiCalls_Update(&s,&p,1);CHECK(s.dashboard.card==UI_MUSIC);
 p.active_id=0x80000001U;p.state=CALL_RINGING;s.pressed_buttons=UI_BIT(UI_ENTER);unsigned context=s.context_token;
 UiCalls_Update(&s,&p,1);CHECK(s.dashboard.card==UI_CALLS&&s.context_token!=context&&s.calls.return_valid);
 CHECK(s.consumed_buttons&UI_BIT(UI_ENTER));
 UiEffect effect;while(Ui_TakeEffect(&s,&effect)){}
 CHECK(UiCalls_Navigate(&s,UI_ENTER,1));CHECK(Ui_TakeEffect(&s,&effect)&&effect.kind==UI_FX_CALL&&effect.arg==CALL_ANSWER&&effect.value==p.active_id);
 p.permissions=0;UiCalls_Update(&s,&p,1);CHECK(UiCalls_Navigate(&s,UI_DOWN,1)&&!Ui_TakeEffect(&s,&effect));
 p.state=CALL_IDLE;p.active_id=0;UiCalls_Update(&s,&p,1);CHECK(s.dashboard.card==UI_MUSIC&&!s.calls.return_valid);
 p.state=CALL_RINGING;p.active_id=0x80000002U;UiCalls_Update(&s,&p,0);CHECK(s.dashboard.card==UI_MUSIC);
 UiCalls_Update(&s,&p,1);CHECK(s.dashboard.card==UI_CALLS);
 s.now_ms=5101;UiCalls_Update(&s,&p,1);CHECK(s.dashboard.card==UI_MUSIC&&!s.calls.phone.epoch);
 p.state=CALL_IDLE;p.active_id=0;p.permissions=15;p.received_ms=5101;UiCalls_Update(&s,&p,1);s.dashboard.card=UI_CALLS;
 CHECK(UiCalls_Navigate(&s,UI_DOWN,0)&&s.calls.selected==1&&PhoneCalls_Target(&s.calls)==20);
 context=s.context_token;s.pressed_buttons=UI_BIT(UI_ENTER);p.generation++;p.count=1;UiCalls_Update(&s,&p,1);
 CHECK(s.context_token!=context&&s.calls.selected==0);
 return 0;
}
unsigned test_calls_wire(void)
{
 uint32_t words[50]={1,2,15,20,0,0,0,0,0,0};
 for(unsigned i=0;i<20;i++){words[10+2*i]=i+1;words[11+2*i]=i/10;}
 PhoneCallsSnapshot a={0},before;
 CHECK(PhoneCalls_Decode(&a,(uint8_t*)words,sizeof(words),1,0));CHECK(a.count==20);
 before=a;words[48]=1;CHECK(!PhoneCalls_Decode(&a,(uint8_t*)words,sizeof(words),1,1));CHECK(same(&a,&before,sizeof(a)));words[48]=20;
 CHECK(!PhoneCalls_Decode(&a,(uint8_t*)words,sizeof(words)-1,1,1));
 words[3]=21;CHECK(!PhoneCalls_Decode(&a,(uint8_t*)words,sizeof(words),1,1));words[3]=20;
 words[4]=5;words[5]=CALL_RINGING;CHECK(!PhoneCalls_Decode(&a,(uint8_t*)words,sizeof(words),1,1));words[4]=0x80000005U;
 CHECK(PhoneCalls_Decode(&a,(uint8_t*)words,sizeof(words),1,0xFFFFFF00U));CHECK(PhoneCalls_Fresh(&a,200));CHECK(!PhoneCalls_Fresh(&a,7000));
 return 0;
}
