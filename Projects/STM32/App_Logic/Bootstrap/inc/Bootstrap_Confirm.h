#ifndef BOOTSTRAP_CONFIRM_H
#define BOOTSTRAP_CONFIRM_H
#include <stdint.h>
#define BOOTSTRAP_CONFIRM_MS 2000U
typedef struct { uint32_t released,holding,since,fired,elapsed; } BootstrapConfirm;
/* Reset on entry/exit. A button held while entering is never authorization. */
void BootstrapConfirm_Init(BootstrapConfirm *s);
/* Call from one owner with raw active-high pressed state and monotonic ms.
 * Any release cancels immediately; returns one event after a fresh 2s hold. */
uint32_t BootstrapConfirm_Process(BootstrapConfirm *s,uint32_t now,uint32_t pressed);
#endif
