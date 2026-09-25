#include "NoodoeBluetooth.h"
#include "Bluetooth_Name.h"
#include "PowerService.h"
#include "Health_Service.h"
#include "bluetooth_port.h"
#include "bluetooth_keys.h"
#include "BSP_BT_HCI.h"
#include "BSP_BT_ResetDiagnostic.h"
#include "bsp_diagnostics_profile.h"
#include "btstack_memory.h"
#include "btstack_event.h"
#include "btstack_run_loop.h"
#include "hal_time_ms.h"
#include "btstack_util.h"
#include "bluetooth_sdp.h"
#include "hci.h"
#include "hci_transport_h4.h"
#include "gap.h"
#include "l2cap.h"
#include "classic/rfcomm.h"
#include "classic/sdp_server.h"
#include "classic/sdp_util.h"
#include "classic/spp_server.h"
#include "btstack_run_loop_freertos.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <string.h>
static char local_name[]="FuckNudo CFW 000000";

#ifndef BLUETOOTH_MAIN_BAUD
#define BLUETOOTH_MAIN_BAUD 921600U
#endif
#define BT_STACK_WORDS 2048U
#define BT_REQUEST_COUNT 8U
#define BT_TASK_PRIORITY 32U
#if configMAX_PRIORITIES <= BT_TASK_PRIORITY
#error "Bluetooth must run above the CMSIS Normal UI priority24"
#endif

typedef struct {
    uint8_t rx[BLUETOOTH_RX_CAPACITY],tx[BLUETOOTH_TX_CAPACITY];
    uint16_t rx_head,rx_tail,rx_count,tx_head,tx_tail,tx_count;
    uint8_t credit,send_requested;
    uint32_t deadline,cancel_deadline,cancel_reason;
    uint8_t cancelling,cancel_sent;
} Link;
typedef struct {
    uint8_t operation,role,channel,reserved;
    uint8_t address[6]; uint32_t value;
} Request;
enum { OP_START=1,OP_STOP,OP_DISCOVER,OP_CONNECT,OP_DISCONNECT,OP_PAIR,OP_WINDOW,OP_FORGET,OP_RAW_RESET };
/* Publish headers at service startup, keeping zero payload out of flash. */
volatile Bluetooth_Diagnostics g_bluetooth;
volatile Bluetooth_ControlMailbox g_bluetooth_control={.magic=0x42435431U,.version=1U};
_Static_assert(sizeof(Bluetooth_ControlMailbox)==48U,"Bluetooth control mailbox ABI");
static Link links[BLUETOOTH_ROLE_COUNT];
static QueueHandle_t requests;
static StaticQueue_t queue_object;
static uint8_t queue_buffer[BT_REQUEST_COUNT*sizeof(Request)];
static TaskHandle_t owner;
static StaticTask_t task_object;
static StackType_t task_stack[BT_STACK_WORDS];
static btstack_timer_source_t timer;
static btstack_packet_callback_registration_t event_registration;
static uint8_t service_record[160],send_buffer[512];
static uint32_t pairing_until,start_tick,pairing_seconds;
static volatile uint32_t fault_pending;
static volatile uint32_t raw_request_active;
static hci_transport_config_uart_t uart_config={HCI_TRANSPORT_CONFIG_UART,115200U,BLUETOOTH_MAIN_BAUD,1,NULL,0};
static void Packet(uint8_t type,uint16_t channel,uint8_t *packet,uint16_t size);

/* The port's clock is available before the BT owner/run loop exists. Start()
 * must register health before scheduling that owner; calling the run-loop
 * virtual method here dereferences its uninitialized instance on first boot.
 * hal_time_ms() is the same HAL clock used by the run-loop port and health
 * supervisor, so startup and steady-state deadlines keep one timebase. */
static uint32_t Now(void) { return hal_time_ms(); }
static int Due(uint32_t now,uint32_t deadline) { return (int32_t)(now-deadline)>=0; }
static int ValidRole(Bluetooth_Role role) { return (unsigned)role<BLUETOOTH_ROLE_COUNT; }
int Bluetooth_LinkSecure(Bluetooth_Role role,const Bluetooth_LinkState *expected)
{
    if(!ValidRole(role)||!expected)return 0;
    int secure=0;taskENTER_CRITICAL();
    const volatile Bluetooth_LinkState *link=&g_bluetooth.links[role];
    if(link->status==BLUETOOTH_LINK_UP&&link->cid==expected->cid&&
       link->reconnects==expected->reconnects&&!memcmp((const void*)link->address,expected->address,6)){
        hci_connection_t *connection=hci_connection_for_bd_addr_and_type(expected->address,BD_ADDR_TYPE_ACL);
        if(connection)secure=gap_security_level(connection->con_handle)>=LEVEL_2;
    }
    taskEXIT_CRITICAL();return secure;
}
/* Owner-only publication; callers/SWD never perform radio or register work.
 * Each accepted sequence can complete once. A rejected later request leaves
 * the active operation ID intact, so it cannot steal a late completion. */
static void ControlBegin(void){g_bluetooth_control.result_sequence++;__DMB();}
static void ControlEnd(void){__DMB();g_bluetooth_control.result_sequence++;}
static void FinishStart(int32_t result)
{
    if(g_bluetooth_control.phase!=1U && g_bluetooth_control.phase!=2U)return;
    ControlBegin();
    g_bluetooth_control.completion_sequence=g_bluetooth_control.operation_id;
    g_bluetooth_control.completion_result=result;
    g_bluetooth_control.controller_state=g_bluetooth.state;
    g_bluetooth_control.phase=result?4U:3U;
    ControlEnd();
}
static int StartAllowed(void)
{
    return (g_bluetooth.state==BLUETOOTH_STATE_OFF || g_bluetooth.state==BLUETOOTH_STATE_FAULT)
        && hci_get_state()==HCI_STATE_OFF && !BSP_BT_HCI_IsQuarantined();
}
static int RawAllowed(void)
{ return StartAllowed() && Bluetooth_TransportIsDetached(); }
static void PollControl(void)
{
    uint32_t sequence=g_bluetooth_control.request_sequence;__DMB();
    if((int32_t)(sequence-g_bluetooth_control.ack_sequence)<=0)return;
    uint32_t command=g_bluetooth_control.command,version=g_bluetooth_control.version;
    __DMB();if(sequence!=g_bluetooth_control.request_sequence)return;
    int result=BLUETOOTH_INVALID;
    if(sequence && version==1U && (command==1U || (NOODOE_DEEP_DIAGNOSTICS&&command==2U))){
        if(g_bluetooth_control.phase==1U || g_bluetooth_control.phase==2U)result=BLUETOOTH_BUSY;
        else if(!StartAllowed() || (command==2U && !RawAllowed()))result=BLUETOOTH_NOT_READY;
        else {Request request={.operation=command==1U?OP_START:OP_RAW_RESET,.value=sequence};
            result=xQueueSend(requests,&request,0)==pdTRUE?0:BLUETOOTH_QUEUE_FULL;}
    }
    ControlBegin();
    if(!result){g_bluetooth_control.operation_id=sequence;g_bluetooth_control.phase=1U;
        raw_request_active=command==2U;}
    g_bluetooth_control.accept_result=result;g_bluetooth_control.ack_sequence=sequence;
    g_bluetooth_control.controller_state=g_bluetooth.state;ControlEnd();
}
static int ByCID(uint16_t cid)
{
    if (!cid) return -1;
    for (unsigned i=0;i<BLUETOOTH_ROLE_COUNT;i++) if (g_bluetooth.links[i].cid==cid) return (int)i;
    return -1;
}
static int ByAddress(const uint8_t address[6])
{
    for (unsigned i=0;i<BLUETOOTH_ROLE_COUNT;i++)
        if (!memcmp((const void *)g_bluetooth.links[i].address,address,6U)
            && (g_bluetooth.links[i].cid || links[i].cancelling)) return (int)i;
    return -1;
}
/* Clear pending bytes at every connection boundary. A new remote session may
 * not consume commands/responses buffered for a previous session. */
static void ClearBuffers(unsigned role)
{
    taskENTER_CRITICAL();
    links[role].rx_head=links[role].rx_tail=links[role].rx_count=0U;
    links[role].tx_head=links[role].tx_tail=links[role].tx_count=0U;
    links[role].credit=0U; links[role].send_requested=0U;
    g_bluetooth.links[role].rx_queued=0U; g_bluetooth.links[role].tx_queued=0U;
    taskEXIT_CRITICAL();
}
static void Retry(unsigned role,uint32_t error)
{
    if(links[role].cancelling && !error)error=links[role].cancel_reason;
    links[role].cancelling=0U;links[role].cancel_sent=0U;links[role].deadline=0U;
    g_bluetooth.links[role].last_error=error;
    g_bluetooth.links[role].cid=0U;
    g_bluetooth.links[role].status=BLUETOOTH_LINK_OFF;
    ClearBuffers(role);
}

/* Publish a new successful session after removing the previous session's
 * queued bytes. PHONE is inbound, so its reconnects field counts successful
 * opens and is a session epoch. Outbound roles retain their attempt counter.
 * Consumers must compare the PHONE epoch as well as LINK_UP: a disconnect
 * and reconnect can both happen between two application polling intervals. */
static void Opened(unsigned role,uint16_t cid,uint16_t mtu)
{
    /* A cancelled outbound attempt or a pending PHONE disconnect must never
     * become usable when a delayed CHANNEL_OPENED arrives. Keep its identity
     * until the real close callback, so a new peer cannot inherit old traffic. */
    if(links[role].cancelling){
        (void)rfcomm_disconnect(cid);return;
    }
    ClearBuffers(role); links[role].credit=1U;
    links[role].deadline=0U;
    g_bluetooth.links[role].cid=cid; g_bluetooth.links[role].mtu=mtu;
    if (Bluetooth_IsPhoneRole(role)) g_bluetooth.links[role].reconnects++;
    g_bluetooth.links[role].last_error=0U;
    g_bluetooth.links[role].status=BLUETOOTH_LINK_UP;
}

/* All channels to a remote address belong to one configured role: Connect and
 * incoming admission reject duplicate peers. The owner's ACL disconnect can
 * therefore drain a stuck SDP/RFCOMM handshake without affecting another role.
 * Do not use gap_connect_cancel(): in this pinned stack that API is LE-only. */
static void CancelRole(unsigned role,uint32_t reason)
{
    if(links[role].cancelling)return;
    links[role].cancelling=1U;links[role].cancel_sent=0U;
    links[role].cancel_reason=reason;links[role].cancel_deadline=Now()+BLUETOOTH_CANCEL_TIMEOUT_MS;
    links[role].deadline=0U;g_bluetooth.links[role].last_error=reason;
    g_bluetooth.links[role].status=BLUETOOTH_LINK_CANCELLING;
    ClearBuffers(role);
}
static void PendingTick(unsigned role)
{
    Link *link=&links[role];
    if(!link->cancelling){
        uint8_t state=g_bluetooth.links[role].status;
        if((state==BLUETOOTH_LINK_SDP || state==BLUETOOTH_LINK_CONNECTING) && Due(Now(),link->deadline))
            CancelRole(role,BLUETOOTH_ERROR_CONNECT_TIMEOUT);
        else return;
    }
    hci_connection_t *connection=hci_connection_for_bd_addr_and_type(
        (const uint8_t*)g_bluetooth.links[role].address,BD_ADDR_TYPE_ACL);
    if(!connection && !g_bluetooth.links[role].cid){Retry(role,0U);return;}
    if(connection){
        if(connection->state==OPEN && link->cancel_sent!=1U){
            uint8_t result=gap_disconnect(connection->con_handle);
            if(result==ERROR_CODE_SUCCESS || result==ERROR_CODE_COMMAND_DISALLOWED)link->cancel_sent=1U;
        }else if(connection->state==SEND_DISCONNECT || connection->state==SENT_DISCONNECT){
            link->cancel_sent=1U;
        }else if(connection->state==SENT_CREATE_CONNECTION && !link->cancel_sent && hci_can_send_command_packet_now()){
            if(hci_send_cmd(&hci_create_connection_cancel,(const uint8_t*)g_bluetooth.links[role].address)==ERROR_CODE_SUCCESS)
                link->cancel_sent=2U;
        }
    }
    if(Due(Now(),link->cancel_deadline))Bluetooth_TransportFault(BLUETOOTH_ERROR_CANCEL_TIMEOUT);
}

/* A channel-close callback can precede the ACL disconnect completion. Retain
 * cancellation/address ownership until both protocol resources and ACL drain;
 * otherwise a new request could bind to a still-closing previous connection. */
static void ChannelEnded(unsigned role,uint32_t error)
{
    if(!links[role].cancelling){Retry(role,error);return;}
    g_bluetooth.links[role].cid=0U;ClearBuffers(role);PendingTick(role);
}

static void FailController(uint32_t reason)
{
    BSP_BT_HCI_CaptureFault(reason);
    g_bluetooth.last_error=reason;g_bluetooth.state=BLUETOOTH_STATE_FAULT;
    FinishStart((int32_t)reason);
    hci_power_control(HCI_POWER_OFF);
}

/* Consume exactly one credit per received RFCOMM frame. Receiver backpressure
 * is expressed by withholding credits, not dropping bytes or blocking ISR. */
static void ReceiveFrame(unsigned role,const uint8_t *data,uint16_t length)
{
    Link *link=&links[role];
    if (link->credit) link->credit--;
    taskENTER_CRITICAL();
    if (length>BLUETOOTH_RX_CAPACITY-link->rx_count) {
        g_bluetooth.links[role].rx_overflow++;
        taskEXIT_CRITICAL();
        (void)rfcomm_disconnect(g_bluetooth.links[role].cid); return;
    }
    for (unsigned i=0;i<length;i++) {
        link->rx[link->rx_head]=data[i]; link->rx_head=(link->rx_head+1U)%BLUETOOTH_RX_CAPACITY;
    }
    link->rx_count+=length;
    g_bluetooth.links[role].rx_bytes+=length;
    g_bluetooth.links[role].rx_queued=link->rx_count;
    taskEXIT_CRITICAL();
}

/* RFCOMM copies synchronously into its HCI outgoing buffer. Advance the TX
 * queue only on successful rfcomm_send, preserving bytes under congestion. */
static void SendFrame(unsigned role)
{
    Link *link=&links[role];
    link->send_requested=0U;
    taskENTER_CRITICAL();
    uint16_t count=link->tx_count;
    if (count>sizeof(send_buffer)) count=sizeof(send_buffer);
    if (count>g_bluetooth.links[role].mtu) count=g_bluetooth.links[role].mtu;
    uint16_t tail=link->tx_tail;
    for (unsigned i=0;i<count;i++) { send_buffer[i]=link->tx[tail]; tail=(tail+1U)%BLUETOOTH_TX_CAPACITY; }
    taskEXIT_CRITICAL();
    if (count && rfcomm_send(g_bluetooth.links[role].cid,send_buffer,count)==ERROR_CODE_SUCCESS) {
        taskENTER_CRITICAL(); link->tx_tail=tail; link->tx_count-=count;
        g_bluetooth.links[role].tx_bytes+=count; g_bluetooth.links[role].tx_queued=link->tx_count;
        taskEXIT_CRITICAL();
    }
}
static int PairAllowed(const uint8_t address[6])
{
    (void)address;
    return pairing_until && !Due(Now(),pairing_until);
}

/* One handler covers HCI identity/security and all RFCOMM roles. Ingress is
 * bounded memory copy only; parsing, NOR writes and display run elsewhere. */
static void Packet(uint8_t type,uint16_t channel,uint8_t *packet,uint16_t size)
{
    uint8_t address[6];
    int role=ByCID(channel);
    if (type==RFCOMM_DATA_PACKET) {
        if (role>=0 && g_bluetooth.links[role].status==BLUETOOTH_LINK_UP)ReceiveFrame((unsigned)role,packet,size);
        return;
    }
    if (type!=HCI_EVENT_PACKET || size<2U) return;
    switch (hci_event_packet_get_type(packet)) {
    case HCI_EVENT_COMMAND_COMPLETE:
        if (size<6U) break;
        if (packet[5]) {
            g_bluetooth.hci_errors++; g_bluetooth.last_error=packet[5];
            if (g_bluetooth.state==BLUETOOTH_STATE_STARTING) {
                /* Never advertise a controller whose baud/patch/init command
                 * returned an error. Preserve status for a measured retry. */
                FailController(packet[5]); break;
            }
        }
        if (hci_event_command_complete_get_command_opcode(packet)==HCI_OPCODE_HCI_READ_LOCAL_VERSION_INFORMATION && size>=14U) {
            uint32_t bytes=0U;
            g_bluetooth.hci_revision=little_endian_read_16(packet,7);
            g_bluetooth.manufacturer=little_endian_read_16(packet,10);
            g_bluetooth.lmp_subversion=little_endian_read_16(packet,12);
            if (packet[5] || Bluetooth_SelectPatch((uint16_t)g_bluetooth.manufacturer,(uint16_t)g_bluetooth.lmp_subversion,&bytes)) {
                /* Stop during this event, before hci_run can send any vendor
                 * baud/patch command to an unknown ROM. */
                FailController(0x301U); break;
            }
            g_bluetooth.patch_bytes=bytes;
            hci_set_chipset(Bluetooth_ChipsetInstance());
        }
        break;
    case BTSTACK_EVENT_STATE:
        if (btstack_event_state_get_state(packet)==HCI_STATE_WORKING) {
            if(g_bluetooth.state!=BLUETOOTH_STATE_STARTING)break;
            g_bluetooth.state=BLUETOOTH_STATE_READY;
            pairing_until=pairing_seconds?Now()+pairing_seconds*1000U:0;
            gap_discoverable_control(pairing_seconds!=0);
            pairing_seconds=0;
            gap_local_bd_addr(address); memcpy((void *)g_bluetooth.local_address,address,6U);
            Bluetooth_NameSuffix(local_name,address);gap_set_local_name(local_name);
            g_bluetooth.baud=BSP_BT_HCI_GetDiagnostics()->baud;
            FinishStart(0);
        } else if (btstack_event_state_get_state(packet)==HCI_STATE_OFF) {
            /* HCI ignores transport Close's return. A quarantined DMA still
             * owns memory: report failed shutdown, never a successful OFF.
             * Preserve an earlier controller fault as the primary cause. */
            if(BSP_BT_HCI_IsQuarantined()){
                if(g_bluetooth.state!=BLUETOOTH_STATE_FAULT || !g_bluetooth.last_error)
                    g_bluetooth.last_error=0x105U;
                g_bluetooth.state=BLUETOOTH_STATE_FAULT;
            }else if(g_bluetooth.state!=BLUETOOTH_STATE_FAULT)g_bluetooth.state=BLUETOOTH_STATE_OFF;
            FinishStart(g_bluetooth.last_error?(int32_t)g_bluetooth.last_error:BLUETOOTH_NOT_READY);
            for (unsigned i=0;i<BLUETOOTH_ROLE_COUNT;i++) Retry(i,0U);
        }
        break;
    case HCI_EVENT_PIN_CODE_REQUEST:
        hci_event_pin_code_request_get_bd_addr(packet,address);
        gap_pin_code_negative(address); /* Phone SSP only; no ELM legacy PIN. */
        break;
    case HCI_EVENT_USER_CONFIRMATION_REQUEST:
        hci_event_user_confirmation_request_get_bd_addr(packet,address);
        if (PairAllowed(address)) gap_ssp_confirmation_response(address);
        else gap_ssp_confirmation_negative(address);
        break;
    case RFCOMM_EVENT_INCOMING_CONNECTION: {
        uint16_t cid=rfcomm_event_incoming_connection_get_rfcomm_cid(packet);
        rfcomm_event_incoming_connection_get_bd_addr(packet,address);
        /* Pending/closing occupies the only slot. Decline another phone
         * without clearing the current peer, parser or queued bytes. */
        role=-1;
        if(ByAddress(address)<0)for(unsigned slot=0;slot<BLUETOOTH_PHONE_CAPACITY;++slot){
            unsigned candidate=Bluetooth_PhoneRole(slot);
            if(!g_bluetooth.links[candidate].cid && !links[candidate].cancelling &&
               g_bluetooth.links[candidate].status==BLUETOOTH_LINK_OFF){role=(int)candidate;break;}
        }
        if(role<0){rfcomm_decline_connection(cid);break;}
        memcpy((void *)g_bluetooth.links[role].address,address,6U);
        g_bluetooth.links[role].cid=cid;
        g_bluetooth.links[role].status=BLUETOOTH_LINK_CONNECTING;
        links[role].deadline=Now()+BLUETOOTH_CONNECT_TIMEOUT_MS;
        rfcomm_accept_connection(cid);
        break;
    }
    case RFCOMM_EVENT_CHANNEL_OPENED: {
        uint16_t cid=rfcomm_event_channel_opened_get_rfcomm_cid(packet);
        rfcomm_event_channel_opened_get_bd_addr(packet,address);
        role=ByCID(cid); if (role<0) role=ByAddress(address);
        if(role>=0 && g_bluetooth.links[role].cid && g_bluetooth.links[role].cid!=cid)role=-1;
        if (role<0) { if (!rfcomm_event_channel_opened_get_status(packet)) rfcomm_disconnect(cid); break; }
        uint8_t status=rfcomm_event_channel_opened_get_status(packet);
        if (status) { ChannelEnded((unsigned)role,status); break; }
        uint16_t mtu=rfcomm_event_channel_opened_get_max_frame_size(packet);
        if (!mtu || mtu>BLUETOOTH_RX_CAPACITY) { rfcomm_disconnect(cid); break; }
        Opened((unsigned)role,cid,mtu);
        break;
    }
    case RFCOMM_EVENT_CHANNEL_CLOSED:
        role=ByCID(rfcomm_event_channel_closed_get_rfcomm_cid(packet));
        if (role>=0) ChannelEnded((unsigned)role,0U);
        break;
    case RFCOMM_EVENT_CAN_SEND_NOW:
        role=ByCID(rfcomm_event_can_send_now_get_rfcomm_cid(packet));
        if (role>=0) SendFrame((unsigned)role);
        break;
    default: break;
    }
}

/* Queue processing is serialized with protocol callbacks. Requests carry
 * copies, never caller-owned pointers which might expire before execution. */
static void ProcessRequest(const Request *r)
{
    unsigned role=r->role;
    g_bluetooth.commands++;
    /* Once accepted, a raw request excludes legacy commands already queued
     * before it as well as new submissions. They cannot start H4 and download
     * a patch immediately before/after this isolated four-byte experiment. */
    if(raw_request_active && r->operation!=OP_RAW_RESET){g_bluetooth.command_rejected++;return;}
    if(r->operation==OP_RAW_RESET){
#if NOODOE_DEEP_DIAGNOSTICS
        if(!raw_request_active || r->value!=g_bluetooth_control.operation_id || g_bluetooth_control.phase!=1U)return;
        if(!RawAllowed()){
            FinishStart(BLUETOOTH_NOT_READY);raw_request_active=0U;return;
        }
        ControlBegin();g_bluetooth_control.phase=2U;ControlEnd();
        /* No HCI power/run-loop callback is invoked here. The fixed BSP
         * transaction yields internally and always returns through cleanup. */
        int result=BSP_BT_ResetDiagnosticRun(r->value);
        if(BSP_BT_HCI_IsQuarantined()){
            g_bluetooth.state=BLUETOOTH_STATE_FAULT;g_bluetooth.last_error=0x105U;
        }
        FinishStart(result);raw_request_active=0U;return;
#else
        FinishStart(BLUETOOTH_UNSUPPORTED);raw_request_active=0U;return;
#endif
    }
    if (r->operation==OP_START) {
        /* Recheck at dispatch: another queued command may have changed state
         * after mailbox acceptance. Legacy Start must not supersede its token. */
        if(r->value && (g_bluetooth_control.operation_id!=r->value || g_bluetooth_control.phase!=1U))return;
        if((!r->value && g_bluetooth_control.phase==1U) || !StartAllowed()){
            if(r->value)FinishStart(BLUETOOTH_NOT_READY);
            g_bluetooth.command_rejected++;return;
        }
        if(r->value){ControlBegin();g_bluetooth_control.phase=2U;ControlEnd();}
        {
            fault_pending=0U; g_bluetooth.last_error=0U;
            g_bluetooth.state=BLUETOOTH_STATE_STARTING; start_tick=Now();
            hci_set_chipset(NULL);
            if(hci_power_control(HCI_POWER_ON))FailController(0x106U);
        }
        return;
    }
    if (r->operation==OP_STOP) {
        pairing_until=pairing_seconds=0U;gap_discoverable_control(0);
        g_bluetooth.state=BLUETOOTH_STATE_STOPPING; hci_power_control(HCI_POWER_OFF); return;
    }
    if (r->operation==OP_WINDOW) {
        pairing_seconds=g_bluetooth.state==BLUETOOTH_STATE_READY?0:r->value;
        pairing_until=!pairing_seconds&&r->value?Now()+r->value*1000U:0;
        gap_discoverable_control(pairing_until!=0);return;
    }
    if (r->operation==OP_FORGET) { gap_drop_link_key_for_bd_addr((uint8_t *)r->address); return; }
    if (r->operation==OP_DISCONNECT) {
        if (g_bluetooth.links[role].cid ||
            g_bluetooth.links[role].status==BLUETOOTH_LINK_CONNECTING)
            CancelRole(role,BLUETOOTH_ERROR_LOCAL_CANCEL);
        else Retry(role,0U);
        return;
    }
    if (g_bluetooth.state!=BLUETOOTH_STATE_READY) { g_bluetooth.command_rejected++; return; }

}

static void Tick(btstack_timer_source_t *ts)
{
    static uint32_t power_stopped;
    Request request;
    PowerService_Acknowledge(POWER_OWNER_BT,0);
    uint32_t deep=PowerService_Mode()==POWER_DEEP;
    if(deep&&!power_stopped){Request stop={.operation=OP_STOP};ProcessRequest(&stop);power_stopped=1;}
    if(!deep&&power_stopped){power_stopped=0;Request start={.operation=OP_START};ProcessRequest(&start);}
    g_bluetooth.heartbeat++;
    g_bluetooth.stack_low_words=uxTaskGetStackHighWaterMark(NULL);
    if (fault_pending) {
        uint32_t reason=fault_pending;fault_pending=0U;FailController(reason);
    }
    if (g_bluetooth.state==BLUETOOTH_STATE_STARTING && Now()-start_tick>20000U) {
        FailController(0x302U);
    }
    if(!deep)PollControl();
    while(xQueueReceive(requests,&request,0)==pdTRUE){
        if(!deep||request.operation==OP_STOP)ProcessRequest(&request);
        else ++g_bluetooth.command_rejected;
    }
    if (pairing_until && Due(Now(),pairing_until)) { pairing_until=0U; gap_discoverable_control(0); }
    if (g_bluetooth.state==BLUETOOTH_STATE_READY) for (unsigned i=0;i<BLUETOOTH_ROLE_COUNT;i++) {
        Link *link=&links[i];
        PendingTick(i);
        if (g_bluetooth.links[i].status!=BLUETOOTH_LINK_UP) continue;
        if (!link->credit && BLUETOOTH_RX_CAPACITY-link->rx_count>=g_bluetooth.links[i].mtu) {
            link->credit=1U;
            if (rfcomm_grant_credits(g_bluetooth.links[i].cid,1U)!=ERROR_CODE_SUCCESS) link->credit=0U;
        }
        if (link->tx_count && !link->send_requested) {
            link->send_requested=1U;
            if (rfcomm_request_can_send_now_event(g_bluetooth.links[i].cid)!=ERROR_CODE_SUCCESS) link->send_requested=0U;
        }
    }
    if(deep&&hci_get_state()==HCI_STATE_OFF&&!g_bsp_bt_hci.opened&&!BSP_BT_HCI_IsQuarantined())g_bluetooth.state=BLUETOOTH_STATE_OFF;
    uint32_t asleep=deep&&g_bluetooth.state==BLUETOOTH_STATE_OFF&&!g_bsp_bt_hci.opened&&!BSP_BT_HCI_IsQuarantined();
    PowerService_Acknowledge(POWER_OWNER_BT,asleep);
    /* This is application housekeeping, not the HCI transport pump. Native
     * BTstack timers and UART/DMA callbacks still wake their run loop at once;
     * keep the10ms startup cadence, then poll OFF-state requests at100ms. */
    uint32_t retained=PowerService_Mode()==POWER_DISPLAY_SLEEP&&g_bluetooth.state!=BLUETOOTH_STATE_STARTING;
    btstack_run_loop_set_timer(ts,asleep?1000U:retained?100U:10U);btstack_run_loop_add_timer(ts);
    if(g_bluetooth_control.controller_state!=g_bluetooth.state){
        ControlBegin();g_bluetooth_control.controller_state=g_bluetooth.state;ControlEnd();}
    /* Controller failure is permitted; completion of this owner iteration is
     * required. A missing radio/peer is not a watchdog fault. */
    HealthService_Progress(HEALTH_BT,Now());
}

static void Thread(void *argument)
{
    (void)argument;
    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_freertos_get_instance());
    hci_init(hci_transport_h4_instance(Bluetooth_UARTInstance()),&uart_config);
    hci_set_link_key_db(Bluetooth_KeyDB());
    event_registration.callback=Packet; hci_add_event_handler(&event_registration);
    l2cap_init(); rfcomm_init(); sdp_init();
    uint8_t status=rfcomm_register_service_with_initial_credits(Packet,1U,512U,1U);
    if (status) { g_bluetooth.last_error=status; g_bluetooth.state=BLUETOOTH_STATE_FAULT; }
    spp_create_sdp_record(service_record,0x10001U,1U,"Noodoe CFW SPP");
    if (de_get_len(service_record)>sizeof(service_record) || sdp_register_service(service_record)) {
        g_bluetooth.last_error=0x303U; g_bluetooth.state=BLUETOOTH_STATE_FAULT;
    }
    gap_set_local_name(local_name); gap_set_class_of_device(0x001f00U);
    gap_ssp_set_io_capability(SSP_IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    gap_ssp_set_auto_accept(0); gap_set_security_level(LEVEL_2);
    gap_connectable_control(1); gap_discoverable_control(0);
    timer.process=Tick; btstack_run_loop_set_timer(&timer,1U); btstack_run_loop_add_timer(&timer);
    if (g_bluetooth.state!=BLUETOOTH_STATE_FAULT) {
        Request start={.operation=OP_START}; ProcessRequest(&start);
    }
    btstack_run_loop_execute();
    /* The service never deletes its static task; stop/start controls HCI only. */
    for (;;) vTaskDelay(portMAX_DELAY);
}

static int Enqueue(const Request *request)
{
    if (!requests) return BLUETOOTH_NOT_READY;
    /* Pair publication and queue insertion atomically against BT ownership;
     * otherwise a preempted caller could enqueue START after raw acceptance. */
    taskENTER_CRITICAL();
    if(raw_request_active){taskEXIT_CRITICAL();return BLUETOOTH_BUSY;}
    if (xQueueSend(requests,request,0)!=pdTRUE) { taskEXIT_CRITICAL();g_bluetooth.command_rejected++; return BLUETOOTH_QUEUE_FULL; }
    taskEXIT_CRITICAL();
    return 0;
}
int Bluetooth_Start(void)
{
    if (owner) { Request r={.operation=OP_START}; return Enqueue(&r); }
    g_bluetooth.magic=0x42545331U;g_bluetooth.version=2U;
#if NOODOE_DEEP_DIAGNOSTICS
    g_bsp_bt_reset_diagnostic.magic=0x42524431U;g_bsp_bt_reset_diagnostic.version=1U;
#endif
    requests=xQueueCreateStatic(BT_REQUEST_COUNT,sizeof(Request),queue_buffer,&queue_object);
    if (!requests) return BLUETOOTH_QUEUE_FULL;
    /* Registration precedes scheduling: a created worker that never runs
     * cannot disappear from the boot/runtime health contract. An absent radio
     * still has a live worker and continues its normal progress reports. */
    HealthService_Register(HEALTH_BT,Now());
    owner=xTaskCreateStatic(Thread,"Bluetooth",BT_STACK_WORDS,NULL,BT_TASK_PRIORITY,task_stack,&task_object);
    return owner?0:BLUETOOTH_QUEUE_FULL;
}
int Bluetooth_Stop(void) { Request r={.operation=OP_STOP}; return Enqueue(&r); }
static int RequestControl(uint32_t sequence,uint32_t command)
{
    if(!requests)return BLUETOOTH_NOT_READY;
    taskENTER_CRITICAL();
    if(!sequence || (int32_t)(sequence-g_bluetooth_control.request_sequence)<=0){taskEXIT_CRITICAL();return BLUETOOTH_INVALID;}
    if(g_bluetooth_control.request_sequence!=g_bluetooth_control.ack_sequence
       || g_bluetooth_control.phase==1U || g_bluetooth_control.phase==2U){taskEXIT_CRITICAL();return BLUETOOTH_BUSY;}
    g_bluetooth_control.command=command;__DMB();g_bluetooth_control.request_sequence=sequence;
    taskEXIT_CRITICAL();return 0;
}
int Bluetooth_RequestStart(uint32_t sequence){return RequestControl(sequence,1U);}
int Bluetooth_RequestResetDiagnostic(uint32_t sequence)
{return NOODOE_DEEP_DIAGNOSTICS?RequestControl(sequence,2U):BLUETOOTH_UNSUPPORTED;}
int Bluetooth_GetResetDiagnosticSnapshot(BSP_BT_ResetDiagnostic *out)
{
#if NOODOE_DEEP_DIAGNOSTICS
    if(!out || __get_IPSR())return 0;
    taskENTER_CRITICAL();
    uint32_t seq=g_bsp_bt_reset_diagnostic.sequence;__DMB();
    if(seq&1U){taskEXIT_CRITICAL();return 0;}
    *out=g_bsp_bt_reset_diagnostic;__DMB();
    int valid=g_bsp_bt_reset_diagnostic.sequence==seq;
    taskEXIT_CRITICAL();return valid;
#else
    (void)out;return 0;
#endif
}
void Bluetooth_GetControl(Bluetooth_ControlMailbox *out)
{if(out){taskENTER_CRITICAL();*out=g_bluetooth_control;taskEXIT_CRITICAL();}}
int Bluetooth_Discover(void) { return BLUETOOTH_UNSUPPORTED; }
int Bluetooth_SetPairingWindow(uint32_t seconds)
{
    if (seconds>180U) return BLUETOOTH_INVALID;
    Request r={.operation=OP_WINDOW,.value=seconds}; return Enqueue(&r);
}
uint32_t Bluetooth_PairingSeconds(void)
{
    taskENTER_CRITICAL();uint32_t now=Now(),until=pairing_until;
    uint32_t seconds=g_bluetooth.state==BLUETOOTH_STATE_READY&&until&&!Due(now,until)?(until-now+999U)/1000U:0U;
    taskEXIT_CRITICAL();return seconds;
}
int Bluetooth_ForgetAll(void)
{
    /* Reject live/in-flight controller work, including queued legacy requests.
     * No blocking HCI command or persistent write runs in this API. */
    taskENTER_CRITICAL();
    if(!StartAllowed()||raw_request_active||g_bluetooth_control.phase==1U||g_bluetooth_control.phase==2U||
       (requests&&uxQueueMessagesWaiting(requests))){taskEXIT_CRITICAL();return BLUETOOTH_BUSY;}
    Bluetooth_ClearKeys();taskEXIT_CRITICAL();return BLUETOOTH_OK;
}
int Bluetooth_Pair(const uint8_t address[6],const char *pin)
{(void)address;(void)pin;return BLUETOOTH_UNSUPPORTED;}
int Bluetooth_Forget(const uint8_t address[6])
{
    if (!address) return BLUETOOTH_INVALID;
    Request r={.operation=OP_FORGET}; memcpy(r.address,address,6U); return Enqueue(&r);
}
int Bluetooth_Connect(Bluetooth_Role role,const uint8_t address[6],uint8_t channel)
{(void)role;(void)address;(void)channel;return BLUETOOTH_UNSUPPORTED;}
int Bluetooth_Disconnect(Bluetooth_Role role)
{
    if (!ValidRole(role)) return BLUETOOTH_INVALID;
    Request r={.operation=OP_DISCONNECT,.role=(uint8_t)role}; return Enqueue(&r);
}
/* Called only under the queue critical section. The snapshot is caller-owned;
 * a reused RFCOMM CID alone is insufficient to identify a remote session. */
static int SameSession(Bluetooth_Role role,const Bluetooth_LinkState *expected)
{
    const volatile Bluetooth_LinkState *current=&g_bluetooth.links[role];
    return expected->status==BLUETOOTH_LINK_UP && current->status==BLUETOOTH_LINK_UP
        && current->cid==expected->cid && current->reconnects==expected->reconnects
        && !memcmp((const void *)current->address,expected->address,6U);
}
static int SendChecked(Bluetooth_Role role,const Bluetooth_LinkState *expected,const void *data,size_t length)
{
    if (!ValidRole(role) || (!data && length) || length>BLUETOOTH_TX_CAPACITY) return BLUETOOTH_INVALID;
    Link *link=&links[role];
    taskENTER_CRITICAL();
    if (g_bluetooth.links[role].status!=BLUETOOTH_LINK_UP || (expected && !SameSession(role,expected))) {
        taskEXIT_CRITICAL(); return BLUETOOTH_NOT_READY;
    }
    if (length>BLUETOOTH_TX_CAPACITY-link->tx_count) {
        g_bluetooth.links[role].tx_rejected++; taskEXIT_CRITICAL(); return BLUETOOTH_QUEUE_FULL;
    }
    const uint8_t *bytes=data;
    for (size_t i=0;i<length;i++) { link->tx[link->tx_head]=bytes[i]; link->tx_head=(link->tx_head+1U)%BLUETOOTH_TX_CAPACITY; }
    link->tx_count+=(uint16_t)length; g_bluetooth.links[role].tx_queued=link->tx_count;
    taskEXIT_CRITICAL(); return 0;
}
static int ReceiveChecked(Bluetooth_Role role,const Bluetooth_LinkState *expected,void *data,size_t capacity)
{
    if (!ValidRole(role) || (!data && capacity)) return BLUETOOTH_INVALID;
    Link *link=&links[role]; uint8_t *bytes=data;
    taskENTER_CRITICAL();
    if (expected && !SameSession(role,expected)) { taskEXIT_CRITICAL(); return BLUETOOTH_NOT_READY; }
    size_t count=capacity<link->rx_count?capacity:link->rx_count;
    for (size_t i=0;i<count;i++) { bytes[i]=link->rx[link->rx_tail]; link->rx_tail=(link->rx_tail+1U)%BLUETOOTH_RX_CAPACITY; }
    link->rx_count-=(uint16_t)count; g_bluetooth.links[role].rx_queued=link->rx_count;
    taskEXIT_CRITICAL(); return (int)count;
}
int Bluetooth_Send(Bluetooth_Role role,const void *data,size_t length)
{ return SendChecked(role,NULL,data,length); }
int Bluetooth_Receive(Bluetooth_Role role,void *data,size_t capacity)
{ return ReceiveChecked(role,NULL,data,capacity); }
int Bluetooth_SendSession(Bluetooth_Role role,const Bluetooth_LinkState *expected,const void *data,size_t length)
{ return expected?SendChecked(role,expected,data,length):BLUETOOTH_INVALID; }
int Bluetooth_ReceiveSession(Bluetooth_Role role,const Bluetooth_LinkState *expected,void *data,size_t capacity)
{ return expected?ReceiveChecked(role,expected,data,capacity):BLUETOOTH_INVALID; }
int Bluetooth_GetLinkState(Bluetooth_Role role,Bluetooth_LinkState *out)
{
    if (!ValidRole(role) || !out) return BLUETOOTH_INVALID;
    taskENTER_CRITICAL(); *out=g_bluetooth.links[role]; taskEXIT_CRITICAL(); return 0;
}
void Bluetooth_GetDiagnostics(Bluetooth_Diagnostics *out)
{
    if (!out) return;
    taskENTER_CRITICAL(); *out=g_bluetooth; taskEXIT_CRITICAL();
}
size_t Bluetooth_GetDiscovered(Bluetooth_DiscoveredDevice *out,size_t capacity)
{(void)out;(void)capacity;return 0U;}
void Bluetooth_TransportFault(uint32_t reason) { fault_pending=reason; }
