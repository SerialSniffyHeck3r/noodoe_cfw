#include "Ui_Home.h"
#include "Dashboard_Pages.h"
#include "Gps_GridFade.h"
#include <stdio.h>
#include <string.h>
static __attribute__((noinline)) void Copy(char *d,uint32_t cap,const char *s){snprintf(d,cap,"%s",s?s:"");}
/* A duration has no decorative leading hours zero. Minutes/seconds keep their
 * positional clock convention, while all distance/speed digits are natural. */
static void Duration(char *d,uint32_t cap,uint64_t ms)
{uint64_t seconds=ms/1000U;snprintf(d,cap,"%lu:%02lu:%02lu",(unsigned long)(seconds/3600U),(unsigned long)(seconds/60U%60U),(unsigned long)(seconds%60U));}
static void MediaDuration(char *d,uint32_t cap,uint64_t ms)
{if(ms>=3600000U)Duration(d,cap,ms);else snprintf(d,cap,"%lu:%02lu",(unsigned long)(ms/60000U),(unsigned long)(ms/1000U%60U));}
static void Decimal(char *d,uint32_t cap,uint64_t tenths)
{if(tenths>99999999ULL)Copy(d,cap,"--");else snprintf(d,cap,"%lu.%lu",(unsigned long)((uint32_t)tenths/10U),(unsigned long)((uint32_t)tenths%10U));}

/* One trip at a time provides legible hierarchy within336x271. A/B/today/fill
 * submodes share the same global section transition. Average includes stops. */
static void Trip(const UiState *s,const DashboardPageFacts *f,DashboardPage *p)
{
    uint32_t n=s->dashboard.selection%TRIP_COUNT;
    static const char *const names[]={"TRIP A","TRIP B","TODAY","SINCE REFUEL"};
    Copy(p->title,sizeof(p->title),names[n]);p->key+=n<<8;p->selection=n;
    const TripRecord *r=f->trips?&f->trips->records[n]:0;
    /* Fixed Moving/Stopped captions are owned by the renderer. Keep raw
     * stopped/total times and quality flags in TripRecord, not unused text. */
    Copy(p->lines[2],64,f->units?"mph":"km/h");Copy(p->lines[3],64,f->units?"mph":"km/h");
    Copy(p->lines[4],64,f->units?"mi":"km");
    if(!r||!r->valid||(n==TRIP_TODAY&&!f->date)){for(uint32_t i=0;i<6;++i)Copy(p->numbers[i],16,"--");return;}
    p->known=1;uint64_t total=r->moving_ms+r->stopped_ms;
    uint64_t minutes=r->moving_ms/60000U;
    if(minutes/60U>9999U)Copy(p->numbers[0],16,"--:--");
    else snprintf(p->numbers[0],16,"%lu:%02lu",(unsigned long)(minutes/60U),(unsigned long)(minutes%60U));
    uint64_t unit=f->units?1609344ULL:1000000ULL;
    snprintf(p->numbers[2],16,"%lu",(unsigned long)(((uint64_t)r->max_kph*1000000ULL)/unit));
    uint64_t average=total?(r->distance_mm*36000000ULL/total)/unit:0U;
    if(average>9999U)Copy(p->numbers[3],16,"---.-");else Decimal(p->numbers[3],16,average);
    uint64_t distance=r->distance_mm*10U/unit;
    if(distance>999999U)Copy(p->numbers[4],16,"-----.-");else Decimal(p->numbers[4],16,distance);
    p->ratio_permille=total?(uint32_t)(r->moving_ms*1000U/total):1001U; /* No ratio for0/0. */
}

/* Notifications are newest-first for each phone. Unknown/stale values remain
 * unknown, not zero battery/zero notifications. >50km/h fixes selection at0. */
static void Phone(const UiState *s,const DashboardPageFacts *f,DashboardPage *p)
{
 const PhoneContentSlot *v=f->phone;
 if(!v||!v->connected){Copy(p->lines[0],64,"No phone connected");return;}
 uint32_t fresh=v->status_revision&&(uint32_t)(f->now_ms-v->status_ms)<=120000U;
 if(!fresh)return;
 p->known=1;p->visual_key=v->status.header_key;
 if(s->notification_reply){
  p->visual_key=v->status.reply_key;p->reply_count=s->reply_count+1;
  p->reply_selection=s->reply_selection;p->key^=0x70000000U;return;
 }
 
 uint32_t locked=!s->speed_valid||s->speed_kph>50U;
 uint32_t n=locked?0:s->dashboard.selection;
 if(n>=v->status.count)n=0;
 if(!v->status.count){Copy(p->lines[0],64,"No notification");return;}
 const PhoneNotification *q=&v->status.notifications[n];
 p->visual_key=q->visual_key;p->key^=q->id*65537U+q->revision*17U;
 snprintf(p->note,sizeof(p->note),"%lu / %lu",(unsigned long)(n+1),(unsigned long)v->status.count);
}

/* Metadata and a bounded RGB565 thumbnail are supplied by the companion.
 * Position extrapolation is capped at the track duration and freshness limit. */
static void Music(const DashboardPageFacts *f,DashboardPage *p)
{
    const PhoneContentSlot *v=f->phone;Copy(p->title,sizeof(p->title),"MUSIC");
    if(!v||!v->connected){Copy(p->lines[0],64,"No phone connected");return;}
    if(!v->music_revision||!v->music.valid||(uint32_t)(f->now_ms-v->music_ms)>15000U){Copy(p->lines[0],64,"Waiting for media");return;}
    Copy(p->lines[0],64,v->music.title);Copy(p->lines[1],64,v->music.artist);
    Copy(p->lines[2],64,v->music.playing?"Playing":"Paused");
    uint64_t pos=v->music.position_ms;
    if(v->music.playing)pos+=(uint32_t)(f->now_ms-v->music_ms);
    if(v->music.duration_ms&&pos>v->music.duration_ms)pos=v->music.duration_ms;
    MediaDuration(p->numbers[0],16,pos);MediaDuration(p->numbers[1],16,v->music.duration_ms);
    p->visual_key=v->music.visual_key;p->key^=v->music.visual_key*65537U;p->known=1;p->ratio_permille=v->music.duration_ms?(uint32_t)(pos*1000U/v->music.duration_ms):0U;
    p->art_valid=!!v->music.art_valid;p->art=v->music.art_rgb565;p->art_revision=v->music_revision^(v->token*65537U);
}

void DashboardPages_Present(const UiState *s,const UiDashboardPresentation *shell,const DashboardPageFacts *f,DashboardPage *p)
{
    if(!s||!shell||!f||!p)return;
    memset(p,0,sizeof(*p));p->kind=s->dashboard.card;p->key=p->kind;p->selection=s->dashboard.selection;p->direction=s->dashboard.item_direction;
    /* All parent menus use this renderer and animation too. Priority warning
     * text cannot leak outside the central clipping rectangle. */
    if(s->menu||s->modal||s->warning||s->power==UI_FAULT){
        p->kind=UI_CARD_COUNT;p->key=0x10000U+(s->menu<<8)+(s->modal<<16)+(s->warning<<20)+s->selection;
        Copy(p->title,32,shell->title);Copy(p->lines[0],64,shell->line);Copy(p->note,40,shell->hint);Copy(p->numbers[0],16,shell->number);return;
    }
    switch(p->kind){
    case UI_BLANK:UiHome_Present(s,f,p);break;
    case UI_TRIP:Trip(s,f,p);break;
    case UI_NOTIFICATIONS:Phone(s,f,p);p->key+=f->phone_slot<<12;break;
    case UI_MUSIC:{
        Music(f,p);p->key+=f->phone_slot<<12;p->selection=f->phone_slot;break;
    }
    case UI_CALLS:{
        const UiCalls *c=&s->calls;const PhoneCallsSnapshot *v=&c->phone;
        if(!PhoneCalls_Fresh(v,f->now_ms)){Copy(p->lines[0],64,"Waiting for phone");break;}
        uint32_t id=PhoneCalls_Target(c);p->key^=id*65537U;p->selection=c->selected;if(v->visual_target==id)p->art_revision=v->call_type;
        p->known=1;p->ratio_permille=v->state;
        if(v->visual_target==id)p->visual_key=v->visual_key;
        if(v->state){static const char *const names[]={"","Incoming call","Calling...","In call","On hold","Ending call"};
            Copy(p->title,32,names[v->state]);MediaDuration(p->numbers[0],16,(uint64_t)v->elapsed_s*1000U);
            if(!(v->permissions&CALL_CONTROL_PERMISSION))Copy(p->note,40,"Permission Denied!");
        }else{
            Copy(p->title,32,c->selected<v->count&&v->entries[c->selected].group?"Recent calls":"Favorites");
            if(!v->count)Copy(p->lines[0],64,(v->permissions&3U)?"No contacts yet":"Permission Denied!");
            else {snprintf(p->note,40,"%lu / %lu",(unsigned long)c->selected+1,(unsigned long)v->count);
                }
        }
        break;
    }
    case UI_PHONE_GPS:
        p->plot_count=PhoneTrail_ProjectView(f->trail,p->plot,GPS_PLOT_WIDTH,GPS_PLOT_HEIGHT,s->dashboard.gps_heading_up,s->dashboard.gps_zoom);
        p->grid_count=PhoneTrail_ProjectGrid(f->trail,p->grid,GPS_PLOT_WIDTH,GPS_PLOT_HEIGHT,s->dashboard.gps_heading_up,s->dashboard.gps_zoom);
        p->heading_east=s->dashboard.gps_heading_up?0:f->trail?f->trail->display_east:0;
        p->heading_north=s->dashboard.gps_heading_up?1024:f->trail?f->trail->display_north:1024;
        for(uint32_t i=0;f->trail&&i<f->trail->count;i++)if(f->trail->points[i].gap)p->plot_gaps|=1ULL<<i;
        p->map_scale=PhoneTrail_Scale(s->dashboard.gps_zoom);
        snprintf(p->numbers[0],16,"%lu m",(unsigned long)p->map_scale);
        Copy(p->note,40,f->trail&&f->trail->live?"":"Waiting for phone GPS");break;
    case UI_SYSTEM:
        Copy(p->title,32,"SETTINGS");p->grey=!s->speed_valid||s->speed_kph>=3U;
        Copy(p->lines[0],64,"Hold ENTER to open");Copy(p->note,40,p->grey?"Available below 3 km/h":"Date / Phone / Vehicle");break;
    default:break;
    }
    if((p->kind==UI_TRIP&&(f->development_mask&2U))||
       ((p->kind==UI_NOTIFICATIONS||p->kind==UI_MUSIC||p->kind==UI_PHONE_GPS)&&(f->development_mask&1U)))
        Copy(p->note,sizeof(p->note),"TEST DATA / not saved");

}
