#include "bsp_lcd_panel.h"
#include "bsp_board_revision.h"
#include "Recovery_Target.h"
#include "bsp_backlight.h"
#include "main.h"
#include "spi.h"

/* 근거 주소와 전송 단위는 analysis/2026-09-12-lcd-bringup/panel-backlight.md에 기록한다. */
#define PANEL_MAGIC             0x504E4C31UL
#define PANEL_METADATA_ADDRESS  0x0800C080UL
#define PANEL_TRANSFER_MS       25UL
#define PANEL_EXPECTED_STATUS   0x009CU
#define PANEL_FAILED            0x80000000UL

volatile BSP_LCD_PanelDiagnostics g_bsp_lcd_panel;
static uint32_t panel_transport_ready;

/*
 * 출력 latch를 먼저 넣은 뒤 mode를 바꾸면 부트로더가 남긴 설정에서 불필요한
 * HIGH 펄스를 만들지 않는다. Cube gpio.c의 해당 핀 pull/speed만 좁혀 재사용한다.
 * 포트 전체 reset이나 MX_GPIO_Init은 전원 유지 핀까지 건드리므로 사용하지 않는다.
 */
static void PanelControlGPIOInit(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOI_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    HAL_GPIO_WritePin(LCD_RESET_CTRL_GPIO_Port, LCD_RESET_CTRL_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_ENABLE_CTRL_GPIO_Port, LCD_ENABLE_CTRL_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LCD_FRAME_CTRL_GPIO_Port, LCD_FRAME_CTRL_Pin, GPIO_PIN_RESET);

    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Pull = GPIO_NOPULL;
    gpio.Pin = LCD_RESET_CTRL_Pin;
    HAL_GPIO_Init(LCD_RESET_CTRL_GPIO_Port, &gpio);
    gpio.Pin = LCD_ENABLE_CTRL_Pin;
    HAL_GPIO_Init(LCD_ENABLE_CTRL_GPIO_Port, &gpio);
    gpio.Pull = GPIO_PULLUP;
    gpio.Pin = LCD_FRAME_CTRL_Pin;
    HAL_GPIO_Init(LCD_FRAME_CTRL_GPIO_Port, &gpio);
}

/* 준비/종료는 tick에 의존하지 않는다. 오류 기록은 종료해도 지우지 않는다. */
HAL_StatusTypeDef BSP_LCD_PanelPrepare(void)
{
    PanelControlGPIOInit();
    panel_transport_ready = 0U;
    g_bsp_lcd_panel.magic = PANEL_MAGIC;
    g_bsp_lcd_panel.board_revision = BSP_BoardRevision();
    g_bsp_lcd_panel.ready = 0U;
    g_bsp_lcd_panel.stage = 1U;
    return HAL_OK;
}

void BSP_LCD_PanelShutdown(void)
{
    BSP_BacklightShutdown();
    PanelControlGPIOInit();
    if (__HAL_RCC_SPI4_IS_CLK_ENABLED() != 0U) {
        CLEAR_BIT(SPI4->CR1, SPI_CR1_SPE);
    }
    panel_transport_ready = 0U;
    g_bsp_lcd_panel.ready = 0U;
}

/* 최초 실패 지점을 남긴다. 종료 중 발생한 핀 변경을 성공 단계로 기록하지 않는다. */
static HAL_StatusTypeDef PanelFail(HAL_StatusTypeDef result)
{
    g_bsp_lcd_panel.failed_stage = g_bsp_lcd_panel.stage;
    g_bsp_lcd_panel.result = (uint32_t)result;
    g_bsp_lcd_panel.spi_error = hspi4.ErrorCode;
    BSP_LCD_PanelShutdown();
    g_bsp_lcd_panel.stage = PANEL_FAILED;
    return result;
}

/*
 * 한 번은 byte가 아니라 16bit 한 word다. uint16_t 지역변수는 정렬되어 있으며
 * HAL의 Size=1도 현재 DFF=16bit에서 한 halfword를 뜻한다. HAL TxRx를 사용하여
 * 수신 DR을 항상 비우므로 write 때도 OVR을 축적하지 않는다.
 * 순정 0x08043024의 PE4 HIGH/LOW 네 번 및 각 2ms 지연을 그대로 유지한다.
 */
static HAL_StatusTypeDef PanelWriteWord(uint16_t word)
{
    uint16_t received = 0U;
    HAL_StatusTypeDef result;

    g_bsp_lcd_panel.last_tx_word = word;
    HAL_GPIO_WritePin(LCD_FRAME_CTRL_GPIO_Port, LCD_FRAME_CTRL_Pin, GPIO_PIN_SET);
    HAL_Delay(2U);
    HAL_GPIO_WritePin(LCD_FRAME_CTRL_GPIO_Port, LCD_FRAME_CTRL_Pin, GPIO_PIN_RESET);
    HAL_Delay(2U);
    result = HAL_SPI_TransmitReceive(&hspi4, (const uint8_t *)&word,
                                    (uint8_t *)&received, 1U, PANEL_TRANSFER_MS);
    g_bsp_lcd_panel.last_rx_word = received;
    if (result != HAL_OK) {
        return result;
    }
    HAL_Delay(2U);
    HAL_GPIO_WritePin(LCD_FRAME_CTRL_GPIO_Port, LCD_FRAME_CTRL_Pin, GPIO_PIN_SET);
    HAL_Delay(2U);
    HAL_GPIO_WritePin(LCD_FRAME_CTRL_GPIO_Port, LCD_FRAME_CTRL_Pin, GPIO_PIN_RESET);
    g_bsp_lcd_panel.write_frames++;
    return HAL_OK;
}

/*
 * 순정 0x0806AD58/0x0806AA90은 16bit 레지스터 번호를 먼저 high/low로 나눈다.
 * 예: Sleep Out 0x1100 -> 0x2011, 0x0000, 0x4000. 각 word는 독립 PE4 프레임이다.
 * read이면 마지막 word만 0xC000으로 바뀌고 후속 dummy read가 뒤따른다.
 */
static HAL_StatusTypeDef PanelAddress(uint16_t command, uint16_t operation)
{
    HAL_StatusTypeDef result;
    g_bsp_lcd_panel.last_command = command;
    result = PanelWriteWord((uint16_t)(0x2000U | (command >> 8U)));
    if (result != HAL_OK) {
        return result;
    }
    result = PanelWriteWord((uint16_t)(command & 0x00FFU));
    if (result != HAL_OK) {
        return result;
    }
    return PanelWriteWord(operation);
}

/*
 * 순정 SPI4 init 인수 0xA5를 memset하여 dummy halfword=0xA5A5로 만든다.
 * read wrapper 0x08042FCE는 PE4 LOW 상태로 전송하고 HIGH로 복원하지 않는다.
 * 응답은 바이트 교환/마스킹 없이 원본 halfword로 비교한다.
 */
static HAL_StatusTypeDef PanelReadPowerMode(uint16_t *power_mode)
{
    const uint16_t dummy = 0xA5A5U;
    HAL_StatusTypeDef result = PanelAddress(0x0A00U, 0xC000U);
    if (result != HAL_OK) {
        return result;
    }
    HAL_GPIO_WritePin(LCD_FRAME_CTRL_GPIO_Port, LCD_FRAME_CTRL_Pin, GPIO_PIN_RESET);
    g_bsp_lcd_panel.last_tx_word = dummy;
    result = HAL_SPI_TransmitReceive(&hspi4, (const uint8_t *)&dummy,
                                    (uint8_t *)power_mode, 1U, PANEL_TRANSFER_MS);
    g_bsp_lcd_panel.last_rx_word = *power_mode;
    if (result == HAL_OK) {
        g_bsp_lcd_panel.read_frames++;
        g_bsp_lcd_panel.power_mode = *power_mode;
    }
    return result;
}

HAL_StatusTypeDef BSP_LCD_PanelInit(void)
{
    uint16_t power_mode = 0U;
    HAL_StatusTypeDef result;

    BSP_LCD_PanelPrepare();
    g_bsp_lcd_panel.init_attempts++;
    g_bsp_lcd_panel.result = HAL_OK;
    g_bsp_lcd_panel.failed_stage = 0U;
    g_bsp_lcd_panel.spi_error = 0U;
    g_bsp_lcd_panel.power_mode = 0U;
    g_bsp_lcd_panel.protocol_selector = *(const volatile uint16_t *)PANEL_METADATA_ADDRESS;

    /* HAL timeout/delay는 IRQ 기반 tick이 필요하다. ISR/IRQ 마스킹 상태에서는 시작하지 않는다. */
    if ((__get_IPSR() != 0U) || (__get_PRIMASK() != 0U) || (__get_BASEPRI() != 0U) ||
        (!RECOVERY_PANEL_SUPPORTED(g_bsp_lcd_panel.protocol_selector)) ||
        (g_bsp_lcd_panel.board_revision < 3U)) {
        return PanelFail(HAL_ERROR);
    }

    /* Stock revision>=3 uses PC13. Do not infer strap from PCBA text. */
    g_bsp_lcd_panel.stage = 2U;
    HAL_GPIO_WritePin(LCD_ENABLE_CTRL_GPIO_Port, LCD_ENABLE_CTRL_Pin, GPIO_PIN_RESET);
    HAL_Delay(10U);
    HAL_GPIO_WritePin(LCD_RESET_CTRL_GPIO_Port, LCD_RESET_CTRL_Pin, GPIO_PIN_SET);
    HAL_Delay(10U);
    HAL_GPIO_WritePin(LCD_RESET_CTRL_GPIO_Port, LCD_RESET_CTRL_Pin, GPIO_PIN_RESET);
    HAL_Delay(20U);
    HAL_GPIO_WritePin(LCD_RESET_CTRL_GPIO_Port, LCD_RESET_CTRL_Pin, GPIO_PIN_SET);
    HAL_Delay(50U);

    /* SPI4만 reset한다. handle도 RESET으로 돌려 Cube HAL이 MSP 핀/클록 설정을 건너뛰지 않게 한다. */
    g_bsp_lcd_panel.stage = 3U;
    __HAL_RCC_SPI4_CLK_ENABLE();
    __HAL_RCC_SPI4_FORCE_RESET();
    __HAL_RCC_SPI4_RELEASE_RESET();
    __HAL_SPI_RESET_HANDLE_STATE(&hspi4);
    MX_SPI4_Init();
    if ((hspi4.State != HAL_SPI_STATE_READY) ||
        (HAL_RCC_GetPCLK2Freq() != 84000000UL) ||
        (hspi4.Init.Mode != SPI_MODE_MASTER) ||
        (hspi4.Init.Direction != SPI_DIRECTION_2LINES) ||
        (hspi4.Init.DataSize != SPI_DATASIZE_16BIT) ||
        (hspi4.Init.CLKPolarity != SPI_POLARITY_LOW) ||
        (hspi4.Init.CLKPhase != SPI_PHASE_1EDGE) ||
        (hspi4.Init.NSS != SPI_NSS_SOFT) ||
        (hspi4.Init.FirstBit != SPI_FIRSTBIT_MSB) ||
        (hspi4.Init.BaudRatePrescaler != SPI_BAUDRATEPRESCALER_16)) {
        return PanelFail(HAL_ERROR);
    }
    panel_transport_ready = 1U;

    g_bsp_lcd_panel.stage = 4U;
    result = PanelAddress(0x1100U, 0x4000U);
    if (result != HAL_OK) {
        return PanelFail(result);
    }
    HAL_Delay(120U);

    g_bsp_lcd_panel.stage = 5U;
    result = PanelAddress(0x2900U, 0x4000U);
    if (result != HAL_OK) {
        return PanelFail(result);
    }
    g_bsp_lcd_panel.stage = 6U;
    result = PanelReadPowerMode(&power_mode);
    if (result != HAL_OK) {
        return PanelFail(result);
    }
    if (power_mode != PANEL_EXPECTED_STATUS) {
        return PanelFail(HAL_ERROR);
    }
    g_bsp_lcd_panel.ready = 1U;
    g_bsp_lcd_panel.stage = 7U;
    return HAL_OK;
}

HAL_StatusTypeDef BSP_LCD_PanelReadStatus(uint16_t *power_mode)
{
    HAL_StatusTypeDef result;
    if (power_mode == NULL) {
        return PanelFail(HAL_ERROR);
    }
    *power_mode = 0U;
    if ((panel_transport_ready == 0U) || (g_bsp_lcd_panel.ready == 0U) ||
        (__get_IPSR() != 0U) || (__get_PRIMASK() != 0U) || (__get_BASEPRI() != 0U)) {
        return PanelFail(HAL_ERROR);
    }
    result = PanelReadPowerMode(power_mode);
    if (result != HAL_OK) {
        return PanelFail(result);
    }
    return HAL_OK;
}

/* These command stages leave reset/strap GPIO unchanged. Caller owns the
 *120ms sleep-out delay and does not illuminate the panel until completion. */
HAL_StatusTypeDef BSP_LCD_PanelSleep(void)
{
    HAL_StatusTypeDef result=PanelAddress(0x2800U,0x4000U);
    return result==HAL_OK?PanelAddress(0x1000U,0x4000U):result;
}
HAL_StatusTypeDef BSP_LCD_PanelWakeBegin(void){return PanelAddress(0x1100U,0x4000U);}
HAL_StatusTypeDef BSP_LCD_PanelWakeFinish(void)
{
    HAL_StatusTypeDef result=PanelAddress(0x2900U,0x4000U);uint16_t value=0;
    if(result==HAL_OK)result=PanelReadPowerMode(&value);
    return result==HAL_OK&&value==0x009CU?HAL_OK:HAL_ERROR;
}
