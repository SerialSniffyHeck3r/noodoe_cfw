#ifndef NOODOE_BLUETOOTH_H
#define NOODOE_BLUETOOTH_H
#include <stdint.h>
#include <stddef.h>
#include "BSP_BT_ResetDiagnostic.h"

typedef enum { BLUETOOTH_PHONE=0, BLUETOOTH_ELM=1, BLUETOOTH_PHONE2=2,
               BLUETOOTH_ROLE_COUNT=1 } Bluetooth_Role;
/* One inbound phone. Wire role1 (ELM) and role2 (phone2) remain reserved
 * and are rejected; they must never become aliases of slot0. */
#define BLUETOOTH_PHONE_CAPACITY 1U
static inline uint32_t Bluetooth_IsPhoneRole(unsigned role)
{return role==BLUETOOTH_PHONE;}
static inline Bluetooth_Role Bluetooth_PhoneRole(unsigned slot)
{return slot==0U?BLUETOOTH_PHONE:BLUETOOTH_ROLE_COUNT;}
typedef enum { BLUETOOTH_STATE_OFF=0, BLUETOOTH_STATE_STARTING=1, BLUETOOTH_STATE_READY=2,
               BLUETOOTH_STATE_FAULT=3, BLUETOOTH_STATE_STOPPING=4 } Bluetooth_State;
typedef enum { BLUETOOTH_LINK_OFF=0, BLUETOOTH_LINK_SDP=1,
               BLUETOOTH_LINK_CONNECTING=2, BLUETOOTH_LINK_UP=3,
               BLUETOOTH_LINK_RETRY=4, BLUETOOTH_LINK_CANCELLING=5 } Bluetooth_LinkStatus;
#define BLUETOOTH_CONNECT_TIMEOUT_MS 15000U
#define BLUETOOTH_CANCEL_TIMEOUT_MS 3000U
#define BLUETOOTH_ERROR_CONNECT_TIMEOUT 0x410U
#define BLUETOOTH_ERROR_CANCEL_TIMEOUT 0x411U
#define BLUETOOTH_ERROR_LOCAL_CANCEL 0x412U
enum { BLUETOOTH_OK=0, BLUETOOTH_INVALID=-1, BLUETOOTH_BUSY=-2,
       BLUETOOTH_NOT_READY=-3, BLUETOOTH_QUEUE_FULL=-4, BLUETOOTH_UNSUPPORTED=-5 };
#define BLUETOOTH_RX_CAPACITY 2048U
#define BLUETOOTH_TX_CAPACITY 1024U
#define BLUETOOTH_MAX_DISCOVERED 12U

typedef struct {
    uint8_t address[6]; /* conventional most-significant octet first */
    uint8_t status, server_channel;
    uint16_t cid, mtu;
    uint32_t rx_bytes, tx_bytes, rx_overflow, tx_rejected;
    /* Successful-open session epoch, including first open. Compare epoch and
     * status when binding protocol state; both edges can pass between polls. */
    uint32_t reconnects, last_error, rx_queued, tx_queued;
} Bluetooth_LinkState;
typedef struct {
    uint32_t magic, version, state, last_error, heartbeat;
    uint32_t manufacturer, lmp_subversion, hci_revision, patch_bytes;
    uint32_t baud, commands, command_rejected, hci_errors, pairings;
    uint32_t key_generation, key_persisted_generation, stack_low_words;
    uint8_t local_address[6]; uint8_t discovered_count, reserved;
    Bluetooth_LinkState links[3]; /* Stable v2 diagnostics ABI; retired entries are zero. */
} Bluetooth_Diagnostics;
extern volatile Bluetooth_Diagnostics g_bluetooth;
typedef struct { uint8_t address[6]; int8_t rssi; uint8_t reserved;
                 uint32_t class_of_device; } Bluetooth_DiscoveredDevice;
typedef struct { uint8_t address[6], key[16], type, valid; } Bluetooth_LinkKey;
typedef struct { uint32_t version, generation; Bluetooth_LinkKey keys[6]; } Bluetooth_KeyStore;

/* Starts one statically allocated BT task. Further Start calls request a
 * power-up of that same task. Ordinary APIs are nonblocking task-context calls;
 * the explicitly bounded flash quiesce/resume pair below is the exception. */
int Bluetooth_Start(void);
int Bluetooth_Stop(void);
/* Explicit one-shot restart control, separate from the stable diagnostics ABI.
 * The host writes command=1 (START) or2 (isolated fixed raw HCI Reset) then an
 * increasing nonzero request_sequence LAST. No argument/payload is accepted.
 * Owner ack distinguishes queue acceptance from completion_sequence/result.
 * phase:0 idle,1 queued,2 running,3 succeeded,4 failed. Read result_sequence
 * before/after owner fields; equal even values make a coherent snapshot.
 * Invalid/BUSY requests do not overwrite an earlier operation's completion. */
typedef struct {
    uint32_t magic,version,request_sequence,command,ack_sequence;
    int32_t accept_result;
    uint32_t operation_id,phase,completion_sequence;
    int32_t completion_result;
    uint32_t controller_state,result_sequence;
} Bluetooth_ControlMailbox;
extern volatile Bluetooth_ControlMailbox g_bluetooth_control;
/* Task-context zero-wait submission. Return0 is publication only; inspect ACK.
 * Only FAULT/OFF with the underlying HCI OFF may start. No automatic retry. */
int Bluetooth_RequestStart(uint32_t request_sequence);
/* Explicit diagnostic only; zero-wait publication, not controller startup.
 * Owner runs once while H4 detached/HCI OFF/service OFF or FAULT. It sends
 * exactly 01 03 0C 00 with temporary CTSE override, waits <=500ms for raw RX,
 * then shuts down. Success leaves OFF/FAULT, never claims READY. The separate
 * BSP_BT_ResetDiagnostic record preserves raw bytes and cleanup evidence.
 * Other queued commands are rejected while this operation owns transport. */
int Bluetooth_RequestResetDiagnostic(uint32_t request_sequence);
/* Task-context snapshot only; returns1 for a coherent copied record,0 for
 * NULL or a publication in progress. Match operation_id/request_sequence to
 * the accepted control token, and phase6 for its final result. Does not wait,
 * start hardware, or interpret controller readiness from Reset success. */
int Bluetooth_GetResetDiagnosticSnapshot(BSP_BT_ResetDiagnostic *out);
void Bluetooth_GetControl(Bluetooth_ControlMailbox *out);
/* Retired outbound APIs: UNSUPPORTED (discovery snapshot is empty).
 * Phones initiate pairing/connection during the explicit pairing window. */
int Bluetooth_Discover(void);
size_t Bluetooth_GetDiscovered(Bluetooth_DiscoveredDevice *out,size_t capacity);
/* Explicit legacy targeted pairing is retired; use SetPairingWindow. */
int Bluetooth_Pair(const uint8_t address[6],const char *pin);
int Bluetooth_SetPairingWindow(uint32_t seconds);
/* Remaining discoverable/SSP window, zero unless the controller is READY.
 * ForgetAll clears host bonds only with HCI fully OFF. It never writes NOR;
 * wait for key_persisted_generation before advertising new pairing. */
uint32_t Bluetooth_PairingSeconds(void);
int Bluetooth_ForgetAll(void);
uint32_t Bluetooth_BondCount(void);
int Bluetooth_Forget(const uint8_t address[6]);
/* Outbound connect is retired and returns UNSUPPORTED for every role.
 * Disconnect only accepts the single inbound PHONE role. */
int Bluetooth_Connect(Bluetooth_Role role,const uint8_t address[6],uint8_t channel);
int Bluetooth_Disconnect(Bluetooth_Role role);
/* Send copies the entire block or returns QUEUE_FULL; no partial enqueue.
 * Receive copies up to capacity and returns bytes, including zero when empty. */
int Bluetooth_Send(Bluetooth_Role role,const void *data,size_t length);
int Bluetooth_Receive(Bluetooth_Role role,void *data,size_t capacity);
/* Bind the transfer to a previously observed connection snapshot. UP, CID,
 * address and reconnect count are rechecked in the same critical section as
 * the queue copy, so a BT-owner preemption cannot cross a session boundary.
 * Mismatch returns NOT_READY without changing queues or caller output. */
int Bluetooth_SendSession(Bluetooth_Role role,const Bluetooth_LinkState *expected,
                          const void *data,size_t length);
int Bluetooth_ReceiveSession(Bluetooth_Role role,const Bluetooth_LinkState *expected,
                             void *data,size_t capacity);
int Bluetooth_GetLinkState(Bluetooth_Role role,Bluetooth_LinkState *out);
/* Read-only, epoch-bound encrypted/authenticated Classic link proof. The
 * installer combines this with a separate physical maintenance intent. */
int Bluetooth_LinkSecure(Bluetooth_Role role,const Bluetooth_LinkState *expected);
void Bluetooth_GetDiagnostics(Bluetooth_Diagnostics *out);
/* Storage worker only, IRQ-enabled task context, never the BT owner/ISR.
 * Quiesce waits at most timeout_ms (1..1000) for the BT owner to hold RTS HIGH,
 * drain in-flight bytes and finish active TX. Success returns a nonzero token.
 * No flash operation may start on failure. Resume that exact token on every
 * success/failure exit after quiescing; it waits at most100ms for owner resume.
 * Calls do not change flash, controller reset, baud or hardware flow control. */
int Bluetooth_QuiesceTransport(uint32_t timeout_ms,uint32_t *token_out);
int Bluetooth_ResumeTransport(uint32_t token);
/* Storage worker API: import only before first Start; export produces an
 * atomic snapshot. Persist outside BT callbacks, then mark that generation.
 * Generation mismatch remains dirty and must be retried by storage worker. */
int Bluetooth_ImportKeys(const Bluetooth_KeyStore *keys);
/* Bootstrap's audited storage load may follow initial HCI startup. Existing
 * RAM keys win; missing durable peers are merged under the same task lock. */
int Bluetooth_RestoreKeys(const Bluetooth_KeyStore *keys);
int Bluetooth_ExportKeys(Bluetooth_KeyStore *keys);
void Bluetooth_MarkKeysPersisted(uint32_t generation);
/* Port-internal failure entry: schedules shutdown without touching UI. */
void Bluetooth_TransportFault(uint32_t reason);
#endif
