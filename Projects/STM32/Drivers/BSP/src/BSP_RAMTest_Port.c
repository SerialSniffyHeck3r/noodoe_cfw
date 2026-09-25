#include "BSP_RAMTest_Port.h"
#include "BSP_RAM.h"
#include "dma.h"
#include "FreeRTOS.h"
#include "task.h"

static uint32_t dma_claimed,dma_active;
int RAMTestPort_Ready(void){return g_bsp_ram.ready!=0U;}
void *RAMTestPort_Allocate(size_t bytes){return BSP_RAM_Allocate(bytes);}
uint32_t RAMTestPort_Time(void){return HAL_GetTick();}
uint32_t RAMTestPort_Cycles(void){return DWT->CYCCNT;}
void RAMTestPort_Enter(void){taskENTER_CRITICAL();}
void RAMTestPort_Exit(void){taskEXIT_CRITICAL();}
void RAMTestPort_Barrier(void){__DSB();}

/* Basic RAM init is the only other compiled owner of this generated stream.
 * It must have completed before Prepare. Reject unexpected configuration or
 * an active/locked stream; this diagnostic never steals an existing transfer.
 * No IRQ is enabled by HAL_DMA_Start, so no high-priority RTOS callback exists. */
int RAMTestPort_DMAClaim(void)
{
    DMA_HandleTypeDef *h=&hdma_memtomem_dma2_stream0;
    if(dma_claimed || !RAMTestPort_Ready() || h->Instance!=DMA2_Stream0 ||
       h->State!=HAL_DMA_STATE_READY || h->Lock!=HAL_UNLOCKED ||
       (h->Instance->CR&(DMA_SxCR_DIR|DMA_SxCR_PINC|DMA_SxCR_MINC|DMA_SxCR_PSIZE|DMA_SxCR_MSIZE))!=
           (DMA_MEMORY_TO_MEMORY|DMA_SxCR_PINC|DMA_SxCR_MINC) ||
       (h->Instance->CR&(DMA_SxCR_EN|DMA_SxCR_TCIE|DMA_SxCR_HTIE|DMA_SxCR_TEIE|DMA_SxCR_DMEIE|DMA_SxCR_CIRC|DMA_SxCR_DBM)) ||
       (h->Instance->FCR&DMA_SxFCR_FEIE) ||
       h->Init.Direction!=DMA_MEMORY_TO_MEMORY || h->Init.PeriphInc!=DMA_PINC_ENABLE ||
       h->Init.MemInc!=DMA_MINC_ENABLE || h->Init.PeriphDataAlignment!=DMA_PDATAALIGN_BYTE ||
       h->Init.MemDataAlignment!=DMA_MDATAALIGN_BYTE || h->Init.Mode!=DMA_NORMAL)return 0;
    /* Enable the shared cycle counter without resetting any existing epoch. */
    CoreDebug->DEMCR|=CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL|=DWT_CTRL_CYCCNTENA_Msk;
    dma_claimed=1U;return 1;
}
void RAMTestPort_DMARelease(void){if(!dma_active)dma_claimed=0U;}
int RAMTestPort_DMAStart(uintptr_t source,uintptr_t destination,uint32_t bytes)
{
    if(!dma_claimed || dma_active || !bytes || bytes>512U)return 0;
    if(!BSP_RAM_DMAAccessible((void*)source,bytes,0)||!BSP_RAM_DMAAccessible((void*)destination,bytes,1))return 0;
    __DSB();
    if(HAL_DMA_Start(&hdma_memtomem_dma2_stream0,(uint32_t)source,(uint32_t)destination,bytes)!=HAL_OK)return 0;
    dma_active=1U;return 1;
}

/* Calling HAL_DMA_PollForTransfer(...,0) before TC would mark the handle READY
 * and unlock it while the stream can still be enabled. Inspect hardware first;
 * only a completed, non-error, disabled stream reaches the HAL finalizer. */
int RAMTestPort_DMAPoll(void)
{
    DMA_HandleTypeDef *h=&hdma_memtomem_dma2_stream0;
    if(!dma_active)return -1;
    uint32_t flags=DMA2->LISR;
    if(flags&(DMA_LISR_TEIF0|DMA_LISR_DMEIF0|DMA_LISR_FEIF0))return -1;
    if(!(flags&DMA_LISR_TCIF0) || (h->Instance->CR&DMA_SxCR_EN))return 0;
    if(HAL_DMA_PollForTransfer(h,HAL_DMA_FULL_TRANSFER,0U)!=HAL_OK)return -1;
    __DSB();dma_active=0U;return 1;
}

/* Cancellation/timeout disables EN once and returns immediately. Only after
 * hardware confirms EN=0 may flags/handle ownership be released. A stuck EN
 * remains quarantined; neither this stream nor the arena is reused. */
int RAMTestPort_DMAStop(void)
{
    DMA_HandleTypeDef *h=&hdma_memtomem_dma2_stream0;
    if(!dma_active)return 1;
    __HAL_DMA_DISABLE(h);
    if(h->Instance->CR&DMA_SxCR_EN)return 0;
    DMA2->LIFCR=DMA_LIFCR_CFEIF0|DMA_LIFCR_CDMEIF0|DMA_LIFCR_CTEIF0|DMA_LIFCR_CHTIF0|DMA_LIFCR_CTCIF0;
    h->State=HAL_DMA_STATE_READY;h->Lock=HAL_UNLOCKED;
    __DSB();dma_active=0U;return 1;
}
