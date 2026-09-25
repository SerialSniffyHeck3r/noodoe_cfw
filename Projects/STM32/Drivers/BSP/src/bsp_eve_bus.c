#include "bsp_eve_bus.h"
#include "spi.h"
#include <stddef.h>

/* blockingHAL의16bitlength제한과stack사용을동시에제한한다. 매chunk RX도
 * 읽어RXoverrun을피하며CS는chunk사이에도LOW로유지한다. */
#define BUS_CHUNK 256U
#define BUS_TIMEOUT_MS 25U
static uint8_t dummy_tx[BUS_CHUNK];
static uint8_t discard_rx[BUS_CHUNK];
volatile BSP_EVE_BusDiagnostics g_bsp_eve_bus;

static BSP_EVE_Status BusResult(BSP_EVE_Status result)
{
    g_bsp_eve_bus.last_status = (uint32_t)result;
    if (result != BSP_EVE_OK) { ++g_bsp_eve_bus.failures; }
    return result;
}

/* IRQ를차단하지않고잘못된호출문맥을거절한다. HALtimeout이동작하려면
 * tickIRQ가살아있어야하며init미완료장치에임의명령을보내지않는다. */
static BSP_EVE_Status BusContext(void)
{
    if (__get_IPSR() || __get_PRIMASK() || __get_BASEPRI()) {
        return BusResult(BSP_EVE_ERROR_CONTEXT);
    }
    if (!g_bsp_eve.ready || hspi1.Instance != SPI1 ||
        HAL_SPI_GetState(&hspi1) != HAL_SPI_STATE_READY) {
        return BusResult(BSP_EVE_ERROR_NOT_READY);
    }
    return BSP_EVE_OK;
}

BSP_EVE_Status BSP_EVE_BusSelect(void)
{
    BSP_EVE_Status result = BusContext();
    if (result != BSP_EVE_OK) { return result; }
    if (g_bsp_eve_bus.selected) { return BusResult(BSP_EVE_ERROR_CONTEXT); }
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
    g_bsp_eve_bus.selected = 1U;
    ++g_bsp_eve_bus.transactions;
    return BusResult(BSP_EVE_OK);
}

/* 에러복구에서도CS를해제할수있게초기화상태검사를요구하지않는다.
 * 이함수는태스크에서만사용하고pendingHAL DMA동작은존재하지않는다. */
BSP_EVE_Status BSP_EVE_BusDeselect(void)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
    g_bsp_eve_bus.selected = 0U;
    return BSP_EVE_OK;
}

static BSP_EVE_Status BusTransfer(const uint8_t *send, uint8_t *receive, uint32_t length)
{
    BSP_EVE_Status result = BusContext();
    if (result != BSP_EVE_OK) { return result; }
    if (!g_bsp_eve_bus.selected || (!send && !receive)) {
        return BusResult(BSP_EVE_ERROR_ARGUMENT);
    }
    while (length) {
        uint16_t part = (uint16_t)(length > BUS_CHUNK ? BUS_CHUNK : length);
        HAL_StatusTypeDef hal = HAL_SPI_TransmitReceive(&hspi1,
            send ? (uint8_t *)send : dummy_tx, receive ? receive : discard_rx,
            part, BUS_TIMEOUT_MS);
        if (hal != HAL_OK) {
            BSP_EVE_BusDeselect();
            g_bsp_eve.spi_status = (uint32_t)hal;
            g_bsp_eve.spi_error = HAL_SPI_GetError(&hspi1);
            return BusResult(BSP_EVE_ERROR_SPI);
        }
        if (send) { send += part; g_bsp_eve_bus.tx_bytes += part; }
        if (receive) { receive += part; g_bsp_eve_bus.rx_bytes += part; }
        length -= part;
    }
    return BusResult(BSP_EVE_OK);
}

BSP_EVE_Status BSP_EVE_BusSend(const void *data, uint32_t length)
{
    return BusTransfer((const uint8_t *)data, NULL, length);
}

BSP_EVE_Status BSP_EVE_BusReceive(void *data, uint32_t length)
{
    return BusTransfer(NULL, (uint8_t *)data, length);
}

BSP_EVE_Status BSP_EVE_BusRead(uint32_t address, void *data, uint32_t length)
{
    if (!data || !length || address > 0x3FFFFFU || length > 0x400000U - address) {
        return BusResult(BSP_EVE_ERROR_ARGUMENT);
    }
    uint8_t header[4] = {(uint8_t)(address >> 16U), (uint8_t)(address >> 8U), (uint8_t)address, 0U};
    BSP_EVE_Status result = BSP_EVE_BusSelect();
    if (result != BSP_EVE_OK) { return result; }
    if (result == BSP_EVE_OK) { result = BSP_EVE_BusSend(header, sizeof(header)); }
    if (result == BSP_EVE_OK) { result = BSP_EVE_BusReceive(data, length); }
    BSP_EVE_BusDeselect();
    return result;
}
