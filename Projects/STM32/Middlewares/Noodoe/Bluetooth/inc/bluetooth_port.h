#ifndef NOODOE_BLUETOOTH_PORT_H
#define NOODOE_BLUETOOTH_PORT_H
#include "btstack_uart.h"
#include "btstack_chipset.h"
const btstack_uart_t *Bluetooth_UARTInstance(void);
/* Owner-only guard for the isolated fixed Reset diagnostic. H4 Close must
 * have removed its poll source, retained no TX pointer and released ownership. */
int Bluetooth_TransportIsDetached(void);
/* Select only unpatched TI ROM IDs supported by pinned TI service packs. */
int Bluetooth_SelectPatch(uint16_t manufacturer,uint16_t lmp,uint32_t *bytes);
const btstack_chipset_t *Bluetooth_ChipsetInstance(void);
#endif
