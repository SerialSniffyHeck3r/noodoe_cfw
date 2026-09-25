#include "Phone_Content.h"
#include <string.h>
/* Bounded in-place UTF-8 reduction for the currently installed ASCII font.
 * Newline/control characters cannot change fixed layout or create extra rows. */
static void Text(char *s,uint32_t cap)
{
    uint32_t r=0,w=0;
    while(r<cap-1U&&s[r]){uint8_t c=(uint8_t)s[r++];
        if(c>=128U){if((c&0xC0U)==0x80U)continue;c='?';}
        s[w++]=(char)(c>=32U&&c<=126U?c:' ');
    }s[w]=0;
}
void PhoneContent_Init(PhoneContent *s){if(s)memset(s,0,sizeof(*s));}
void PhoneContent_Links(PhoneContent *s,uint32_t mask)
{
    if(!s)return;
    for(uint32_t i=0;i<PHONE_CONTENT_SLOTS;++i){PhoneContentSlot *p=&s->slots[i];uint32_t on=!!(mask&(1U<<i));
        if(on==p->connected)continue;
        memset(p,0,sizeof(*p));p->connected=on;
        if(on){if(!++s->token_counter)++s->token_counter;p->token=s->token_counter;}
    }
}
static PhoneContentSlot *Slot(PhoneContent *s,uint32_t i,uint32_t token)
{return s&&i<PHONE_CONTENT_SLOTS&&token&&s->slots[i].connected&&s->slots[i].token==token?&s->slots[i]:0;}
uint32_t PhoneContent_Status(PhoneContent *s,uint32_t i,uint32_t token,const PhoneStatus *v,uint32_t now)
{
    PhoneContentSlot *p=Slot(s,i,token);
    if(!p||!v||v->count>PHONE_NOTIFICATION_CAPACITY||(v->battery_valid&&v->battery_percent>100U))return 0;
    p->status=*v;p->status_ms=now;++p->status_revision;
    for(uint32_t n=0;n<p->status.count;++n){PhoneNotification *q=&p->status.notifications[n];Text(q->app,sizeof(q->app));Text(q->title,sizeof(q->title));Text(q->body,sizeof(q->body));}
    return 1;
}
uint32_t PhoneContent_Music(PhoneContent *s,uint32_t i,uint32_t token,const PhoneMusic *v,uint32_t now)
{
    PhoneContentSlot *p=Slot(s,i,token);
    if(!p||!v||(v->duration_ms&&v->position_ms>v->duration_ms))return 0;
    p->music=*v;p->music_ms=now;++p->music_revision;
    Text(p->music.title,sizeof(p->music.title));Text(p->music.artist,sizeof(p->music.artist));return 1;
}
