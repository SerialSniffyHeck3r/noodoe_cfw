#ifndef BSP_BT_HCI_H
#define BSP_BT_HCI_H
#include "stm32f4xx_hal.h"
#include <stdint.h>

/* USART1 owns PA9/10/11/12 and DMA2 S5/S7. Buffers must remain in DMA-visible
 * SRAM until completion. Only the Bluetooth task starts transfers. */
typedef struct {
    uint32_t magic, version, opened, baud;
    uint32_t tx_blocks, rx_blocks, tx_bytes, rx_bytes;
    uint32_t errors, last_hal_error, tx_busy, rx_busy;
    uint32_t reset_count, cts_bypasses, rx_complete, tx_complete;
} BSP_BT_HCI_Diagnostics;
extern volatile BSP_BT_HCI_Diagnostics g_bsp_bt_hci;
/* First owner-observed fault per Open attempt, captured before Close alters
 * pins/UART. This separate record preserves the existing diagnostics ABI.
 * No USART DR reads; sequence brackets a coherent SWD snapshot. */
typedef struct {
    uint32_t magic,version,sequence,reason,tick;
    BSP_BT_HCI_Diagnostics transport;
    uint32_t uart_sr,uart_brr,uart_cr1,uart_cr2,uart_cr3;
    uint32_t gpioa_moder,gpioa_idr,gpioa_odr,gpioa_afr0,gpioa_afr1;
    uint32_t gpioi_moder,gpioi_idr,gpioi_odr;
    uint32_t dma_rx[6],dma_tx[6];
} BSP_BT_HCI_FaultSnapshot;
extern volatile BSP_BT_HCI_FaultSnapshot g_bsp_bt_hci_fault;
void BSP_BT_HCI_CaptureFault(uint32_t reason); /* Bluetooth owner only */
/* Separate startup ABI: task-time digital samples, not a measured waveform.
 * Read an unchanged even sequence around the record; only sample_count entries
 * belong to attempt. Relative timing is obtained from each sample's tick_ms.
 * Stages1..6 mark entry/reset/enable/UART/RX/release. Stage7 samples requested
 * release offsets 1,2,5,10,20,50,100,150ms; scheduling may run later.
 * Flags: bit0 release-delay completed, bit1 fault seen, bit2 transport closed.
 * Stage8 is a fault sample; stage9 is after the known shutdown GPIO writes. */
#define BSP_BT_HCI_STARTUP_SAMPLES 16U
typedef struct {
    uint32_t tick_ms,stage,gpioa_idr,gpioi_idr,uart_sr,rx_remaining,tx_remaining;
} BSP_BT_HCI_StartupSample;
typedef struct {
    uint32_t magic,version,sequence,attempt,sample_count,flags;
    BSP_BT_HCI_StartupSample samples[BSP_BT_HCI_STARTUP_SAMPLES];
} BSP_BT_HCI_StartupTrace;
extern volatile BSP_BT_HCI_StartupTrace g_bsp_bt_hci_startup;
/* Task context; initialize UART while keeping controller reset asserted.
 * Owns first UART/MSP initialization after PA8 LOW then PI1 HIGH; upper layers
 * must not call MX_USART1_UART_Init before this lifecycle starts.
 * The first Receive arms RX DMA, yields 10 ms, releases reset and yields 150 ms.
 * Send is rejected until that first receive has released the controller. */
int BSP_BT_HCI_Open(void);
/* Zero only after both DMA streams are observed disabled. Failed aborts
 * quarantine static buffers until MCU reset; Open cannot bypass this latch. */
int BSP_BT_HCI_Close(void);
uint32_t BSP_BT_HCI_IsQuarantined(void);
/* Changes BRR while retaining an armed RX DMA. Call after the controller's
 * baud command completed, never while transmitting. Returns zero on success. */
int BSP_BT_HCI_SetBaud(uint32_t baud);
int BSP_BT_HCI_SetFlowControl(int enabled);
int BSP_BT_HCI_Receive(uint8_t *data, uint16_t size);
int BSP_BT_HCI_Send(const uint8_t *data, uint16_t size);
/* Bluetooth owner only. Pause keeps RTS HIGH across every DMA rearm and
 * rejects new DMA TX. Existing RX/TX continue so in-flight bytes can drain.
 * Quiescent additionally requires TX idle and no pending/error UART byte. */
void BSP_BT_HCI_SetPaused(uint32_t paused);
uint32_t BSP_BT_HCI_IsPaused(void);
uint32_t BSP_BT_HCI_IsQuiescent(void);
/* ISR forwarding only records completion/errors and wakes the owner. It does
 * not call Bluetooth protocol handlers, storage or graphics code. */
void BSP_BT_HCI_OnRxComplete(UART_HandleTypeDef *uart);
void BSP_BT_HCI_OnTxComplete(UART_HandleTypeDef *uart);
void BSP_BT_HCI_OnError(UART_HandleTypeDef *uart);
void BSP_BT_HCI_SetWakeFromISR(void (*callback)(void));
uint32_t BSP_BT_HCI_TakeEvents(void); /* bit0 RX, bit1 TX, bit2 error */
const volatile BSP_BT_HCI_Diagnostics *BSP_BT_HCI_GetDiagnostics(void);
#endif
