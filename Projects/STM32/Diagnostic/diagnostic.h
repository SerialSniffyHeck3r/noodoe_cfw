#ifndef NOODOE_DIAGNOSTIC_H
#define NOODOE_DIAGNOSTIC_H
#include <stdint.h>
/* All destructive operations remain in Gate. This runtime only submits a
 * sealed recovery intent, after its config/log writer has drained. */
void Diagnostic_RequestStock(void);
void Diagnostic_ResetToGate(uint32_t reason);
#endif
