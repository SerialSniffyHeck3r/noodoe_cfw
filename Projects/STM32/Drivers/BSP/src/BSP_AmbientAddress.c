#include "bsp_ambient_bitbang_private.h"
#include <stddef.h>

volatile BSP_AmbientAddress_Diagnostics g_bsp_ambient_address;
_Static_assert(sizeof(BSP_AmbientAddress_Diagnostics)==BSP_AMBIENT_ADDRESS_BYTES,
               "fixed-address diagnostic wire ABI");
_Static_assert(offsetof(BSP_AmbientAddress_Diagnostics,entry)==56U,
               "fixed-address entry offset");
_Static_assert(offsetof(BSP_AmbientAddress_Diagnostics,saved_cr1)==216U,
               "fixed-address restoration offset");

/* Policy only: the existing bitbang engine owns the single pin takeover,
 * electrical timing and exact restoration. Even when a previous candidate
 * matched, a later electrical failure must remain the aggregate failure. */
uint32_t BSP_AmbientAddress_Execute(BSP_AmbientAddress_Candidate candidate)
{
    uint32_t mismatch=0U;
    for(uint32_t i=0U;i<BSP_AMBIENT_ADDRESS_COUNT;++i) {
        volatile BSP_AmbientAddress_Entry *row=&g_bsp_ambient_address.entry[i];
        row->attempted=1U;g_bsp_ambient_address.attempted_mask|=1UL<<i;
        uint32_t result=candidate(i,row);
        if(row->address_ack)g_bsp_ambient_address.address_ack_mask|=1UL<<i;
        if(result==BSP_ABB_OK)g_bsp_ambient_address.id_match_mask|=1UL<<i;
        else if(result==BSP_ABB_ID_MISMATCH)mismatch=1U;
        else if(result!=BSP_ABB_NACK)return result;
    }
    if(g_bsp_ambient_address.id_match_mask)return BSP_ABB_OK;
    return mismatch?BSP_ABB_ID_MISMATCH:BSP_ABB_NACK;
}

/* The public surface accepts identity/correlation only. The fixed candidate
 * set cannot be replaced by a caller-controlled scanner or register writer. */
uint32_t BSP_AmbientAddress_ReadFixedCandidates(uint32_t operation_id,uint32_t request_seq)
{
    return BSP_AmbientBitbang_RunAddressDiagnostic(operation_id,request_seq);
}
