#include "BSP_BT_HCI.h"
#include "BSP_Dash.h"
#include "BSP_NOR.h"
#include "usart.h"

/* HAL weak callback overrides are project-owned. Each BSP filters Instance;
 * no callback parses packets, accesses NOR, or renders a display frame. */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{ BSP_BT_HCI_OnRxComplete(uart);BSP_Dash_OnRxComplete(uart); }
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *uart)
{ BSP_BT_HCI_OnTxComplete(uart);BSP_Dash_OnTxComplete(uart); }
void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart)
{ BSP_BT_HCI_OnError(uart);BSP_Dash_OnError(uart); }
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *spi)
{ BSP_NOR_OnTxRxComplete(spi); }
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *spi)
{ BSP_NOR_OnError(spi); }

/* Strong handler is retained outside Cube. The generated IRQ declaration is
 * weak in its USER CODE block so a later IOC regeneration cannot duplicate it. */
void USART1_IRQHandler(void){HAL_UART_IRQHandler(&huart1);}
