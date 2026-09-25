#ifndef NOODOE_BLUETOOTH_KEYS_H
#define NOODOE_BLUETOOTH_KEYS_H
#include "btstack_defines.h"
#include "classic/btstack_link_key_db.h"
const btstack_link_key_db_t *Bluetooth_KeyDB(void);
/* Service-only, after checking HCI OFF under the same critical section. */
void Bluetooth_ClearKeys(void);
#endif
