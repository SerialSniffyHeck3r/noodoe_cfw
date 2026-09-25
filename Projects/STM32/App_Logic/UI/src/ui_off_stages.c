#include "Ui_OffStages.h"
static const uint32_t states[]={OFF_DISPLAY_HOLD,OFF_BT_HOLD,OFF_DEEP_SLEEP};
/* An invalid empty mask falls back to screen hold, never an unrequested STOP. */
static uint32_t Mask(const UiConfig *c)
{uint32_t m=c->off_stage_mask&UI_OFF_STAGE_ALL;return m?m:UI_OFF_STAGE_DISPLAY;}
static uint32_t Duration(const UiConfig *c,uint32_t index)
{return index==0?c->standby_ms:c->bt_retention_ms;}
static uint32_t Select(const UiConfig *c,uint32_t begin)
{
    uint32_t m=Mask(c);
    for(uint32_t i=begin;i<3;++i){
        if(!(m&(1U<<i)))continue;
        if(i==2||Duration(c,i)||(m&~((1U<<(i+1))-1U))==0)return states[i];
    }
    return UI_BOOT; /* No later enabled stage; caller keeps its current state. */
}
uint32_t Ui_OffStages_IsOff(uint32_t p)
{return p==OFF_DISPLAY_HOLD||p==OFF_BT_HOLD||p==OFF_DEEP_SLEEP;}
uint32_t Ui_OffStages_First(const UiConfig *c){return Select(c,0);}
uint32_t Ui_OffStages_Next(const UiConfig *c,uint32_t power,uint32_t elapsed)
{
    uint32_t i;for(i=0;i<3&&states[i]!=power;++i){}
    if(i==3)return power;
    if(!(Mask(c)&(1U<<i))){uint32_t n=Select(c,i+1);return n==UI_BOOT?Select(c,0):n;}
    if(i==2||elapsed<Duration(c,i))return power;
    uint32_t n=Select(c,i+1);return n==UI_BOOT?power:n;
}
