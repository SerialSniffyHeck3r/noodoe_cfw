#ifndef RADIO_SELF_TEST_H
#define RADIO_SELF_TEST_H
#include <stdint.h>
typedef struct {uint32_t epoch,nonce,bytes,crc,started,elapsed,complete,errors;} RadioSelfTestState;
extern RadioSelfTestState g_radio_self_test;
/* Same bounded NDCP handler in Bootstrap/Product. Caller admits only an
 * encrypted idle diagnostic connection; no flash/HCI reset/pairing changes. */
uint32_t RadioSelfTest_Handle(const uint8_t*,uint32_t n,uint32_t epoch,uint32_t now,uint8_t *reply,uint32_t *bytes);
#endif
