#ifndef BOOTSTRAP_TARGET_H
#define BOOTSTRAP_TARGET_H
#include <stdint.h>
/* Read returns raw physical NOR bytes; this boundary swaps each pair once.
 * The caller already owns storage and has verified the entire OTA image. */
uint32_t BootstrapTarget_Check(uint32_t (*read)(uint32_t,void *,uint32_t),
 uint32_t (*now)(void),const uint8_t slot_sha[2][32],const uint8_t journal_sha[32],
 const uint8_t requirement[44]);
#endif
