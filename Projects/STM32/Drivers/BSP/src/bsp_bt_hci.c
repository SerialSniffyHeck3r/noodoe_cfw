#include "BSP_RAM.h"
#include "BSP_BT_HCI.h"
#include "usart.h"
#include "FreeRTOS.h"
#include "task.h"

volatile BSP_BT_HCI_Diagnostics g_bsp_bt_hci = { .magic=0x42484831U, .version=1U };
/* Large diagnostic payloads start in BSS; headers are published by their owner.
 * This saves zero-filled flash initializers without reducing record capacity. */
volatile BSP_BT_HCI_FaultSnapshot g_bsp_bt_hci_fault;
volatile BSP_BT_HCI_StartupTrace g_bsp_bt_hci_startup;
static uint32_t fault_latched;
static volatile uint32_t events;
static void (*wake_from_isr)(void);
static uint16_t tx_size, rx_size;
static uint8_t reset_pending;
static volatile uint32_t transport_paused;
static uint32_t dma_quarantined;

/* BT owner publication only. No DR reads or flag clears occur here; the
 * bounded samples preserve digital observations even after Close. */
static void StartupSample(uint32_t stage,uint32_t flags)
{
    uint32_t seq=(g_bsp_bt_hci_startup.sequence+1U)|1U;
    g_bsp_bt_hci_startup.sequence=seq;__DMB();
    uint32_t n=g_bsp_bt_hci_startup.sample_count;
    if(n<BSP_BT_HCI_STARTUP_SAMPLES){
        volatile BSP_BT_HCI_StartupSample *s=&g_bsp_bt_hci_startup.samples[n];
        s->tick_ms=HAL_GetTick();s->stage=stage;
        s->gpioa_idr=GPIOA->IDR;s->gpioi_idr=GPIOI->IDR;
        s->uart_sr=USART1->SR;
        s->rx_remaining=DMA2_Stream5->NDTR;s->tx_remaining=DMA2_Stream7->NDTR;
        g_bsp_bt_hci_startup.sample_count=n+1U;
    }
    g_bsp_bt_hci_startup.flags|=flags;
    __DMB();g_bsp_bt_hci_startup.sequence=seq+1U;
}

static void StartupBegin(void)
{
    uint32_t seq=(g_bsp_bt_hci_startup.sequence+1U)|1U;
    g_bsp_bt_hci_startup.sequence=seq;__DMB();
    g_bsp_bt_hci_startup.magic=0x42545431U;g_bsp_bt_hci_startup.version=1U;
    /* Preserve a previous fault payload while initializing headers on first Open. */
    g_bsp_bt_hci_fault.magic=0x42464631U;g_bsp_bt_hci_fault.version=1U;
    ++g_bsp_bt_hci_startup.attempt;
    g_bsp_bt_hci_startup.sample_count=0U;g_bsp_bt_hci_startup.flags=0U;
    __DMB();g_bsp_bt_hci_startup.sequence=seq+1U;
    StartupSample(1U,0U);
}

/* USART hardware RTS follows the data register, not our DMA block lifecycle.
 * Hold PA12 HIGH between H4 blocks. One already-in-flight byte may remain in
 * DR; the next DMA must consume it without an SR/DR clear sequence. */
static void HoldRTS(void)
{
    GPIOA->BSRR=GPIO_PIN_12;
    MODIFY_REG(GPIOA->MODER,3UL<<(12U*2U),1UL<<(12U*2U));
}
static void ReleaseRTS(void)
{ MODIFY_REG(GPIOA->MODER,3UL<<(12U*2U),2UL<<(12U*2U)); }

/* Normal DMA completion mirrors the HAL state transition, but asserts RTS
 * immediately in IRQ context. Root's HAL callback dispatch remains shared. */
static void RxDMAComplete(DMA_HandleTypeDef *dma)
{
    (void)dma;
    HoldRTS();
    CLEAR_BIT(huart1.Instance->CR3,USART_CR3_DMAR|USART_CR3_EIE);
    huart1.RxXferCount=0U; huart1.RxState=HAL_UART_STATE_READY;
    HAL_UART_RxCpltCallback(&huart1);
}
static void RxDMAError(DMA_HandleTypeDef *dma)
{
    (void)dma;
    HoldRTS(); CLEAR_BIT(huart1.Instance->CR3,USART_CR3_DMAR|USART_CR3_EIE);
    huart1.RxState=HAL_UART_STATE_READY; huart1.ErrorCode|=HAL_UART_ERROR_DMA;
    HAL_UART_ErrorCallback(&huart1);
}

/* IRQ priorities are at or below the FreeRTOS syscall ceiling. This hook
 * only wakes the BT task; actual H4 packet processing occurs in that task. */
static void Wake(void) { if (wake_from_isr != NULL) wake_from_isr(); }

/* HAL abort may return on a TX timeout before attempting RX. Never treat
 * that result, or a stale HAL READY state, as proof that DMA released memory.
 * Block new requests first; leave the original buffers/busy flags owned when
 * either stream remains enabled. No peripheral-wide DMA reset is permitted. */
static int StopDMA(void)
{
    HAL_StatusTypeDef result;
    if(dma_quarantined)return -1;
    /* First Open owns HAL/MSP initialization. Never pass a zero-initialized
     * handle to Abort; an inherited active stream still requires quarantine. */
    result=(huart1.Instance==USART1)?HAL_UART_Abort(&huart1):HAL_OK;
    if(result==HAL_OK && !(DMA2_Stream5->CR&DMA_SxCR_EN)
       && !(DMA2_Stream7->CR&DMA_SxCR_EN))return 0;
    BSP_BT_HCI_CaptureFault(0x105U);
    dma_quarantined=1U;transport_paused=1U;
    g_bsp_bt_hci.errors++;g_bsp_bt_hci.last_hal_error=HAL_UART_ERROR_DMA;
    /* Only after HAL abort: clearing these bits beforehand makes HAL skip
     * its DMA abort paths. A disable request is not a completion assertion. */
    CLEAR_BIT(USART1->CR3,USART_CR3_DMAT|USART_CR3_DMAR|USART_CR3_EIE);
    CLEAR_BIT(DMA2_Stream5->CR,DMA_SxCR_EN);
    CLEAR_BIT(DMA2_Stream7->CR,DMA_SxCR_EN);
    return -1;
}

/* Stock MSP deinit 0x08037C30..4E resets USART1 via APB2RSTR bit4.
 * Keep the already initialized DMA/MSP handles; invoking generated MSP init
 * again would call Error_Handler on DMA-init failure. UART Init below restores
 * the USART configuration, while no other APB/DMA peripheral is reset. */
static void ResetUSART(void)
{
    __HAL_RCC_USART1_FORCE_RESET();__DSB();
    __HAL_RCC_USART1_RELEASE_RESET();__DSB();
}

int BSP_BT_HCI_Open(void)
{
    GPIO_InitTypeDef io = {0};
    if(dma_quarantined || g_bsp_bt_hci.opened)return -1;
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOI_CLK_ENABLE();
    fault_latched=0U;
    StartupBegin();
    if(StopDMA())return -1;
    g_bsp_bt_hci.opened=0U; reset_pending=0U; transport_paused=0U;fault_latched=0U;
    /* Preload LOW before changing an inherited mux, then establish reset as
     * an output before raising enable. Stock 0x0803E82A/3C raises PI1 before
     * its first UART/MSP init; Runtime must not drive TX/RTS before this. */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);
    io.Pin=GPIO_PIN_8; io.Mode=GPIO_MODE_OUTPUT_PP;
    io.Pull=GPIO_NOPULL; io.Speed=GPIO_SPEED_FREQ_MEDIUM;
    HAL_GPIO_Init(GPIOA,&io);
    StartupSample(2U,0U);
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_1, GPIO_PIN_SET);
    io.Pin=GPIO_PIN_1; io.Speed=GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOI,&io);
    StartupSample(3U,0U);
    __HAL_RCC_USART1_CLK_ENABLE();
    ResetUSART();
    huart1.Instance=USART1;
    huart1.Init.BaudRate=115200U;
    huart1.Init.WordLength=UART_WORDLENGTH_8B;
    huart1.Init.StopBits=UART_STOPBITS_1;
    huart1.Init.Parity=UART_PARITY_NONE;
    huart1.Init.Mode=UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl=UART_HWCONTROL_RTS_CTS;
    huart1.Init.OverSampling=UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1)!=HAL_OK) return -1;
    HoldRTS();
    StartupSample(4U,0U);
    /* DMA TX completion switches to USART TC interrupt in STM32 HAL. */
    HAL_NVIC_SetPriority(USART1_IRQn,5U,0U);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
    events=0U; g_bsp_bt_hci.rx_busy=0U; g_bsp_bt_hci.tx_busy=0U;
    /* Stock 0x0803E924 arms RX before 0x0803E954 releases PA8. H4 calls
     * receive_block immediately after uart.open returns. Keep reset asserted
     * here until that first DMA is armed, instead of booting an unserved RX.
     * Clear only stale bytes while the controller is still held in reset. */
    __HAL_UART_CLEAR_OREFLAG(&huart1);
    g_bsp_bt_hci.baud=115200U;
    reset_pending=1U; g_bsp_bt_hci.opened=1U;
    return 0;
}

/* Read-only hardware evidence must precede HAL abort and PA8/PI1 shutdown.
 * Latch one failure so follow-on power-off errors cannot overwrite its cause.
 * DMA stream registers are observed without clearing their status flags. */
void BSP_BT_HCI_CaptureFault(uint32_t reason)
{
    if(fault_latched)return;
    fault_latched=1U;
    if(g_bsp_bt_hci_startup.attempt)StartupSample(8U,2U);
    uint32_t sequence=(g_bsp_bt_hci_fault.sequence+1U)|1U;
    g_bsp_bt_hci_fault.sequence=sequence;__DMB();
    g_bsp_bt_hci_fault.magic=0x42464631U;g_bsp_bt_hci_fault.version=1U;
    g_bsp_bt_hci_fault.reason=reason;g_bsp_bt_hci_fault.tick=HAL_GetTick();
    g_bsp_bt_hci_fault.transport=g_bsp_bt_hci;
    g_bsp_bt_hci_fault.uart_sr=USART1->SR;g_bsp_bt_hci_fault.uart_brr=USART1->BRR;
    g_bsp_bt_hci_fault.uart_cr1=USART1->CR1;g_bsp_bt_hci_fault.uart_cr2=USART1->CR2;
    g_bsp_bt_hci_fault.uart_cr3=USART1->CR3;
    g_bsp_bt_hci_fault.gpioa_moder=GPIOA->MODER;g_bsp_bt_hci_fault.gpioa_idr=GPIOA->IDR;
    g_bsp_bt_hci_fault.gpioa_odr=GPIOA->ODR;g_bsp_bt_hci_fault.gpioa_afr0=GPIOA->AFR[0];
    g_bsp_bt_hci_fault.gpioa_afr1=GPIOA->AFR[1];
    g_bsp_bt_hci_fault.gpioi_moder=GPIOI->MODER;g_bsp_bt_hci_fault.gpioi_idr=GPIOI->IDR;
    g_bsp_bt_hci_fault.gpioi_odr=GPIOI->ODR;
    const volatile uint32_t *rx=(const volatile uint32_t*)DMA2_Stream5;
    const volatile uint32_t *tx=(const volatile uint32_t*)DMA2_Stream7;
    for(unsigned i=0;i<6U;i++){g_bsp_bt_hci_fault.dma_rx[i]=rx[i];g_bsp_bt_hci_fault.dma_tx[i]=tx[i];}
    __DMB();g_bsp_bt_hci_fault.sequence=sequence+1U;
}

/* Close affects only BT reset and its USART, never the Noodoe power latch. */
int BSP_BT_HCI_Close(void)
{
    /* Before first Open, no UART buffers or GPIO mux belong to this driver.
     * An inherited-DMA rejection still retains its quarantine result. */
    if(huart1.Instance!=USART1)return dma_quarantined?-1:0;
    g_bsp_bt_hci.opened=0U;
    HoldRTS();
    int result=StopDMA();
    if(!result)ResetUSART();
    HAL_GPIO_WritePin(GPIOA,GPIO_PIN_8,GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOI,GPIO_PIN_1,GPIO_PIN_RESET);
    if(g_bsp_bt_hci_startup.attempt)StartupSample(9U,4U);
    if(result)return result;
    g_bsp_bt_hci.tx_busy=0U; g_bsp_bt_hci.rx_busy=0U; events=0U; reset_pending=0U; transport_paused=0U;
    return 0;
}
uint32_t BSP_BT_HCI_IsQuarantined(void){return dma_quarantined;}

int BSP_BT_HCI_SetBaud(uint32_t baud)
{
    if (!g_bsp_bt_hci.opened || huart1.Instance!=USART1 ||
        (baud!=115200U && baud!=230400U && baud!=921600U && baud!=3686400U)
        || g_bsp_bt_hci.tx_busy) return -1;
    /* UE stays enabled, TX idle stays HIGH and RX DMA is not aborted. The
     * H4 transport calls this between baud-complete and the next command. */
    huart1.Instance->BRR=UART_BRR_SAMPLING16(HAL_RCC_GetPCLK2Freq(),baud);
    huart1.Init.BaudRate=baud; g_bsp_bt_hci.baud=baud;
    return 0;
}

int BSP_BT_HCI_SetFlowControl(int enabled)
{
    if(!g_bsp_bt_hci.opened || huart1.Instance!=USART1)return -1;
    /* TI FF36 may hold CTS inactive until it receives the command. BTstack's
     * CC256x workaround temporarily disables CTSE only; RTS remains active. */
    if (enabled) SET_BIT(huart1.Instance->CR3,USART_CR3_CTSE);
    else { CLEAR_BIT(huart1.Instance->CR3,USART_CR3_CTSE); g_bsp_bt_hci.cts_bypasses++; }
    return 0;
}

int BSP_BT_HCI_Receive(uint8_t *data,uint16_t size)
{
    if(!BSP_RAM_DMAAccessible(data,size,1))return -1;
    if (!g_bsp_bt_hci.opened || data==NULL || size==0U || g_bsp_bt_hci.rx_busy) return -1;
    /* HAL_UART_Receive_DMA unconditionally clears ORE through SR/DR reads.
     * That silently discards a valid byte arriving between normal DMA blocks.
     * Keep the package unchanged and own this small no-discard DMA arm path. */
    if (huart1.Instance->SR&(USART_SR_ORE|USART_SR_FE|USART_SR_NE)) {
        g_bsp_bt_hci.errors++; g_bsp_bt_hci.last_hal_error=huart1.Instance->SR; return -1;
    }
    rx_size=size; g_bsp_bt_hci.rx_busy=1U;
    huart1.pRxBuffPtr=data; huart1.RxXferSize=size; huart1.RxXferCount=size;
    huart1.ErrorCode=HAL_UART_ERROR_NONE; huart1.RxState=HAL_UART_STATE_BUSY_RX;
    huart1.hdmarx->XferCpltCallback=RxDMAComplete;
    huart1.hdmarx->XferHalfCpltCallback=NULL;
    huart1.hdmarx->XferErrorCallback=RxDMAError;
    huart1.hdmarx->XferAbortCallback=NULL;
    if (HAL_DMA_Start_IT(huart1.hdmarx,(uint32_t)&huart1.Instance->DR,(uint32_t)data,size)!=HAL_OK) {
        huart1.RxState=HAL_UART_STATE_READY;
        g_bsp_bt_hci.rx_busy=0U; g_bsp_bt_hci.errors++; return -1;
    }
    taskENTER_CRITICAL();
    SET_BIT(huart1.Instance->CR3,USART_CR3_DMAR|USART_CR3_EIE);
    /* The DMA can now drain a pending DR byte before the peer is released. */
    if (!transport_paused) ReleaseRTS();
    taskEXIT_CRITICAL();
    if (reset_pending) {
        /* RX DMA and host RTS are ready before the controller leaves reset.
         * Delay in task context so UI/IO tasks remain scheduled during boot.
         * This first receive returns only after the stock 150 ms interval;
         * H4 therefore cannot submit its first HCI Reset too early. */
        StartupSample(5U,0U);
        vTaskDelay(pdMS_TO_TICKS(10U));
        reset_pending=0U;
        HAL_GPIO_WritePin(GPIOA,GPIO_PIN_8,GPIO_PIN_SET);
        g_bsp_bt_hci.reset_count++;
        StartupSample(6U,0U);
        /* Keep the original 150 RTOS ticks at the project's 1kHz tick rate.
         * Absolute task deadlines avoid adding eight scheduler delays. IRQs
         * stay enabled; late samples record their actual HAL tick timestamp. */
        static const uint8_t offsets[]={1U,2U,5U,10U,20U,50U,100U,150U};
        TickType_t deadline=xTaskGetTickCount();
        uint32_t previous=0U;
        for(unsigned i=0U;i<sizeof(offsets);i++){
            vTaskDelayUntil(&deadline,pdMS_TO_TICKS(offsets[i]-previous));
            previous=offsets[i];
            StartupSample(7U,i+1U==sizeof(offsets)?1U:0U);
        }
    }
    return 0;
}

int BSP_BT_HCI_Send(const uint8_t *data,uint16_t size)
{
    if(!BSP_RAM_DMAAccessible(data,size,0))return -1;
    if (!g_bsp_bt_hci.opened || reset_pending || transport_paused || data==NULL || size==0U || g_bsp_bt_hci.tx_busy) return -1;
    tx_size=size; g_bsp_bt_hci.tx_busy=1U;
    if (HAL_UART_Transmit_DMA(&huart1,(uint8_t *)data,size)!=HAL_OK) {
        g_bsp_bt_hci.tx_busy=0U; g_bsp_bt_hci.errors++; return -1;
    }
    __HAL_DMA_DISABLE_IT(huart1.hdmatx,DMA_IT_HT);
    return 0;
}

/* IRQ exclusion makes pause publication and RTS mux change indivisible from
 * normal DMA completion. Resume releases RTS only if a receive is armed. */
void BSP_BT_HCI_SetPaused(uint32_t paused)
{
    if(!g_bsp_bt_hci.opened || huart1.Instance!=USART1)return;
    taskENTER_CRITICAL();
    transport_paused=!!paused;
    if (transport_paused || !g_bsp_bt_hci.opened || !g_bsp_bt_hci.rx_busy) HoldRTS();
    else ReleaseRTS();
    taskEXIT_CRITICAL();
}
uint32_t BSP_BT_HCI_IsPaused(void) { return transport_paused; }
uint32_t BSP_BT_HCI_IsQuiescent(void)
{
    if(!g_bsp_bt_hci.opened || huart1.Instance!=USART1)return 0U;
    uint32_t sr=huart1.Instance->SR;
    return g_bsp_bt_hci.opened && transport_paused && !g_bsp_bt_hci.tx_busy
        && (sr&USART_SR_TC) && !(sr&(USART_SR_RXNE|USART_SR_ORE|USART_SR_FE|USART_SR_NE));
}

void BSP_BT_HCI_OnRxComplete(UART_HandleTypeDef *uart)
{
    if (uart!=&huart1 || !g_bsp_bt_hci.opened) return;
    g_bsp_bt_hci.rx_busy=0U; g_bsp_bt_hci.rx_blocks++;
    g_bsp_bt_hci.rx_bytes+=rx_size; g_bsp_bt_hci.rx_complete++;
    events|=1U; Wake();
}
void BSP_BT_HCI_OnTxComplete(UART_HandleTypeDef *uart)
{
    if (uart!=&huart1 || !g_bsp_bt_hci.opened) return;
    g_bsp_bt_hci.tx_busy=0U; g_bsp_bt_hci.tx_blocks++;
    g_bsp_bt_hci.tx_bytes+=tx_size; g_bsp_bt_hci.tx_complete++;
    events|=2U; Wake();
}
void BSP_BT_HCI_OnError(UART_HandleTypeDef *uart)
{
    if (uart!=&huart1 || !g_bsp_bt_hci.opened) return;
    g_bsp_bt_hci.errors++; g_bsp_bt_hci.last_hal_error=uart->ErrorCode;
    events|=4U; Wake();
}
void BSP_BT_HCI_SetWakeFromISR(void (*callback)(void)) { wake_from_isr=callback; }
uint32_t BSP_BT_HCI_TakeEvents(void)
{
    uint32_t result;
    taskENTER_CRITICAL(); result=events; events=0U; taskEXIT_CRITICAL();
    return result;
}
const volatile BSP_BT_HCI_Diagnostics *BSP_BT_HCI_GetDiagnostics(void) { return &g_bsp_bt_hci; }
