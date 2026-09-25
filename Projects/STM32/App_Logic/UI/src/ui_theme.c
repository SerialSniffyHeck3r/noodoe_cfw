#include "Ui_Theme.h"
uint32_t UiTheme_Step(UiThemeState *s,const UiThemeConfig *c,uint32_t now,
 uint32_t clock_valid,uint32_t minute,uint32_t valid,uint32_t millilux)
{
    if(c->mode<2U){s->light=c->mode==1U;s->armed=0;return s->light;}
    if(c->source){
        s->armed=0;
        if(clock_valid&&c->day_minute!=c->night_minute)
            s->light=c->day_minute<c->night_minute?
                minute>=c->day_minute&&minute<c->night_minute:
                minute>=c->day_minute||minute<c->night_minute;
        return s->light;
    }
    if(!valid||c->dark_lux>=c->light_lux){s->armed=0;return s->light;}
    uint32_t target=millilux<=c->dark_lux*1000U?0U:
        millilux>=c->light_lux*1000U?1U:s->light;
    if(target==s->light){s->armed=0;return s->light;}
    if(!s->armed||s->candidate!=target){s->armed=1;s->candidate=target;s->since=now;}
    else if(now-s->since>=3000U){s->light=target;s->armed=0;}
    return s->light;
}
