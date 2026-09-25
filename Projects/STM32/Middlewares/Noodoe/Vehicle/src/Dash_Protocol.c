#include "Dash_Protocol.h"
#include <string.h>

uint32_t DashProtocol_Init(Dash_Protocol *p,const uint32_t thresholds[10],uint32_t now)
{
    if(!p || !thresholds || thresholds[9]!=0U || thresholds[0]==UINT32_MAX)return 0U;
    for(uint32_t i=1;i<10U;++i)if(thresholds[i]>=thresholds[i-1U])return 0U;
    memset(p,0,sizeof(*p));memcpy(p->thresholds,thresholds,sizeof(p->thresholds));
    DashProtocol_Start(p,now);return 1U;
}

/* No bytes from an earlier link epoch may survive a start/stop/reconnect. */
void DashProtocol_Start(Dash_Protocol *p,uint32_t now)
{
    p->phase=DASH_PHASE_STARTUP;p->phase_ms=now;p->seen=0U;p->group_count=0U;
    p->pending_request=p->pending_light=p->pending_stop=0U;
}
void DashProtocol_Stop(Dash_Protocol *p)
{
    p->phase=DASH_PHASE_OFF;p->seen=0U;p->group_count=0U;
    p->pending_light=p->pending_request=0U;p->pending_stop=1U;
}
void DashProtocol_Reconnect(Dash_Protocol *p)
{
    p->seen=0U;p->group_count=0U;p->pending_light=0U;
    p->pending_stop=0U;p->pending_request=1U;
}

/* The original counts valid dispatches only while its timeout is400. Data in
 * STARTUP/GAP still belongs to the vehicle parser, but does not cancel retry. */
void DashProtocol_OnFrames(Dash_Protocol *p,uint32_t frames,uint32_t now)
{
    if(!frames || p->phase!=DASH_PHASE_ACTIVE || p->pending_request)return;
    p->seen=1U;p->phase_ms=now;
    uint32_t replies=frames/4U,rest=frames%4U+p->group_count;
    replies+=rest/4U;p->group_count=rest%4U;
    uint32_t room=DASH_REPLY_QUEUE_MAX-p->pending_light;
    if(replies>room){p->overflowed_replies+=replies-room;replies=room;}
    p->pending_light+=replies;
}

/* Strict '>' matches the stock compare at0x0804E88C, with unsigned wrap.
 * A400ms gap starts a fresh800ms interval instead of immediately sending01. */
void DashProtocol_Process(Dash_Protocol *p,uint32_t now)
{
    if(p->pending_request || p->pending_stop)return;
    uint32_t age=now-p->phase_ms;
    if(p->phase==DASH_PHASE_STARTUP && age>DASH_STARTUP_MS)DashProtocol_Reconnect(p);
    else if(p->phase==DASH_PHASE_ACTIVE && age>DASH_RECEIVE_GAP_MS){
        p->phase=DASH_PHASE_GAP;p->phase_ms=now;p->seen=0U;p->pending_light=0U;
    }else if(p->phase==DASH_PHASE_GAP && age>DASH_RETRY_MS)DashProtocol_Reconnect(p);
}

/* The whole wire frame XOR is zero; A1 transmits the index, never lux or %. */
Dash_TxKind DashProtocol_Peek(const Dash_Protocol *p,uint8_t out[6],uint32_t *length)
{
    if(!p || !out || !length)return DASH_TX_NONE;
    Dash_TxKind kind=p->pending_stop?DASH_TX_STOP:(p->pending_request?DASH_TX_REQUEST:
                     (p->pending_light?DASH_TX_LIGHT:DASH_TX_NONE));
    *length=0U;if(kind==DASH_TX_NONE)return kind;
    out[0]=0xF5U;
    if(kind==DASH_TX_LIGHT){out[1]=0xA1U;out[2]=2U;out[3]=1U;out[4]=(uint8_t)p->light_index;*length=6U;}
    else{out[1]=1U;out[2]=1U;out[3]=kind==DASH_TX_REQUEST?4U:0U;*length=5U;}
    uint8_t sum=0U;for(uint32_t i=0;i<*length-1U;++i)sum^=out[i];
    out[*length-1U]=sum;return kind;
}
void DashProtocol_Accepted(Dash_Protocol *p,Dash_TxKind kind,uint32_t now)
{
    if(kind==DASH_TX_REQUEST){p->pending_request=0U;p->phase=DASH_PHASE_ACTIVE;p->phase_ms=now;p->group_count=0U;p->seen=0U;}
    else if(kind==DASH_TX_LIGHT && p->pending_light)--p->pending_light;
    else if(kind==DASH_TX_STOP)p->pending_stop=0U;
}
uint32_t DashProtocol_LightIndex(const Dash_Protocol *p,uint32_t millilux)
{
    uint32_t lux=millilux/1000U;
    for(uint32_t i=0;i<9U;++i)if(lux>=p->thresholds[i])return i;
    return 9U;
}
