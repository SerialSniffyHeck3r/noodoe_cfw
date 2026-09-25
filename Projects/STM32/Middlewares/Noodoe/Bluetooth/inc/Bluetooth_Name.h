#ifndef NOODOE_BLUETOOTH_NAME_H
#define NOODOE_BLUETOOTH_NAME_H
#include <stdint.h>
/* Canonical display-order BD_ADDR, never UID or peer address. Caller owns
 * the 20-byte name for the entire stack lifetime; both roles share this. */
static inline void Bluetooth_NameSuffix(char name[20],const uint8_t address[6])
{
    static const char hex[]="0123456789ABCDEF";
    for(unsigned i=0;i<3;i++){
        name[13+i*2]=hex[address[3+i]>>4];
        name[14+i*2]=hex[address[3+i]&15];
    }
}
#endif
