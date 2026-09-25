#include "bsp_backlight.h"
#include "main.h"
#include "tim.h"

#define BACKLIGHT_MAGIC  0x424C5431UL
#define BACKLIGHT_FAILED 0x80000000UL

volatile BSP_BacklightDiagnostics g_bsp_backlight;

/*
 * Cube의 PC8/PI8 설정만 초기화한다. 순정 GPIO 초기 레벨은 PC8 HIGH, PI8 LOW다.
 * PI0도 여기서는 일단 GPIO LOW로 만들어 TIM5 AF 전환 전의 pull-up 점등을 막는다.
 * 후속 MX_TIM5_Init의 MSP post-init이 PI0를 생성된 AF2/PULLUP/VERY_HIGH로 바꾼다.
 */
static void BacklightGPIOOff(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOI_CLK_ENABLE();
    HAL_GPIO_WritePin(BACKLIGHT_DISABLE_CTRL_GPIO_Port, BACKLIGHT_DISABLE_CTRL_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(BACKLIGHT_POWER_CTRL_GPIO_Port, BACKLIGHT_POWER_CTRL_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_0, GPIO_PIN_RESET);

    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Pin = BACKLIGHT_DISABLE_CTRL_Pin;
    HAL_GPIO_Init(BACKLIGHT_DISABLE_CTRL_GPIO_Port, &gpio);
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Pin = BACKLIGHT_POWER_CTRL_Pin;
    HAL_GPIO_Init(BACKLIGHT_POWER_CTRL_GPIO_Port, &gpio);
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Pin = GPIO_PIN_0;
    HAL_GPIO_Init(GPIOI, &gpio);
}

/*
 * timer handle을 믿을 수 없는 초기/실패 상태도 처리한다. 외부 disable을 먼저 걸고
 * PWM GPIO를 LOW로 바꾼 뒤, clock이 켜져 있을 때만 TIM5를 멈춘다. 다른 포트나
 * 타이머는 reset하지 않는다. 이 보드에서 TIM5는 백라이트 전용이라는 계약이다.
 */
void BSP_BacklightShutdown(void)
{
    BacklightGPIOOff();
    if (__HAL_RCC_TIM5_IS_CLK_ENABLED() != 0U) {
        TIM5->CCR4 = 0U;
        CLEAR_BIT(TIM5->CCER, TIM_CCER_CC4E);
        CLEAR_BIT(TIM5->CR1, TIM_CR1_CEN);
        TIM5->DIER = 0U;
        TIM5->SR = 0U;
    }
    g_bsp_backlight.initialized = 0U;
    g_bsp_backlight.percent = 0U;
    g_bsp_backlight.compare = 0U;
}

static HAL_StatusTypeDef BacklightFail(HAL_StatusTypeDef result)
{
    BSP_BacklightShutdown();
    g_bsp_backlight.result = (uint32_t)result;
    g_bsp_backlight.stage = BACKLIGHT_FAILED;
    return result;
}

HAL_StatusTypeDef BSP_BacklightInit(void)
{
    HAL_StatusTypeDef result;
    uint32_t timer_hz;

    BSP_BacklightShutdown();
    g_bsp_backlight.magic = BACKLIGHT_MAGIC;
    g_bsp_backlight.stage = 1U;
    g_bsp_backlight.result = HAL_OK;
    if ((__get_IPSR() != 0U) || (__get_PRIMASK() != 0U) || (__get_BASEPRI() != 0U)) {
        return BacklightFail(HAL_ERROR);
    }

    /* 순정과 IOC의 84MHz 조건을 검증한다. TIMPRE 확장 모드는 이번 고정 설정의 대상이 아니다. */
    timer_hz = HAL_RCC_GetPCLK1Freq();
    if ((RCC->CFGR & RCC_CFGR_PPRE1) != 0U) {
        timer_hz *= 2U;
    }
    g_bsp_backlight.timer_hz = timer_hz;
    if ((timer_hz != 84000000UL) || ((RCC->DCKCFGR & RCC_DCKCFGR_TIMPRE) != 0U)) {
        return BacklightFail(HAL_ERROR);
    }

    /*
     * BL이 남긴 CCER/DIER/UIF를 없앤 뒤 생성 함수로 PSC/ARR/PWM/AF를 다시 설정한다.
     * Cube가 TIM5_IRQn을 enable하더라도 reset된 DIER=0이라 PWM용 IRQ는 발생하지 않는다.
     * HAL init 오류는 생성 Error_Handler로 가지만, 아직 외부 전원/disable이 off라 점등하지 않는다.
     */
    __HAL_RCC_TIM5_CLK_ENABLE();
    __HAL_RCC_TIM5_FORCE_RESET();
    __HAL_RCC_TIM5_RELEASE_RESET();
    __HAL_TIM_RESET_HANDLE_STATE(&htim5);
    MX_TIM5_Init();
    g_bsp_backlight.prescaler = TIM5->PSC;
    g_bsp_backlight.period = TIM5->ARR;
    if ((TIM5->PSC != 1679U) || (TIM5->ARR != 99U) ||
        ((TIM5->CCMR2 & TIM_CCMR2_OC4M) != (TIM_OCMODE_PWM1 << 8U)) ||
        ((TIM5->CCER & TIM_CCER_CC4P) != 0U)) {
        return BacklightFail(HAL_ERROR);
    }

    /* CCR4 preload도 즉시 0으로 반영하고, 외부 enable 전에 실제 PWM LOW를 먼저 만든다. */
    __HAL_TIM_SET_COMPARE(&htim5, TIM_CHANNEL_4, 0U);
    __HAL_TIM_SET_COUNTER(&htim5, 0U);
    TIM5->EGR = TIM_EGR_UG;
    __HAL_TIM_CLEAR_FLAG(&htim5, TIM_FLAG_UPDATE);
    result = HAL_TIM_PWM_Start(&htim5, TIM_CHANNEL_4);
    if (result != HAL_OK) {
        return BacklightFail(result);
    }

    /* 순정 0x080434D6..434FC의 별도 전원 순서. 여기까지도 duty는 0이다. */
    g_bsp_backlight.stage = 2U;
    HAL_GPIO_WritePin(BACKLIGHT_POWER_CTRL_GPIO_Port, BACKLIGHT_POWER_CTRL_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(BACKLIGHT_DISABLE_CTRL_GPIO_Port, BACKLIGHT_DISABLE_CTRL_Pin, GPIO_PIN_RESET);
    HAL_Delay(3U);
    g_bsp_backlight.initialized = 1U;
    g_bsp_backlight.stage = 3U;
    return HAL_OK;
}

HAL_StatusTypeDef BSP_BacklightSetPercent(uint32_t percent)
{
    if ((g_bsp_backlight.initialized == 0U) || (percent > 100U) ||
        (TIM5->PSC != 1679U) || (TIM5->ARR != 99U) ||
        ((TIM5->CR1 & TIM_CR1_CEN) == 0U) || ((TIM5->CCER & TIM_CCER_CC4E) == 0U)) {
        return BacklightFail(HAL_ERROR);
    }
    if (percent == 100U) {
        percent = 99U;
    }
    /*
     * PWM1/ARR99이므로 CCR4가 백분율과 같다. 25 -> 한 주기 100count 중 25count HIGH.
     * OC preload는 다음 update(최대 2ms)에 적용된다. 반복적으로 timer를 stop/start하여
     * 위상을 끊지 않는다. percent=0도 CC4E/CEN을 유지하여 PI0를 명시적으로 LOW 구동한다.
     */
    __HAL_TIM_SET_COMPARE(&htim5, TIM_CHANNEL_4, percent);
    g_bsp_backlight.percent = percent;
    g_bsp_backlight.compare = TIM5->CCR4;
    g_bsp_backlight.updates++;
    return HAL_OK;
}
