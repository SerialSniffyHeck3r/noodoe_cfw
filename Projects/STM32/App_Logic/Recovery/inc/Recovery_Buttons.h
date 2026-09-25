#ifndef RECOVERY_BUTTONS_H
#define RECOVERY_BUTTONS_H
#include <stdint.h>
#define RECOVERY_BUTTON_DOWN 1U
#define RECOVERY_BUTTON_ENTER 2U
typedef struct {uint32_t state,raw,stable,changed,held,confirm_held;} RecoveryButtons;
/* state0 chord,1 release,2 confirmation,3 accepted,4 cancelled. Fault/forced
 * entry starts atrelease so a boot-heldENTER cannot confirm accidentally. */
void RecoveryButtons_Init(RecoveryButtons *,uint32_t wait_release,uint32_t now);
void RecoveryButtons_Process(RecoveryButtons *,uint32_t raw,uint32_t now);
#endif
