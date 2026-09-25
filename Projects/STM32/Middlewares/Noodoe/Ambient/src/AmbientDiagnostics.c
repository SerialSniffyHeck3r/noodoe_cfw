#include "AmbientService.h"
#include "stm32f4xx_hal.h"
/* Optional one-shot electrical experiments; never linked into Bootstrap. */
static uint32_t Lock(void){uint32_t k=__get_PRIMASK();__disable_irq();return k;}
static void Unlock(uint32_t k){__DMB();__set_PRIMASK(k);}
uint32_t AmbientService_GetAddressDiagnosticSnapshot(BSP_AmbientAddress_Diagnostics *out)
{
    if(!out)return 0U;
    uint32_t key=Lock(),seq=g_bsp_ambient_address.sequence;
    if(!seq || (seq&1U)){Unlock(key);return 0U;}
    *out=g_bsp_ambient_address;
    Unlock(key);return 1U;
}
uint32_t AmbientService_GetIDDiagnosticSnapshot(BSP_AmbientBitbang_Diagnostics *out)
{
    if(!out)return 0U;
    uint32_t key=Lock(),seq=g_bsp_ambient_bitbang.sequence;
    if(!seq || (seq&1U)){Unlock(key);return 0U;}
    /* The producer is the single thread-mode owner; the brief masked copy
     * cannot preempt an executing producer and receive a torn even record. */
    *out=g_bsp_ambient_bitbang;
    Unlock(key);return 1U;
}


uint32_t AmbientDiagnostics_Execute(uint32_t command,uint32_t argument,uint32_t id,uint32_t seq,uint32_t *restore){uint32_t result;
        if(command==AMBIENT_COMMAND_ADDRESS_DIAGNOSTIC) {
            result=BSP_AmbientAddress_ReadFixedCandidates(id,seq);
            *restore=g_bsp_ambient_address.restore_result;
        }
        else {
            result=argument?
                BSP_AmbientBitbang_ReadIDsWithPullup(id,seq):
                BSP_AmbientBitbang_ReadIDs(id,seq);
            *restore=g_bsp_ambient_bitbang.restore_result;
        }
return result;}
