#include "Ui_Home.h"
#include <stdio.h>
/* Explicit month/day lookup avoids locale-dependent libc formatting. The
 * weekday comes from the Gregorian calendar, never from an example string. */
void UiHome_Present(const UiState *s,const DashboardPageFacts *f,DashboardPage *p)
{
    p->selection=s->dashboard.selection%UI_HOME_VIEWS;p->key+=p->selection<<8;
    if(!p->selection)return;
    static const char *const months[]={"","Jan.","Feb.","Mar.","Apr.","May","Jun.","Jul.","Aug.","Sep.","Oct.","Nov.","Dec."};
    static const char *const days[]={"","Mon","Tue","Wed","Thu","Fri","Sat","Sun"};
    uint32_t month=f->date/100U%100U,day=f->date%100U;
    if(f->date&&month>=1U&&month<=12U&&day>=1U&&day<=31U&&f->weekday>=1U&&f->weekday<=7U)
        snprintf(p->note,sizeof(p->note),"%s %lu %s",months[month],(unsigned long)day,days[f->weekday]);
    else snprintf(p->note,sizeof(p->note),"Date unavailable");
    if(p->selection==2U){
        p->known=s->speed_valid;
        uint32_t speed=f->units?((uint64_t)s->speed_kph*1000000U+804672U)/1609344U:s->speed_kph;
        if(s->speed_valid)snprintf(p->numbers[0],sizeof(p->numbers[0]),"%lu",(unsigned long)speed);
        else snprintf(p->numbers[0],sizeof(p->numbers[0]),"---");
        snprintf(p->lines[0],sizeof(p->lines[0]),"%s",f->units?"mph":"km/h");
    }
}
/* Only a main-category change restarts this deadline. Subview UP/DOWN,
 * telemetry refresh and button hints cannot hold the Home strip open. */
uint32_t UiHome_StripVisible(uint32_t category,uint32_t now)
{
    static uint32_t previous=UINT32_MAX,entered;
    if(category!=previous){previous=category;entered=now;}
    return category!=UI_BLANK||(uint32_t)(now-entered)<UI_HOME_STRIP_TIMEOUT_MS;
}
