#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "RAM codec requires little-endian target"
#endif
#include "Noodoe_Crc32.h"
#include "Resources.h"
#include <string.h>
static uint32_t U32(const uint8_t *p){ uint32_t v;memcpy(&v,p,4);return v;}
static uint32_t CRC(const uint8_t *p,uint32_t n)
{return Noodoe_Crc32(p,n);}
/* Reject impossible entries before touching SDRAM. Metadata is part of SHA,
 * so a repaired CRC alone cannot redirect a glyph into a different asset. */
ResourcesError Resources_CheckHeader(const uint8_t *h,const uint8_t required[32])
{
    if(!h||!required)return RESOURCE_FORMAT;
    if(U32(h)!=RESOURCES_MAGIC||U32(h+4092)!=RESOURCES_COMMIT)return RESOURCE_FORMAT;
    if(U32(h+4)!=1)return RESOURCE_VERSION;
    uint32_t total=U32(h+8),count=U32(h+12),end=0;
    /* Other known-good APPs can require an older table. The requested SHA
     * pins its exact contents; only the live loader enforces its own count. */
    if(!total||total>RESOURCES_SLOT_BYTES-RESOURCES_HEADER_BYTES||!count||count>(4088U-48U)/16U)return RESOURCE_SIZE;
    if(CRC(h,4088)!=U32(h+4088))return RESOURCE_CRC;
    if(memcmp(h+16,required,32))return RESOURCE_VERSION;
    for(uint32_t i=0;i<count;++i){const uint8_t *e=h+48+i*16;
        uint32_t off=U32(e+4),n=U32(e+8);
        if(U32(e)!=i+1||off<end||(off&3)||!n||off>=total||n>total-off)return RESOURCE_FORMAT;
        end=off+n;
    }
    return end==total?RESOURCE_OK:RESOURCE_SIZE;
}
/* Apps with no external resources still publish a recognizable requirement. */
__attribute__((section(".resource_requirement"),used))
const ResourceRequirement g_resource_requirement={0x51534352U,1U,
#if NOODOE_PRODUCT
1U,
#else
0U,
#endif
RESOURCES_EXPECTED_SHA};
