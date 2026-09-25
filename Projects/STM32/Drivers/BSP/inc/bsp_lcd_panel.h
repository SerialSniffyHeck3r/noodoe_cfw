#ifndef BSP_LCD_PANEL_H
#define BSP_LCD_PANEL_H

#include "stm32f4xx_hal.h"

/* SWD로 읽는 현재 부팅의 기록이다. 각 필드는 32bit이며 영구 보관하지 않는다. */
typedef struct {
    uint32_t magic, stage, failed_stage, result, spi_error;
    uint32_t board_revision, protocol_selector, last_command;
    uint32_t last_tx_word, last_rx_word, power_mode;
    uint32_t write_frames, read_frames, init_attempts, ready;
} BSP_LCD_PanelDiagnostics;

extern volatile BSP_LCD_PanelDiagnostics g_bsp_lcd_panel;

/*
 * EVE 초기화 전에 호출한다. PE4/PI11/PC13만 설정하여 패널을 reset/off에 둔다.
 * 전원 유지 PD13/PG14, EVE SPI1, 다른 GPIO는 변경하지 않는다.
 * 현재 실물 strap revision 6 전용이며, revision<3의 EVE reset 경로는 지원하지 않는다.
 */
HAL_StatusTypeDef BSP_LCD_PanelPrepare(void);

/*
 * 동작하는 HAL 1ms tick 및 IRQ가 있는 Thread/task에서 한 번씩 직렬 호출한다.
 * 순정 metadata 0x0800C080 == 3인 패널만 초기화한다. GPIO reset, 생성 SPI4 init,
 * legacy 명령, 0x0A00 == 0x009C 검증까지 수행한다. 무한 재시도는 하지 않는다.
 * 오류가 나면 패널 reset/off 및 백라이트 off로 전환하고 HAL 오류를 반환한다.
 */
HAL_StatusTypeDef BSP_LCD_PanelInit(void);

/* 초기화 완료 뒤 0x0A00을 읽는다. 출력은 16bit 원본이며 실패 시 0으로 남는다. */
HAL_StatusTypeDef BSP_LCD_PanelReadStatus(uint16_t *power_mode);
/* Serial command only. Caller waits120ms after WakeBegin before WakeFinish. */
HAL_StatusTypeDef BSP_LCD_PanelSleep(void);
HAL_StatusTypeDef BSP_LCD_PanelWakeBegin(void);
HAL_StatusTypeDef BSP_LCD_PanelWakeFinish(void);

/* 대기 없이 reset/off로 만든다. 실패 경로에서도 사용할 수 있는 보수적 차단이다. */
void BSP_LCD_PanelShutdown(void);

#endif
