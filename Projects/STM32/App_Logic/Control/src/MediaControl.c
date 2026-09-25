#include "MediaControl.h"
#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "Media scalar codec requires little-endian target"
#endif
#include "NoodoeBluetooth.h"
#include "NDCP.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
typedef struct {uint32_t state,action,sequence,started;Bluetooth_LinkState link;uint32_t opcode,length;uint8_t payload[20];} Request;
static Request requests[BLUETOOTH_PHONE_CAPACITY];
static uint32_t sequence;
volatile MediaControlDiagnostics g_media_control;
static uint32_t Same(const Bluetooth_LinkState *a,const Bluetooth_LinkState *b)
{return a->status==BLUETOOTH_LINK_UP&&b->status==BLUETOOTH_LINK_UP&&a->cid==b->cid&&a->reconnects==b->reconnects&&!memcmp(a->address,b->address,6);}
/* One outstanding request per phone bounds memory and prevents rapid repeat
 * toggles. Critical sections copy only fixed structs; no transport/I/O inside. */
static uint32_t Queue(uint32_t slot,uint32_t action,uint32_t now,uint32_t opcode,const void *payload,uint32_t length)
{
    if(slot>=BLUETOOTH_PHONE_CAPACITY||action>=3)return 1;
    Bluetooth_LinkState link;
    if(Bluetooth_GetLinkState(Bluetooth_PhoneRole(slot),&link)!=BLUETOOTH_OK||link.status!=BLUETOOTH_LINK_UP)return 2;
    taskENTER_CRITICAL();++g_media_control.requests;
    if(requests[slot].state){taskEXIT_CRITICAL();return 3;}
    uint32_t id=(++sequence)|0x80000000U;
    requests[slot]=(Request){.state=1,.action=action,.sequence=id,.started=now,.link=link,.opcode=opcode,.length=length};
    memcpy(requests[slot].payload,payload,length);
    ++g_media_control.accepted;g_media_control.last_slot=slot;g_media_control.last_action=action;
    g_media_control.last_sequence=id;g_media_control.last_result=0xFFFFFFFFU;
    taskEXIT_CRITICAL();return 0;
}
uint32_t MediaControl_Request(uint32_t slot,uint32_t action,uint32_t now)
{uint8_t p[4]={(uint8_t)action,0,0,0};return Queue(slot,action,now,MEDIA_CONTROL_OPCODE,p,4);}
uint32_t MediaControl_Reply(const uint32_t words[5],uint32_t now)
{return Queue(0,0,now,PHONE_REPLY_OPCODE,words,20);}
uint32_t MediaControl_Call(const uint32_t words[5],uint32_t now)
{return Queue(0,0,now,PHONE_CALL_OPCODE,words,20);}
/* I/O owner calls before draining requests. Never automatically retransmit
 * play/pause after timeout: a lost ACK must not toggle a second time. */
void MediaControl_Poll(uint32_t now)
{
    for(uint32_t slot=0;slot<BLUETOOTH_PHONE_CAPACITY;++slot){Bluetooth_LinkState link;
        uint32_t valid=Bluetooth_GetLinkState(Bluetooth_PhoneRole(slot),&link)==BLUETOOTH_OK;
        taskENTER_CRITICAL();Request *r=&requests[slot];
        if(r->state&&(!valid||!Same(&r->link,&link)||now-r->started>=3000U)){
            r->state=0;++g_media_control.failed;g_media_control.last_result=valid&&Same(&r->link,&link)?7:2;
        }taskEXIT_CRITICAL();
    }
}
/* Called only when Control's existing frame serializer is idle. All four
 * payload bytes are action LE32:0 toggle,1 previous,2 next. No separate BT
 * writer can splice a command into an update or protocol reply frame. */
uint32_t MediaControl_Take(uint32_t slot,uint8_t *out,uint32_t capacity)
{
    if(slot>=BLUETOOTH_PHONE_CAPACITY||!out)return 0;
    Request copy;taskENTER_CRITICAL();copy=requests[slot];taskEXIT_CRITICAL();
    if(copy.state!=1)return 0;
    Bluetooth_LinkState current;
    if(Bluetooth_GetLinkState(Bluetooth_PhoneRole(slot),&current)!=BLUETOOTH_OK||!Same(&copy.link,&current))return 0;
    uint32_t size=NDCP_Encode(out,capacity,copy.opcode,0,copy.sequence,copy.payload,copy.length);
    if(size){taskENTER_CRITICAL();requests[slot].state=2;taskEXIT_CRITICAL();}return size;
}
/* Acknowledgements must match both sequence and the same connected peer.
 * Result0 is the companion's report, not proof of a media service response. */
void MediaControl_Acknowledge(uint32_t slot,uint32_t id,uint32_t result)
{
    if(slot>=BLUETOOTH_PHONE_CAPACITY)return;
    Bluetooth_LinkState link;uint32_t valid=Bluetooth_GetLinkState(Bluetooth_PhoneRole(slot),&link)==BLUETOOTH_OK;
    taskENTER_CRITICAL();Request *r=&requests[slot];
    if(valid&&r->state==2&&r->sequence==id&&Same(&r->link,&link)){
        r->state=0;++g_media_control.acknowledged;if(result)++g_media_control.failed;g_media_control.last_result=result;
    }taskEXIT_CRITICAL();
}
/* A complete24-byte media frame is queued atomically. If transport was full
 * until its deadline, discard the still-unsent frame instead of a late toggle.
 * Bytes already accepted by Bluetooth cannot be recalled by this predicate. */
uint32_t MediaControl_IsPending(uint32_t slot,uint32_t id)
{
    if(slot>=BLUETOOTH_PHONE_CAPACITY)return 0;
    taskENTER_CRITICAL();uint32_t pending=requests[slot].state==2&&requests[slot].sequence==id;taskEXIT_CRITICAL();return pending;
}
