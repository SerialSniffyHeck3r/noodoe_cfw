#ifndef BSP_BT_RESET_DIAGNOSTIC_H
#define BSP_BT_RESET_DIAGNOSTIC_H
#include <stdint.h>

/* Isolated volatile-controller Reset experiment, not firmware/NVM programming.
 * No arbitrary payload, baud, pin or timeout is accepted. The BT owner must
 * have detached H4 and established service OFF/FAULT and HCI OFF beforehand.
 * Task context only: Open's 10+150ms plus <=500ms RX window, yielding to RTOS.
 * Static DMA buffers remain owned forever if Close quarantines failed aborts.
 */
typedef enum {
    BSP_BT_RESET_OK=0, BSP_BT_RESET_CONTEXT=1, BSP_BT_RESET_NOT_READY=2,
    BSP_BT_RESET_OPEN=3, BSP_BT_RESET_RX_ARM=4, BSP_BT_RESET_TX_SUBMIT=5,
    BSP_BT_RESET_TX_TIMEOUT=6, BSP_BT_RESET_RX_TIMEOUT=7,
    BSP_BT_RESET_INVALID_RESPONSE=8, BSP_BT_RESET_UART_ERROR=9,
    BSP_BT_RESET_STATUS=10, BSP_BT_RESET_CLEANUP=11, BSP_BT_RESET_FLOW=12
} BSP_BT_ResetResult;
/* Stable SWD ABI: equal even sequence before/after reading all192bytes.
 * phase:0 idle,1 opening,2 RX arm,3 TX,4 RX wait,5 cleanup,6 completed.
 * flow_restore:0 no override,1 prior CTSE verified restored,2 failed.
 * primary_result precedes cleanup; result includes cleanup/restore failure.
 * SR/GPIO/DMA CR are pre-Close observations, NDTR/counts are final only when
 * data_valid=1. No DR reads. raw_data is copied only after DMA EN=0 and a
 * successful Close. reset_status is UINT32_MAX without a matching event.
 * tx_submit_count counts HAL/BSP send attempts, not presumed wire delivery.
 * tx_completed_bytes=4 requires NDTR0 and USART TC before Close.
 */
typedef struct {
    uint32_t magic,version,sequence,operation_id,request_sequence,phase,result,primary_result;
    uint32_t started_ms,elapsed_ms,uart_brr,cr3_before,cr3_bypass,cr3_restored;
    uint32_t tx_submit_count,tx_requested_bytes,tx_completed_bytes,rx_captured_bytes;
    uint32_t hal_error,uart_sr,dma_rx_cr,dma_tx_cr,dma_rx_remaining,dma_tx_remaining;
    uint32_t gpioa_idr,gpioi_idr;
    int32_t close_result;
    uint32_t flow_restore,data_valid,reset_status,reserved0,reserved1;
    uint8_t raw_data[64];
} BSP_BT_ResetDiagnostic;
extern volatile BSP_BT_ResetDiagnostic g_bsp_bt_reset_diagnostic;
int BSP_BT_ResetDiagnosticRun(uint32_t operation_id);
#endif
