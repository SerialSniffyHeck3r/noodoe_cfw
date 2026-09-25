#ifndef BUTTONS_TEST_HAL_H
#define BUTTONS_TEST_HAL_H
#include <stdint.h>
/* 테스트 HAL은 실제 C 버튼 로직의 바깥 경계만 모의한다. 보드 레지스터 접근을
 * 호스트 메모리로 바꾸며 debounce/classification/event 로직을 복제하지 않는다. */
typedef struct { uint32_t IDR; } GPIO_TypeDef;
typedef struct { uint32_t Pin, Mode, Pull, Speed, Alternate; } GPIO_InitTypeDef;
typedef enum { GPIO_PIN_RESET = 0, GPIO_PIN_SET = 1 } GPIO_PinState;
extern GPIO_TypeDef test_gpio_a, test_gpio_d, test_gpio_i;
#define GPIOA (&test_gpio_a)
#define GPIOD (&test_gpio_d)
#define GPIOI (&test_gpio_i)
#define GPIO_PIN_5  (1U << 5)
#define GPIO_PIN_6  (1U << 6)
#define GPIO_PIN_12 (1U << 12)
#define GPIO_PIN_13 (1U << 13)
#define GPIO_PIN_15 (1U << 15)
#define GPIO_NOPULL 0U
#define GPIO_MODE_IT_RISING_FALLING 0x103U
#define EXTI9_5_IRQn 23
#define EXTI15_10_IRQn 40
extern uint32_t test_clock_mask, test_exti_pending;
#define __HAL_RCC_GPIOA_CLK_ENABLE() (test_clock_mask |= 1U)
#define __HAL_RCC_GPIOD_CLK_ENABLE() (test_clock_mask |= 2U)
#define __HAL_RCC_GPIOI_CLK_ENABLE() (test_clock_mask |= 4U)
#define __HAL_RCC_SYSCFG_CLK_ENABLE() (test_clock_mask |= 8U)
#define __HAL_GPIO_EXTI_CLEAR_IT(pin) (test_exti_pending &= ~(uint32_t)(pin))
uint32_t HAL_GetTick(void);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin);
void HAL_GPIO_Init(GPIO_TypeDef *port, const GPIO_InitTypeDef *config);
void HAL_NVIC_SetPriority(int irq, uint32_t priority, uint32_t subpriority);
void HAL_NVIC_EnableIRQ(int irq);
uint32_t __get_PRIMASK(void);
void __disable_irq(void);
void __set_PRIMASK(uint32_t mask);
#endif
