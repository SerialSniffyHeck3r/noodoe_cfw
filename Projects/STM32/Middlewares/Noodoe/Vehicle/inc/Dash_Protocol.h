#ifndef DASH_PROTOCOL_H
#define DASH_PROTOCOL_H
#include <stdint.h>

#define DASH_STARTUP_MS 3000U
#define DASH_RECEIVE_GAP_MS 400U
#define DASH_RETRY_MS 800U
#define DASH_REPLY_QUEUE_MAX 8U
typedef enum {DASH_PHASE_OFF=0,DASH_PHASE_STARTUP,DASH_PHASE_ACTIVE,DASH_PHASE_GAP} Dash_Phase;
typedef enum {DASH_TX_NONE=0,DASH_TX_REQUEST,DASH_TX_LIGHT,DASH_TX_STOP} Dash_TxKind;

/* Pure wire policy, owned by one task. Times are wrapping monotonic ms.
 * ACTIVE means the400ms stock state, not proof that the peer has answered.
 * seen distinguishes a request awaiting data from an observed active link. */
typedef struct {
    uint32_t phase,phase_ms,seen,group_count,pending_light,pending_request,pending_stop;
    uint32_t light_index,thresholds[10],overflowed_replies;
} Dash_Protocol;

/* Copies this module's preserved calibration table. Invalid tables fail;
 * no guessed calibration or sensor reading is substituted. Initial cached
 * index is zero and remains explicitly unavailable at the service layer until
 * a live sample or a diagnostic override is supplied. No hardware access. */
uint32_t DashProtocol_Init(Dash_Protocol *p,const uint32_t thresholds[10],uint32_t now);
/* Start uses stock3000ms grace. Stop queues CMD01=00 and cancels stale replies.
 * Explicit reconnect queues CMD01=04. These functions never send or wait. */
void DashProtocol_Start(Dash_Protocol *p,uint32_t now);
void DashProtocol_Stop(Dash_Protocol *p);
void DashProtocol_Reconnect(Dash_Protocol *p);
/* Notify complete XOR-valid F5 frames admitted by the stock parser: commands
 *21/22/41/42 with nonempty payload. Unknown IDs are rejected at0x0804EA5A,
 * before the dispatch default branch. A burst has eight pending reply slots. */
void DashProtocol_OnFrames(Dash_Protocol *p,uint32_t frames,uint32_t now);
void DashProtocol_Process(Dash_Protocol *p,uint32_t now);
/* Peek is idempotent while BSP is busy. Accepted consumes exactly the chosen
 * frame only after transport acceptance; physical TC is tracked by the service.
 * Six bytes of caller storage are required. No allocation or storage writes. */
Dash_TxKind DashProtocol_Peek(const Dash_Protocol *p,uint8_t out[6],uint32_t *length);
void DashProtocol_Accepted(Dash_Protocol *p,Dash_TxKind kind,uint32_t now);
/* Millilux is floored to integer lux exactly before threshold classification.
 * Returns index0..9; table validation belongs to Init. No sensor side effects. */
uint32_t DashProtocol_LightIndex(const Dash_Protocol *p,uint32_t millilux);
#endif
