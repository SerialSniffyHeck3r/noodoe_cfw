#ifndef SETTINGS_TEST_PORT_H
#define SETTINGS_TEST_PORT_H
#include <stdint.h>
/* Development-only timed key stimulus. Commit request last, then wait for
 * ack before reusing fields. It cannot inject speed, IGN, clock or UI state. */
typedef struct {uint32_t magic,version,request,button,hold_ms,ack,result,active,started_ms,now_ms;} SettingsTestMailbox;
extern volatile SettingsTestMailbox g_settings_test;
void SettingsTestPort_Process(uint32_t now);
#endif
