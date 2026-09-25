#ifndef BSP_EVE_BUS_H
#define BSP_EVE_BUS_H
#include <stdint.h>
#include "bsp_eve.h"

/* SPI1은display소유태스크하나가사용한다. Select~Deselect사이에주소와데이터를
 * 여러조각으로전송할수있다. DMA/ISR비동기소유권은없고각전송은동기식이다.
 * LCD패널SPI4/백라이트TIM5와EVE의SPI1을혼동하지않는다. */
typedef struct {
    uint32_t selected, tx_bytes, rx_bytes, transactions, failures, last_status;
} BSP_EVE_BusDiagnostics;
extern volatile BSP_EVE_BusDiagnostics g_bsp_eve_bus;
BSP_EVE_Status BSP_EVE_BusSelect(void);
BSP_EVE_Status BSP_EVE_BusDeselect(void);
BSP_EVE_Status BSP_EVE_BusSend(const void *data, uint32_t length);
BSP_EVE_Status BSP_EVE_BusReceive(void *data, uint32_t length);
/* 22bit EVE address영역읽기만. 성공한때만완전한값으로사용하며partialerror는
 * caller가폐기한다. 스트림선택중에는별도memorytransaction을끼워넣지않는다. */
BSP_EVE_Status BSP_EVE_BusRead(uint32_t address, void *data, uint32_t length);
#endif
