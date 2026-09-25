#include "BSP_BT_ResetDiagnostic.h"
#include "BSP_BT_HCI.h"
#include "usart.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

/* The service initializes headers before requests; the large payload is BSS. */
volatile BSP_BT_ResetDiagnostic g_bsp_bt_reset_diagnostic;
_Static_assert(sizeof(BSP_BT_ResetDiagnostic)==192U,"Raw Reset diagnostic ABI");
static uint8_t raw_rx[64];
static const uint8_t reset_command[4]={0x01U,0x03U,0x0cU,0x00U};
static uint32_t active;

/* The only writer is the BT owner. Publish short, coherent records without
 * holding IRQ masks while waiting or copying DMA data. */
static void Publish(BSP_BT_ResetDiagnostic *record,uint32_t phase)
{
    uint32_t seq=(g_bsp_bt_reset_diagnostic.sequence+1U)|1U;
    record->sequence=seq;record->phase=phase;
    record->elapsed_ms=HAL_GetTick()-record->started_ms;
    g_bsp_bt_reset_diagnostic.sequence=seq;__DMB();
    g_bsp_bt_reset_diagnostic=*record;
    __DMB();g_bsp_bt_reset_diagnostic.sequence=seq+1U;
}

/* Accept a complete H4 Command Complete for opcode0x0C03; command credits may
 * be any value. Bytes before the event are retained rather than discarded.
 * Reset's nonzero status proves reception but is not a successful Reset. */
static uint32_t Parse(BSP_BT_ResetDiagnostic *r)
{
    if(!r->rx_captured_bytes)return BSP_BT_RESET_RX_TIMEOUT;
    for(uint32_t i=0;i+7U<=r->rx_captured_bytes;i++){
        const uint8_t *p=r->raw_data+i;
        if(p[0]==4U && p[1]==0x0eU && p[2]==4U && p[4]==3U && p[5]==0x0cU){
            r->reset_status=p[6];return p[6]?BSP_BT_RESET_STATUS:BSP_BT_RESET_OK;
        }
    }
    return BSP_BT_RESET_INVALID_RESPONSE;
}

int BSP_BT_ResetDiagnosticRun(uint32_t operation_id)
{
    /* A reentrant/ISR call must not replace the active/previous evidence.
     * The service adds HCI/run-loop ownership checks before reaching here. */
    if(!operation_id || __get_IPSR() || __get_PRIMASK() || __get_BASEPRI() || __get_FAULTMASK())
        return BSP_BT_RESET_CONTEXT;
    taskENTER_CRITICAL();
    if(active){taskEXIT_CRITICAL();return BSP_BT_RESET_NOT_READY;}
    active=1U;taskEXIT_CRITICAL();
    BSP_BT_ResetDiagnostic r={.magic=0x42524431U,.version=1U,.operation_id=operation_id,
        .request_sequence=operation_id,.started_ms=HAL_GetTick(),.reset_status=UINT32_MAX};
    uint32_t result=BSP_BT_RESET_OK,opened=0U,flow_changed=0U,rx_armed=0U;
    uint32_t error_baseline=g_bsp_bt_hci.errors;
    Publish(&r,1U);
    if(g_bsp_bt_hci.opened || BSP_BT_HCI_IsQuarantined()){
        result=BSP_BT_RESET_NOT_READY;goto finish;
    }
    memset(raw_rx,0,sizeof(raw_rx));
    /* H4 is detached by the service, so IRQ completion can never send a
     * follow-on HCI command. Keep the normal proven PA8/PI1 lifecycle. */
    BSP_BT_HCI_SetWakeFromISR(NULL);
    opened=1U; /* Failed partial Open still needs the known cleanup path. */
    if(BSP_BT_HCI_Open()){result=BSP_BT_RESET_OPEN;goto cleanup;}
    r.uart_brr=USART1->BRR;
    Publish(&r,2U);
    if(BSP_BT_HCI_Receive(raw_rx,sizeof(raw_rx))){result=BSP_BT_RESET_RX_ARM;goto cleanup;}
    rx_armed=1U;
    r.cr3_before=USART1->CR3;
    if(!(r.cr3_before&USART_CR3_CTSE) || !(r.cr3_before&USART_CR3_RTSE)){
        result=BSP_BT_RESET_FLOW;goto cleanup;
    }
    /* The sole deliberate deviation: temporarily ignore host CTS, keeping
     * host RTS active. Do not remux or drive the controller's RTS pin. */
    flow_changed=1U;
    if(BSP_BT_HCI_SetFlowControl(0)){result=BSP_BT_RESET_FLOW;goto cleanup;}
    r.cr3_bypass=USART1->CR3;
    if((r.cr3_bypass&USART_CR3_CTSE) || !(r.cr3_bypass&USART_CR3_RTSE)){
        result=BSP_BT_RESET_FLOW;goto cleanup;
    }
    Publish(&r,3U);
    r.tx_submit_count=1U;r.tx_requested_bytes=sizeof(reset_command);
    if(BSP_BT_HCI_Send(reset_command,sizeof(reset_command))){result=BSP_BT_RESET_TX_SUBMIT;goto cleanup;}
    uint32_t sent_at=HAL_GetTick();
    /* At115200, four bytes take<0.4ms.20ms allows task/IRQ latency but does
     * not resend or treat a callback alone as physical transfer completion. */
    while(DMA2_Stream7->NDTR || !(USART1->SR&USART_SR_TC)){
        if(g_bsp_bt_hci.errors!=error_baseline){result=BSP_BT_RESET_UART_ERROR;goto cleanup;}
        if(HAL_GetTick()-sent_at>=20U){result=BSP_BT_RESET_TX_TIMEOUT;goto cleanup;}
        vTaskDelay(pdMS_TO_TICKS(1U));
    }
    r.tx_completed_bytes=4U;
    /* End the CTS override as soon as that single transfer completes. */
    (void)BSP_BT_HCI_SetFlowControl((r.cr3_before&USART_CR3_CTSE)!=0U);
    r.cr3_restored=USART1->CR3;
    r.flow_restore=((r.cr3_restored^r.cr3_before)&USART_CR3_CTSE)?2U:1U;
    if(r.flow_restore!=1U){result=BSP_BT_RESET_FLOW;goto cleanup;}
    flow_changed=0U;
    Publish(&r,4U);
    /* Raw64-byte Normal DMA owns its buffer for one bounded500ms window.
     * No H4 parser is invoked, even if the controller responds successfully. */
    while(HAL_GetTick()-sent_at<500U && DMA2_Stream5->NDTR){
        if(g_bsp_bt_hci.errors!=error_baseline || (USART1->SR&(USART_SR_ORE|USART_SR_FE|USART_SR_NE))){
            result=BSP_BT_RESET_UART_ERROR;break;
        }
        vTaskDelay(pdMS_TO_TICKS(1U));
    }
cleanup:
    /* A completion/timeout can make either loop condition false before its
     * body checks an error arriving with the final byte. Freeze a final SR
     * sample and fresh error counter before any cleanup changes registers. */
    r.uart_sr=USART1->SR;
    if(result==BSP_BT_RESET_OK && (g_bsp_bt_hci.errors!=error_baseline ||
       (r.uart_sr&(USART_SR_ORE|USART_SR_FE|USART_SR_NE))))result=BSP_BT_RESET_UART_ERROR;
    r.primary_result=result;
    if(flow_changed){
        (void)BSP_BT_HCI_SetFlowControl((r.cr3_before&USART_CR3_CTSE)!=0U);
        r.cr3_restored=USART1->CR3;
        r.flow_restore=((r.cr3_restored^r.cr3_before)&USART_CR3_CTSE)?2U:1U;
    }
    /* HAL ErrorCode belongs to this Open/Receive attempt. The cumulative
     * BSP last-error field may still describe an earlier failed operation. */
    r.hal_error=g_bsp_bt_hci.errors!=error_baseline?huart1.ErrorCode:0U;
    r.dma_rx_cr=DMA2_Stream5->CR;r.dma_tx_cr=DMA2_Stream7->CR;
    r.dma_rx_remaining=DMA2_Stream5->NDTR;r.dma_tx_remaining=DMA2_Stream7->NDTR;
    r.gpioa_idr=GPIOA->IDR;r.gpioi_idr=GPIOI->IDR;
    Publish(&r,5U);
    r.close_result=opened?BSP_BT_HCI_Close():0;
    if(r.close_result || BSP_BT_HCI_IsQuarantined())result=BSP_BT_RESET_CLEANUP;
    else if(rx_armed){
        /* HAL Abort preserves NDTR. Read it only after both EN bits were
         * verified zero, then copy bytes that DMA actually committed. */
        __DMB();r.dma_rx_remaining=DMA2_Stream5->NDTR;r.dma_tx_remaining=DMA2_Stream7->NDTR;
        if(r.dma_rx_remaining<=sizeof(raw_rx)){
            r.rx_captured_bytes=sizeof(raw_rx)-r.dma_rx_remaining;
            memcpy(r.raw_data,raw_rx,r.rx_captured_bytes);r.data_valid=1U;
            if(result==BSP_BT_RESET_OK)result=Parse(&r);
        }else result=BSP_BT_RESET_INVALID_RESPONSE;
    }
    if(!r.close_result && result!=BSP_BT_RESET_CLEANUP)r.primary_result=result;
    if(r.flow_restore==2U && result!=BSP_BT_RESET_CLEANUP)result=BSP_BT_RESET_FLOW;
finish:
    if(!opened)r.primary_result=result;
    r.result=result;Publish(&r,6U);
    active=0U;return (int)result;
}
