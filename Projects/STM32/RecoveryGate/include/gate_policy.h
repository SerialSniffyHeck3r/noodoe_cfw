#ifndef NOODOE_GATE_POLICY_H
#define NOODOE_GATE_POLICY_H
#include "gate_abi.h"
typedef struct {
 uint32_t raw_enter,enter,changed,pressed,off_since,on_since,off_known,armed,on_pending;
 uint32_t qualified,armed_since,expired;
} GateGesture;
uint32_t GatePolicy_Crc(const void *,uint32_t);
uint32_t GateRetained_Valid(const GateRetained *);
void GateRetained_Init(GateRetained *);
void GateRetained_Request(GateRetained *,uint32_t reason);
/* Returns nonzero when repeated unconfirmed handoffs must stop in WAIT. */
uint32_t GateRetained_BeforeBoot(GateRetained *);
uint32_t GateRetained_Confirm(GateRetained *,uint32_t healthy_ms);
void GateGesture_Init(GateGesture *,uint32_t now,uint32_t enter);
/* ENTER80ms stable, IGN OFF200ms, ENTER held500ms before ON; then held2s.
 * Authorization expires30s after arming and requires a fresh release before
 * another attempt. Initial ON+ENTER cannot satisfy this. Returns1 once. */
uint32_t GateGesture_Process(GateGesture *,uint32_t now,uint32_t ign_on,uint32_t enter);
#endif
