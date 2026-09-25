#include "Development_Data.h"
#include "Graphics_Viewport.h"
#include <string.h>
#include <stdio.h>
volatile DevelopmentMailbox g_product_data;
static DevelopmentSample sample;
static uint32_t seen,revision,override_active;
static uint32_t media_slot;
#if DATA_DEBUG
typedef struct {uint32_t revision,track,playing,position,stamp;} FixturePlayer;
static FixturePlayer players[PHONE_CONTENT_SLOTS];
/* Runtime clock advances a fixture player independently for each phone. A
 * new fixture revision resets it; swapping phones never resets the other. */
static FixturePlayer *Player(uint32_t slot,uint32_t now)
{
    FixturePlayer *p=&players[slot];
    if(p->revision!=revision)*p=(FixturePlayer){revision,0,0,sample.position_ms,now};
    if(p->playing){uint32_t delta=now-p->stamp,left=sample.duration_ms-p->position;
        p->position+=delta<left?delta:left;}
    p->stamp=now;return p;
}
#endif
void DevelopmentData_SelectPhone(uint32_t slot){if(slot<PHONE_CONTENT_SLOTS)media_slot=slot;}
uint32_t DevelopmentData_MediaAction(uint32_t slot,uint32_t action,uint32_t now)
{
#if DATA_DEBUG
    if(slot>=PHONE_CONTENT_SLOTS||action>2||!(g_product_data.active_mask&DEVELOPMENT_REMOTE))return 0;
    FixturePlayer *p=Player(slot,now);
    if(!action)p->playing^=1U;
    else{p->track=(p->track+(action==1?2U:1U))%3U;p->position=0;}
    return 1;
#else
    (void)slot;(void)action;(void)now;return 0;
#endif
}

/* Bounded ASCII sanitization matches the current product font. This producer
 * is not a BT decoder; it cannot publish to PhoneContent or Runtime caches. */
#if DATA_DEBUG
static void Text(char *d,uint32_t cap,const char *s)
{
    uint32_t i=0;for(;i+1<cap&&s[i];++i){unsigned char c=s[i];d[i]=(c>=32&&c<=126)?(char)c:'?';}d[i]=0;
}
#endif
void DevelopmentData_Default(DevelopmentSample *v)
{
    if(!v)return;
    memset(v,0,sizeof(*v));v->mask=DEVELOPMENT_REMOTE; /* UART-derived trips stay live; explicit fixtures may opt in. */
    for(uint32_t i=0;i<4;++i){v->moving_minutes[i]=83+17*i;v->stopped_minutes[i]=13+7*i;
        v->distance_tenths_km[i]=776+231*i;v->max_kph[i]=112+2*i;}
    v->battery_percent=68;v->notification_count=3;v->position_ms=83000;v->duration_ms=225000;
    v->latitude_e7=370000000;v->longitude_e7=1270000000;
    strcpy(v->notification_title,"New message");strcpy(v->notification_body,"Meet at the station.");
    strcpy(v->music_title,"Night ride");strcpy(v->music_artist,"Display test artist");
}
/* DATA_DEBUG is the default source, independent of the viewport debug color.
 * Timed SWD samples can refine it but never permanently change that choice. */
static void DefaultSource(void)
{
    override_active=0;g_product_data.expires_ms=0;
#if DATA_DEBUG
    DevelopmentData_Default(&sample);if(!++revision)++revision;
    g_product_data.active_mask=sample.mask;
#else
    g_product_data.active_mask=0;
#endif
}
void DevelopmentData_Init(void)
{
    memset((void*)&g_product_data,0,sizeof(g_product_data));g_product_data.magic=0x44415431U;g_product_data.version=2;
    memset(&sample,0,sizeof(sample));seen=revision=0;
    media_slot=0;
#if DATA_DEBUG
    memset(players,0,sizeof(players));
#endif
    DefaultSource();
}
#if DATA_DEBUG && GRAPHICS_DEV_VIEWPORT
static uint32_t Valid(const DevelopmentSample *v,uint32_t ttl)
{
    if(!v||!v->mask||v->mask>3||ttl<1000||ttl>180000||v->battery_percent>100||v->notification_count>6||
       v->position_ms>v->duration_ms||v->duration_ms>86400000||
       v->latitude_e7<-890000000||v->latitude_e7>890000000||v->longitude_e7<-1790000000||v->longitude_e7>1790000000)return 0;
    for(uint32_t i=0;i<4;++i)if(v->moving_minutes[i]>599999||v->stopped_minutes[i]>599999||v->distance_tenths_km[i]>999999||v->max_kph[i]>255)return 0;
    return 1;
}
#endif
uint32_t DevelopmentData_Request(const DevelopmentSample *v,uint32_t ttl)
{
#if DATA_DEBUG && GRAPHICS_DEV_VIEWPORT
    if(g_product_data.request_id!=g_product_data.ack_id||(v&&!Valid(v,ttl)))return 0;
    uint32_t id=g_product_data.request_id+1;if(!id)id=1;
    if(v)g_product_data.sample=*v;
    g_product_data.command=v?2:3;g_product_data.ttl_ms=ttl;
    __sync_synchronize();g_product_data.request_id=id;return 1;
#else
    (void)v;(void)ttl;return 0;
#endif
}
/* Build caller-owned dev-domain snapshots for this UI tick only. Dev time and
 * trips are fixture values, not accumulated UART history. Live TripComputer
 * keeps running independently. No fake connection is sent into UiState. */
#if DATA_DEBUG
static void BuildScratch(DevelopmentScratch *work,uint32_t now)
{
    TripComputer_Init(&work->trips);
    for(uint32_t i=0;i<4;++i){TripRecord *r=&work->trips.records[i];r->valid=1;
        r->moving_ms=(uint64_t)sample.moving_minutes[i]*60000;
        r->stopped_ms=(uint64_t)sample.stopped_minutes[i]*60000;
        r->distance_mm=(uint64_t)sample.distance_tenths_km[i]*100000;r->max_kph=sample.max_kph[i];}
    PhoneTrail_Init(&work->trail);
    for(uint32_t i=0;i<6;++i)PhoneTrail_Feed(&work->trail,now+i*1000,1,sample.latitude_e7+(int32_t)i*1700,sample.longitude_e7+(int32_t)(i*i)*300);
}
#endif
void DevelopmentData_Poll(uint32_t now)
{
    g_product_data.now_ms=now;
#if DATA_DEBUG && GRAPHICS_DEV_VIEWPORT
    uint32_t id=g_product_data.request_id;
    if(id&&id!=seen){
        uint32_t command=g_product_data.command,ttl=g_product_data.ttl_ms;
        DevelopmentSample next=g_product_data.sample;
        __sync_synchronize();if(id!=g_product_data.request_id)return;
        seen=id;g_product_data.result=0;
        if(command==3)DefaultSource();
        else{
            if(command==1)DevelopmentData_Default(&next);
            if((command!=1&&command!=2)||!Valid(&next,ttl)){g_product_data.result=2;DefaultSource();}
            else{sample=next;if(!++revision)++revision;g_product_data.active_mask=sample.mask;g_product_data.expires_ms=now+ttl;override_active=1;}
        }
        __sync_synchronize();g_product_data.ack_id=id;
    }
    if(override_active&&(int32_t)(now-g_product_data.expires_ms)>=0)DefaultSource();
#else
    /* Disallowed mailbox writes cannot activate a fake source in live mode. */
    if(g_product_data.request_id!=g_product_data.ack_id){g_product_data.result=2;g_product_data.ack_id=g_product_data.request_id;}
#if !DATA_DEBUG
    g_product_data.active_mask=0;
#endif
#endif
}
void DevelopmentData_Resolve(DashboardPageFacts *f,PhoneContentSlot *out,DevelopmentScratch *work,uint32_t now)
{
    if(!f||!out||!work)return;
#if DATA_DEBUG
    uint32_t mask=g_product_data.active_mask;f->development_mask=mask;
    if(!mask)return;
    BuildScratch(work,now);
    if(mask&DEVELOPMENT_TRIPS)f->trips=&work->trips;
    if(!(mask&DEVELOPMENT_REMOTE))return;
    memset(out,0,sizeof(*out));out->connected=1;out->token=0x80000000U|revision;
    out->status_revision=out->music_revision=revision;out->status_ms=out->music_ms=now;
    out->status.battery_valid=out->status.notifications_valid=1;out->status.battery_percent=sample.battery_percent;
    out->status.count=sample.notification_count;
    for(uint32_t i=0;i<sample.notification_count;++i){PhoneNotification *n=&out->status.notifications[i];
        n->id=i+1;strcpy(n->app,"Messages");Text(n->title,sizeof(n->title),sample.notification_title);Text(n->body,sizeof(n->body),sample.notification_body);
        if(i)snprintf(n->title,sizeof(n->title),"Earlier message %lu",(unsigned long)(i+1));}
    FixturePlayer *player=Player(media_slot,now);
    out->music.valid=1;out->music.playing=player->playing;out->music.position_ms=player->position;out->music.duration_ms=sample.duration_ms;
    Text(out->music.title,sizeof(out->music.title),sample.music_title);Text(out->music.artist,sizeof(out->music.artist),sample.music_artist);
    if(player->track)Text(out->music.title,sizeof(out->music.title),player->track==1?"City lights":"Open road");
    /* The development producer supplies real RGB565 pixels too. Renderer no
     * longer recognizes magic fixture art_valid=2 or fabricates media content. */
    out->music.art_valid=1;
    for(uint32_t y=0;y<32;++y)for(uint32_t x=0;x<32;++x){uint16_t rgb=(uint16_t)((x<<11)|(y*2U<<5)|((x+y)/2U));uint32_t i=(y*32+x)*2;
        out->music.art_rgb565[i]=(uint8_t)rgb;out->music.art_rgb565[i+1]=(uint8_t)(rgb>>8);}
    f->phone=out;f->phone_slot=media_slot;f->trail=&work->trail;
#else
    (void)now;f->development_mask=0;
#endif
}
