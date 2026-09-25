#ifndef BSP_AMBIENT_BITBANG_PRIVATE_H
#define BSP_AMBIENT_BITBANG_PRIVATE_H
#include "BSP_AmbientAddress.h"
/* Private BSP-to-BSP bridge. The bus owner validates index0..3 and permits
 * only address-probe plus the fixed identity words. No generic I2C API. */
typedef uint32_t (*BSP_AmbientAddress_Candidate)(uint32_t index,
    volatile BSP_AmbientAddress_Entry *entry);
uint32_t BSP_AmbientAddress_Execute(BSP_AmbientAddress_Candidate candidate);
uint32_t BSP_AmbientBitbang_RunAddressDiagnostic(uint32_t id,uint32_t sequence);
#endif
