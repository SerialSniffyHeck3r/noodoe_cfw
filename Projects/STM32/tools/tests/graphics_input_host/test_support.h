#ifndef GRAPHICS_INPUT_TEST_SUPPORT_H
#define GRAPHICS_INPUT_TEST_SUPPORT_H
#include "graphics_internal.h"
#include "BSP_Buttons.h"
#include <stddef.h>

/* Graphics_Shutdown 본문은 runner가 원본에서 복사한다. 아래 전역은 그 함수가
 * 접근하는 실제 이름의 상태를 모사한다. 함수 자체의 분기/호출은 바꾸지 않는다. */
typedef uintptr_t osThreadId_t;
extern osThreadId_t graphics_owner;
extern uint32_t lvgl_started, brightness;
extern lv_display_t *graphics_display;
extern lv_group_t *graphics_group;
uint32_t __get_IPSR(void);
uint32_t __get_PRIMASK(void);
uint32_t __get_BASEPRI(void);
osThreadId_t osThreadGetId(void);
void BSP_Display_Shutdown(void);
#endif
