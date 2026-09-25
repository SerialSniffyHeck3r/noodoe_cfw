#ifndef BSP_INPUT_MODE_H
#define BSP_INPUT_MODE_H
#include <stdint.h>
/* Stock PH9 selector only: no pull, no EXTI and no changes to other pins.
 * Init is idempotent. Read returns the electrical level, not UI permission. */
void BSP_InputMode_Init(void);
uint32_t BSP_InputMode_Read(void);
#endif
