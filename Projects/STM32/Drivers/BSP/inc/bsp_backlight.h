#ifndef BSP_BACKLIGHT_H
#define BSP_BACKLIGHT_H

#include "stm32f4xx_hal.h"

/* 진단 단위: timer_hz/PWM 주파수는 Hz, percent는 PWM HIGH 듀티 백분율이다. */
typedef struct {
    uint32_t magic, stage, result, initialized, percent;
    uint32_t timer_hz, prescaler, period, compare, updates;
} BSP_BacklightDiagnostics;

extern volatile BSP_BacklightDiagnostics g_bsp_backlight;

/*
 * HAL tick이 동작하는 Thread/task에서 호출한다. 생성 TIM5 CH4 설정을 사용하고
 * 84MHz timer / (1679+1) / (99+1) = 500Hz를 확인한다.
 * 첫 PWM은 CCR4=0이다. PI8 HIGH -> PC8 LOW -> 3ms 후에도 빛은 켜지지 않는다.
 * 실패하면 PI0 LOW, PC8 HIGH, PI8 LOW로 차단한다.
 */
HAL_StatusTypeDef BSP_BacklightInit(void);

/*
 * Init 성공 후 직렬 호출한다. 0..100을 받으며 순정처럼 100은 99로 제한한다.
 * 25는 CCR4=25이다. 0은 PWM을 LOW로 유지하며 외부 pull-up에 맡겨두지 않는다.
 * 범위 밖 입력/미초기화/설정 불일치는 HAL_ERROR와 강제 off를 반환한다.
 */
HAL_StatusTypeDef BSP_BacklightSetPercent(uint32_t percent);

/* Init 전에도 호출 가능하다. 다른 TIM5 사용자와 공유하지 않는 전용 BSP 계약이다. */
void BSP_BacklightShutdown(void);

#endif
