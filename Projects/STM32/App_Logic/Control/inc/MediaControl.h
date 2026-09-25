#ifndef MEDIA_CONTROL_H
#define MEDIA_CONTROL_H
#include <stdint.h>
#define MEDIA_CONTROL_OPCODE 0x10U
#define PHONE_REPLY_OPCODE 0x11U
#define PHONE_CALL_OPCODE 0x12U
uint32_t MediaControl_Call(const uint32_t words[5],uint32_t now);
uint32_t MediaControl_Reply(const uint32_t words[5],uint32_t now);
/* Task-context request; copies a command bound to the current phone session.
 * Returns0 on local acceptance,2 offline,3 busy,1 invalid. Acceptance is not
 * playback success: the companion must execute and acknowledge the request. */
uint32_t MediaControl_Request(uint32_t slot,uint32_t action,uint32_t now);
void MediaControl_Poll(uint32_t now);
uint32_t MediaControl_Take(uint32_t slot,uint8_t *out,uint32_t capacity);
uint32_t MediaControl_IsPending(uint32_t slot,uint32_t sequence);
void MediaControl_Acknowledge(uint32_t slot,uint32_t sequence,uint32_t result);
typedef struct {uint32_t requests,accepted,acknowledged,failed,last_slot,last_action,last_sequence,last_result;} MediaControlDiagnostics;
extern volatile MediaControlDiagnostics g_media_control;
#endif
