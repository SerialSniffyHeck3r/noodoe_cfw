#ifndef BSP_DASH_H
#define BSP_DASH_H
#include "stm32f4xx_hal.h"
#include <stdint.h>
/* First40bytes retain the previous diagnostic ABI. tx_bytes counts accepted
 * DMA bytes; tx_completed counts actual UART TC, tx_failed includes a stopped
 * failed transfer. A retained DMA owner is tx_last_result3 and cannot restart. */
typedef struct { uint32_t magic,ready,rx_bytes,tx_bytes,overflows,errors,restarts,tx_enabled,tx_busy,last_hal_error;
    uint32_t tx_completed,tx_failed,tx_last_result; } BSP_Dash_Diagnostics;
extern volatile BSP_Dash_Diagnostics g_bsp_dash;
/* UART5 owns only PC12/PD2. Default receive-only; enabling TX is explicit. */
uint32_t BSP_Dash_Init(void);
/* IO owner only. Suspend only after TX completion; RX bytes are discarded
 * at sleep boundaries. Wake restores UART5 and its prior TX authorization. */
uint32_t BSP_Dash_SetSleeping(uint32_t sleeping);
uint32_t BSP_Dash_Read(uint8_t *data,uint32_t capacity);
void BSP_Dash_Process(void);
uint32_t BSP_Dash_EnableTx(uint32_t enabled);
uint32_t BSP_Dash_Send(const uint8_t *data,uint32_t length);
/* I/O owner only: request one bounded abort through Process, never free the
 * DMA buffer here. Completion/failure remains observable in diagnostics. */
void BSP_Dash_RequestTxAbort(void);
/* Copy the preserved per-module10-entry calibration table at0800C040.
 * Caller validates values; no flash write, bus transaction or waiting. */
void BSP_Dash_ReadLightThresholds(uint32_t out[10]);
void BSP_Dash_OnRxComplete(UART_HandleTypeDef *uart);
void BSP_Dash_OnTxComplete(UART_HandleTypeDef *uart);
void BSP_Dash_OnError(UART_HandleTypeDef *uart);
#endif
