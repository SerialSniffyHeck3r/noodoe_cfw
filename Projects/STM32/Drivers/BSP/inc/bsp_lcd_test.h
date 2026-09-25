#ifndef BSP_LCD_TEST_H
#define BSP_LCD_TEST_H

#include <stdint.h>

/* 전 필드를 32bit로 고정하여 ELF 심볼 주소에서 SWD로 읽을 수 있게 한다.
 * version/size를 함께 검사해야 과거 BIN을 새 구조체로 잘못 해석하지 않는다.
 * 오류는 stage=ERROR와 최초 error에 보존하며, 성공 카운터로 덮어쓰지 않는다.
 */
typedef struct {
  uint32_t magic;
  uint32_t version;
  uint32_t stage;
  uint32_t error;
  uint32_t hal_tick;
  uint32_t transitions;
  uint32_t backlight_on;
  uint32_t requested_percent;
  uint32_t init_attempts;
  uint32_t last_result;
  uint32_t core_clock;
  uint32_t wait_errors;
  uint32_t eve_frames;
} BSP_LCDTestStatus;

enum {
  BSP_LCD_TEST_START = 1U,
  BSP_LCD_TEST_BACKLIGHT = 2U,
  BSP_LCD_TEST_EVE = 3U,
  BSP_LCD_TEST_PANEL = 4U,
  BSP_LCD_TEST_DRAW = 5U,
  BSP_LCD_TEST_BLINK = 6U,
  BSP_LCD_TEST_ERROR = 0xFFU
};

extern volatile BSP_LCDTestStatus g_bsp_lcd_test;

/* 배열 index는 BSP_Buttons의 UP/DOWN/ENTER 열거값이다. 기존 LCD 진단의
 * 52-byte 배치를 보존하고 버튼 소비/그림 반영 증거만 별도로 관측한다. */
typedef struct {
  uint32_t magic;
  uint32_t version;
  uint32_t service_count;
  uint32_t redraws;
  uint32_t pressed_mask;
  uint32_t long_mask;
  uint32_t very_long_mask;
  uint32_t last_button;
  uint32_t last_event;
  uint32_t last_duration_ms;
  uint32_t consumed_events;
  uint32_t last_duration_ms_by_button[3];
  uint32_t short_count[3];
  uint32_t long_count[3];
  uint32_t very_long_count[3];
} BSP_LCDButtonsStatus;
extern volatile BSP_LCDButtonsStatus g_bsp_lcd_buttons;

/* 프로젝트 수명 내내 보존하는 수동 선택 시험이다. RTOS 태스크에서 한 번
 * 호출하면 버튼 표시와 25%/OFF 깜빡임을 계속 실행한다. 반환/동적 할당 없음.
 * 외부 flash/SDRAM/BT/UART/RTC를 초기화하거나 쓰지 않는다.
 */
/* 원본 raw DL/버튼/25% blink 시험은 보존하며 필요 시 직접 호출할 수 있다.
 * 두 시험은 모두 태스크를 소유하는 무한 루프라 동시에 호출하지 않는다. */
void LCDTestLegacy(void);
void LCDTest(void);

#endif
