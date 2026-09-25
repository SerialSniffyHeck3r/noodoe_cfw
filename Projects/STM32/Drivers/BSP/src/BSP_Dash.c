#include "BSP_Dash.h"
#include "usart.h"
#include <string.h>

#define RX_SIZE 2048U
static uint8_t rx_byte,rx_ring[RX_SIZE],tx_buffer[260];
static volatile uint32_t producer,consumer,recover_rx,recover_tx;
/* An uncertain DMA stop must retain ownership of tx_buffer until MCU reset.
 * Neither a late TC callback nor a second HAL abort (which may skip DMA after
 * DMAT was cleared by the first attempt) can establish that ownership ended. */
static volatile uint32_t tx_fault_locked;
static uint32_t power_sleeping,power_tx;
volatile BSP_Dash_Diagnostics g_bsp_dash;

/* Preserve the IOC transport settings but initialize RX-only before TE can
 * drive PC12. Generated HAL MSP owns the AF/DMA/NVIC mapping. Restore PC12 to
 * high impedance input until the caller explicitly enables transmission. */
uint32_t BSP_Dash_Init(void)
{
    /* Reinitializing just UART does not prove an old DMA stream has stopped.
     * Refuse that shortcut while the private TX copy is retained for safety. */
    if(tx_fault_locked || g_bsp_dash.tx_busy)return 3U;
    /* Once a new initialization is accepted, an old successful attempt is no
     * longer evidence of readiness. Both HAL-init and RX-arm failures stay
     * unready, and the new RX-only configuration never inherits TX permission. */
    g_bsp_dash.ready=0U;g_bsp_dash.tx_enabled=0U;
    g_bsp_dash.magic=0x44415331U;
    huart5.Instance=UART5; huart5.Init.BaudRate=115200U;
    huart5.Init.WordLength=UART_WORDLENGTH_8B; huart5.Init.StopBits=UART_STOPBITS_1;
    huart5.Init.Parity=UART_PARITY_NONE; huart5.Init.Mode=UART_MODE_RX;
    huart5.Init.HwFlowCtl=UART_HWCONTROL_NONE; huart5.Init.OverSampling=UART_OVERSAMPLING_16;
    if(HAL_UART_Init(&huart5)!=HAL_OK) return 1U;
    GPIO_InitTypeDef pin={0}; pin.Pin=GPIO_PIN_12; pin.Mode=GPIO_MODE_INPUT; pin.Pull=GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC,&pin);
    producer=consumer=0U; recover_rx=recover_tx=0U;
    /* F4 error flags survive a HAL handle reset. Discard pre-init data by the
     * documented SR-then-DR sequence before enabling the new byte receiver. */
    __HAL_UART_CLEAR_OREFLAG(&huart5);
    if(HAL_UART_Receive_IT(&huart5,&rx_byte,1U)!=HAL_OK) return 2U;
    g_bsp_dash.ready=1U; return 0U;
}

/* One ISR producer/one task consumer. Publish data before the producer index;
 * uint32_t aligned indices are atomic on Cortex-M4, including counter wrap. */
void BSP_Dash_OnRxComplete(UART_HandleTypeDef *uart)
{
    if(uart->Instance!=UART5) return;
    uint32_t head=producer;
    if(head-consumer<RX_SIZE) { rx_ring[head&(RX_SIZE-1U)]=rx_byte; __DMB(); producer=head+1U; }
    else ++g_bsp_dash.overflows;
    ++g_bsp_dash.rx_bytes;
    if(HAL_UART_Receive_IT(uart,&rx_byte,1U)!=HAL_OK) recover_rx=1U;
}

/* Copy available bytes without waiting. The I/O task owns all parser calls,
 * so fragmented UART callbacks never act as application packet boundaries. */
uint32_t BSP_Dash_Read(uint8_t *data,uint32_t capacity)
{
    if(!data) return 0U;
    uint32_t count=0U,tail=consumer;
    while(count<capacity && tail!=producer) { __DMB(); data[count++]=rx_ring[tail&(RX_SIZE-1U)]; ++tail; }
    __DMB(); consumer=tail; return count;
}

/* Recovery runs outside ISR; discard partial queued data after UART framing/
 * overrun errors, rearm a fresh byte and let the F5 parser resynchronize. */
void BSP_Dash_Process(void)
{
    /* Claim both pending flags before enabling a new receive. An IRQ can run
     * as soon as HAL_UART_Receive_IT enables RXNEIE; clearing recover_rx after
     * that call would erase a fresh ISR request and leave stopped RX stranded.
     * Mask only the fixed-size flag exchange, never a HAL abort or bus wait. */
    uint32_t irq=__get_PRIMASK();__disable_irq();
    uint32_t rx=recover_rx,tx=recover_tx;recover_rx=recover_tx=0U;
    __DMB();__set_PRIMASK(irq);
    if(!rx && !tx) return;
    /* A DMA error can leave the public TX slot occupied even after HAL changed
     * gState to READY. A dedicated latch survives a subsequent unrelated RX
     * error overwriting last_hal_error. AbortTransmit stops TC/DMAT/its stream. */
    if(g_bsp_dash.tx_busy && tx && !tx_fault_locked) {
        HAL_StatusTypeDef stopped=HAL_UART_AbortTransmit(&huart5);
        /* HAL clears CR3.DMAT before attempting HAL_DMA_Abort. If that abort
         * times out, the stream may still own tx_buffer. A later HAL_UART abort
         * would see DMAT=0 and return OK without touching DMA; do not retry it
         * as proof of recovery. Require success AND actual EN=0 on the bound
         * stream. A missing DMA handle also cannot prove safe buffer release. */
        DMA_HandleTypeDef *dma=huart5.hdmatx;
        if(stopped==HAL_OK && dma && dma->Instance &&
           !(dma->Instance->CR&DMA_SxCR_EN)) {
            g_bsp_dash.tx_last_result=2U;++g_bsp_dash.tx_failed;__DMB();
            g_bsp_dash.tx_busy=0U;
        } else {
            /* Fail-stop only TX: stop further requests, retain the private
             * copy, publish the DMA error, and continue the independent RX
             * recovery below. No busy-looping abort, HAL state forgery, GPIO
             * change, or automatic reset is used. MCU reset clears this latch. */
            tx_fault_locked=1U;g_bsp_dash.tx_busy=1U;g_bsp_dash.tx_enabled=0U;
            g_bsp_dash.tx_last_result=3U;++g_bsp_dash.tx_failed;
            CLEAR_BIT(UART5->CR3,USART_CR3_DMAT);
            g_bsp_dash.last_hal_error=huart5.ErrorCode|HAL_UART_ERROR_DMA;
            ++g_bsp_dash.errors;
        }
    }
    if(rx){
        HAL_UART_AbortReceive(&huart5); consumer=producer;
        /* HAL_F4 AbortReceive disables interrupts but never clears SR/DR.
         * With ORE set and RXNE clear its IRQ handler cannot drain DR, so each
         * rearm repeats the same error forever. Clear PE/FE/NE/ORE via SR then
         * DR while RX interrupts are disabled; the partial frame is already
         * discarded and the parser will synchronize at the next F5 packet. */
        __HAL_UART_CLEAR_OREFLAG(&huart5);
        if(HAL_UART_Receive_IT(&huart5,&rx_byte,1U)==HAL_OK) ++g_bsp_dash.restarts;
        else recover_rx=1U;
    }
}

/* The bench controller arms TX only for recorded/understood requests. Disabling
 * while a frame is in flight is rejected; a wire frame is never truncated. */
uint32_t BSP_Dash_EnableTx(uint32_t enabled)
{
    if(!g_bsp_dash.ready || g_bsp_dash.tx_busy || tx_fault_locked) return 1U;
    GPIO_InitTypeDef pin={0}; pin.Pin=GPIO_PIN_12; pin.Pull=GPIO_NOPULL;
    if(enabled) {
        pin.Mode=GPIO_MODE_AF_PP; pin.Speed=GPIO_SPEED_FREQ_MEDIUM; pin.Alternate=GPIO_AF8_UART5;
        SET_BIT(UART5->CR1,USART_CR1_TE);
    } else { CLEAR_BIT(UART5->CR1,USART_CR1_TE); pin.Mode=GPIO_MODE_INPUT; }
    HAL_GPIO_Init(GPIOC,&pin); g_bsp_dash.tx_enabled=enabled!=0U; return 0U;
}

/* Single bounded task-owned TX slot. HAL DMA keeps this private copy valid
 * until UART TC completion; caller buffers may be immediately reused. */
uint32_t BSP_Dash_Send(const uint8_t *data,uint32_t length)
{
    if(!data || !length || length>sizeof(tx_buffer)) return 1U;
    if(!g_bsp_dash.ready || !g_bsp_dash.tx_enabled || g_bsp_dash.tx_busy || tx_fault_locked) return 2U;
    memcpy(tx_buffer,data,length); g_bsp_dash.tx_busy=1U;
    g_bsp_dash.tx_last_result=0U;
    if(HAL_UART_Transmit_DMA(&huart5,tx_buffer,length)!=HAL_OK) {
        g_bsp_dash.tx_last_result=1U;++g_bsp_dash.tx_failed;g_bsp_dash.tx_busy=0U;return 3U;
    }
    g_bsp_dash.tx_bytes+=length; return 0U;
}
void BSP_Dash_OnTxComplete(UART_HandleTypeDef *uart)
{
    /* A delayed interrupt after a failed abort cannot release retained data. */
    if(uart->Instance==UART5 && !tx_fault_locked && g_bsp_dash.tx_busy) {
        ++g_bsp_dash.tx_completed;g_bsp_dash.tx_last_result=0U;__DMB();g_bsp_dash.tx_busy=0U;
    }
}
void BSP_Dash_OnError(UART_HandleTypeDef *uart)
{
    if(uart->Instance!=UART5) return;
    if(uart->ErrorCode&HAL_UART_ERROR_DMA)recover_tx=1U;
    ++g_bsp_dash.errors; g_bsp_dash.last_hal_error=uart->ErrorCode; recover_rx=1U;
}

void BSP_Dash_RequestTxAbort(void)
{if(g_bsp_dash.tx_busy && !tx_fault_locked)recover_tx=1U;}

void BSP_Dash_ReadLightThresholds(uint32_t out[10])
{
    if(!out)return;
    const volatile uint32_t *source=(const volatile uint32_t *)0x0800C040UL;
    for(uint32_t i=0;i<10U;++i)out[i]=source[i];
}

uint32_t BSP_Dash_SetSleeping(uint32_t sleeping)
{
    if(sleeping){
        if(power_sleeping)return 0;
        if(g_bsp_dash.tx_busy||tx_fault_locked)return 1;
        power_tx=g_bsp_dash.tx_enabled;
        if(HAL_UART_AbortReceive(&huart5)!=HAL_OK)return 2;
        __HAL_UART_DISABLE(&huart5);g_bsp_dash.ready=0;consumer=producer;
        power_sleeping=1;return 0;
    }
    if(!power_sleeping)return 0;
    uint32_t result=BSP_Dash_Init();if(result)return result;
    if(power_tx&&(result=BSP_Dash_EnableTx(1)))return result;
    power_sleeping=0;return 0;
}
