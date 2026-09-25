#ifndef INPUT_MODE_H
#define INPUT_MODE_H
#include <stdint.h>
typedef struct {uint32_t raw_high,ign_on,allowed,blocked_presses,inhibited,pressed,accepted;
 uint32_t stable_high,candidate,since,initialized;} InputModeState;
extern InputModeState g_input_mode;
enum {INPUT_MODE_DROP,INPUT_MODE_ACCEPT,INPUT_MODE_NOTIFY};
/* UI owner only, allocation-free. Recovery/installation must use the original
 * unfiltered ButtonEvents stream. Sample cancels a held normal action even
 * without a new button edge; it returns the mask to invalidate in consumers. */
void InputMode_Init(uint32_t boot_held);
uint32_t InputMode_Sample(uint32_t high,uint32_t ignition,uint32_t held,uint32_t now);
/* Notify is emitted only on a fresh physical PRESS while ON and PH9 LOW.
 * Release rearms the key, never executes a previously cancelled operation. */
uint32_t InputMode_Event(uint32_t button,uint32_t event);
#endif
