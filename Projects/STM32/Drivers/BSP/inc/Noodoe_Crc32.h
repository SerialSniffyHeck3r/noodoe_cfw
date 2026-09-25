#ifndef NOODOE_CRC32_H
#define NOODOE_CRC32_H
#include <stdint.h>
/* Reflected ISO CRC32 running state. Caller owns initial/final XOR; a zero
 * byte count does not dereference data. No hardware, allocation or locking. */
uint32_t Noodoe_Crc32Feed(uint32_t state,const volatile void *data,uint32_t bytes);
/* Complete ISO-HDLC checksum with standard initial/final XOR. */
uint32_t Noodoe_Crc32(const volatile void *data,uint32_t bytes);
#endif
