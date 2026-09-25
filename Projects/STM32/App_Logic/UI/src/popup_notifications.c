#include "Popup_Notifications.h"
#include <string.h>
volatile PopupNotificationDiagnostics g_popup_notifications;
static uint32_t started_ms,was_trip;

/* Bounded validation/copy: current Lato subset is printable ASCII. Refuse
 * unsupported glyphs, empty strings and an unterminated64-byte input; never
 * silently accept a message whose bytes the current font cannot display. */
static uint32_t CopyMessage(char *to,const char *from)
{
    if(!from)return 0;
    for(uint32_t i=0;i<TOAST_MESSAGE_CAPACITY;++i){uint8_t c=(uint8_t)from[i];
        if(!c){to[i]=0;return i!=0;}
        if(c<32||c>126||i==TOAST_MESSAGE_CAPACITY-1U)return 0;
        to[i]=(char)c;
    }return 0;
}
void PopupNotifications_Init(void)
{
    memset((void*)&g_popup_notifications,0,sizeof(g_popup_notifications));
    g_popup_notifications.magic=0x544F4131U;g_popup_notifications.version=1;
    started_ms=was_trip=0;
}
uint32_t PopupNotifications_Request(const char *message,uint32_t seconds)
{
    if(g_popup_notifications.request_id!=g_popup_notifications.ack_id)return 0;
    if(message&&(seconds<1||seconds>TOAST_MAX_SECONDS||!CopyMessage((char*)g_popup_notifications.request_text,message)))return 0;
    g_popup_notifications.command=message?1U:2U;g_popup_notifications.seconds=seconds;
    g_popup_notifications.request_id++;return 1;
}
/* Accepted messages own their text/timing. The timer starts at UI consumption,
 * not when a producer queued it. uint32 subtraction handles tick wrap, and
 * seconds validation prevents multiplication overflow or indefinite overlays. */
static void Start(const char *text,uint32_t seconds,uint32_t now,uint32_t source)
{
    (void)CopyMessage((char*)g_popup_notifications.text,text);
    started_ms=now;g_popup_notifications.duration_ms=seconds*1000U;
    g_popup_notifications.elapsed_ms=0;g_popup_notifications.visible=1;
    g_popup_notifications.source=source;g_popup_notifications.shown_count++;
}
void PopupNotifications_Process(uint32_t now,uint32_t trip)
{
    ++g_popup_notifications.seq;g_popup_notifications.now_ms=now;
    uint32_t request=g_popup_notifications.request_id,explicit_hide=0;
    if(request!=g_popup_notifications.ack_id){
        uint32_t command=g_popup_notifications.command,seconds=g_popup_notifications.seconds;
        char text[TOAST_MESSAGE_CAPACITY];g_popup_notifications.result=0;
        if(command==2){g_popup_notifications.visible=0;explicit_hide=1;}
        else if(command==1&&seconds>=1&&seconds<=TOAST_MAX_SECONDS&&CopyMessage(text,(const char*)g_popup_notifications.request_text))Start(text,seconds,now,2);
        else g_popup_notifications.result=1;
        g_popup_notifications.ack_id=request;
    }
    if(g_popup_notifications.visible){
        g_popup_notifications.elapsed_ms=now-started_ms;
        if(g_popup_notifications.elapsed_ms>=g_popup_notifications.duration_ms)g_popup_notifications.visible=0;
    }
    /* Do not restart on each frame or A/B selection. Existing caller toasts
     * take precedence. TODAY/REFUEL and modal contexts are not manually reset. */
    if(trip&&!was_trip&&!explicit_hide&&!g_popup_notifications.visible)Start("Hold O to reset",3,now,1);
    if(!trip&&g_popup_notifications.source==1)g_popup_notifications.visible=0;
    was_trip=!!trip;++g_popup_notifications.seq;
}
uint32_t PopupNotifications_Get(PopupNotificationSnapshot *s)
{
    if(!s)return 0;
    uint32_t seq=g_popup_notifications.seq;if(seq&1U)return 0;
    s->visible=g_popup_notifications.visible;s->elapsed_ms=g_popup_notifications.elapsed_ms;
    s->duration_ms=g_popup_notifications.duration_ms;s->revision=g_popup_notifications.shown_count;
    for(uint32_t i=0;i<TOAST_MESSAGE_CAPACITY;++i)s->text[i]=g_popup_notifications.text[i];
    return seq==g_popup_notifications.seq;
}
