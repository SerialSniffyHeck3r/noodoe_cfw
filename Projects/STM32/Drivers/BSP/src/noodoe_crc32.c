#include "Noodoe_Crc32.h"
/* Shared out-of-line implementation avoids one polynomial loop per storage,
 * protocol and recovery module, including builds without LTO. Metadata's RAM
 * flash writer deliberately keeps its independent SRAM-resident checksum. */
__attribute__((noinline)) uint32_t Noodoe_Crc32Feed(uint32_t state,const volatile void *data,uint32_t bytes)
{const volatile uint8_t *p=data;while(bytes--){state^=*p++;for(uint32_t bit=0;bit<8;bit++)state=(state>>1)^((0U-(state&1U))&0xedb88320U);}return state;}
__attribute__((noinline)) uint32_t Noodoe_Crc32(const volatile void *data,uint32_t bytes)
{return ~Noodoe_Crc32Feed(~0U,data,bytes);}
