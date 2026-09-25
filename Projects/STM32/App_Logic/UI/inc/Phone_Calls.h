#ifndef PHONE_CALLS_H
#define PHONE_CALLS_H
#include <stdint.h>
#define PHONE_CALLS_MAX 20U
enum { CALL_IDLE, CALL_RINGING, CALL_DIALING, CALL_ACTIVE, CALL_HELD, CALL_ENDING };
enum { CALL_DIAL, CALL_ANSWER, CALL_END };
enum { CALL_CONTACTS_PERMISSION=1, CALL_RECENTS_PERMISSION=2, CALL_DIAL_PERMISSION=4, CALL_CONTROL_PERMISSION=8 };
typedef struct {uint32_t id,group;} PhoneCallEntry;
/* Phone-local opaque IDs only. Names and numbers stay on the phone; the
 * currently selected three-row list is rendered as a bounded CJK panel. */
typedef struct {
 uint32_t epoch,generation,permissions,count,active_id,state,elapsed_s;
 uint32_t visual_target,visual_key,received_ms,call_type;
 PhoneCallEntry entries[PHONE_CALLS_MAX];
} PhoneCallsSnapshot;
typedef struct {
 PhoneCallsSnapshot phone;
 uint32_t selected,return_card,return_selection,return_valid,shown_incoming;
} UiCalls;
/* Validate a complete version1 snapshot before publishing. LE32, exact size,
 * at most ten entries per group, unique IDs and no partial list mutation. */
uint32_t PhoneCalls_Decode(PhoneCallsSnapshot *out,const uint8_t *p,uint32_t bytes,uint32_t epoch,uint32_t now);
uint32_t PhoneCalls_Fresh(const PhoneCallsSnapshot *p,uint32_t now);
uint32_t PhoneCalls_Target(const UiCalls *calls);
#endif
