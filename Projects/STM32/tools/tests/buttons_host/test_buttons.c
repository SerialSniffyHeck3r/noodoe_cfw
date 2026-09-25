#include "BSP_Buttons.h"
#include "stm32f4xx_hal.h"
#include <stddef.h>

GPIO_TypeDef test_gpio_a, test_gpio_d, test_gpio_i;
uint32_t test_clock_mask, test_exti_pending;
static uint32_t test_tick, test_primask;
static uint32_t init_count, init_valid;
static uint32_t irq23_priority, irq40_priority, irq_enabled;
volatile uint32_t g_test_case;
volatile uint32_t g_test_failure_line;
volatile uint32_t g_test_assertions;

/* 에뮬레이터에서 반환 값/전역 line을 읽으므로 printf/libc나 실물 SWD가 필요 없다. */
#define CHECK(condition) do { ++g_test_assertions; if (!(condition)) { \
    g_test_failure_line = __LINE__; return __LINE__; } } while (0)

/* freestanding GCC가 struct 초기화/복사에 사용하는 기본 런타임만 제공한다. */
void *memset(void *dst, int value, size_t size)
{
    unsigned char *p = dst;
    while (size-- != 0U) { *p++ = (unsigned char)value; }
    return dst;
}
void *memcpy(void *dst, const void *src, size_t size)
{
    unsigned char *d = dst;
    const unsigned char *s = src;
    while (size-- != 0U) { *d++ = *s++; }
    return dst;
}

uint32_t HAL_GetTick(void) { return test_tick; }
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin)
{
    return (port->IDR & pin) != 0U ? GPIO_PIN_SET : GPIO_PIN_RESET;
}
void HAL_GPIO_Init(GPIO_TypeDef *port, const GPIO_InitTypeDef *config)
{
    ++init_count;
    if (config->Mode != GPIO_MODE_IT_RISING_FALLING || config->Pull != GPIO_NOPULL) {
        init_valid = 0U;
    }
    if (!((port == GPIOD && config->Pin == GPIO_PIN_12)
       || (port == GPIOI && config->Pin == GPIO_PIN_6)
       || (port == GPIOA && config->Pin == GPIO_PIN_15))) { init_valid = 0U; }
}
void HAL_NVIC_SetPriority(int irq, uint32_t priority, uint32_t subpriority)
{
    if (subpriority != 0U) { init_valid = 0U; }
    if (irq == EXTI9_5_IRQn) { irq23_priority = priority; }
    else if (irq == EXTI15_10_IRQn) { irq40_priority = priority; }
    else { init_valid = 0U; }
}
void HAL_NVIC_EnableIRQ(int irq)
{
    if (irq == EXTI9_5_IRQn) { irq_enabled |= 1U; }
    else if (irq == EXTI15_10_IRQn) { irq_enabled |= 2U; }
    else { init_valid = 0U; }
}
uint32_t __get_PRIMASK(void) { return test_primask; }
void __disable_irq(void) { test_primask = 1U; }
void __set_PRIMASK(uint32_t mask) { test_primask = mask; }

/* 테스트 입력은 GPIO 레벨과 HAL tick뿐이다. Process는 반드시 실제 BSP 소스다. */
static void Raw(BSP_Buttons_Button button, uint32_t pressed)
{
    GPIO_TypeDef *port = button == BSP_BUTTON_UP ? GPIOD
                         : button == BSP_BUTTON_DOWN ? GPIOI : GPIOA;
    uint32_t pin = button == BSP_BUTTON_UP ? GPIO_PIN_12
                   : button == BSP_BUTTON_DOWN ? GPIO_PIN_6 : GPIO_PIN_15;
    if (pressed != 0U) { port->IDR &= ~pin; }
    else { port->IDR |= pin; }
}
static void At(uint32_t tick) { test_tick = tick; BSP_Buttons_Process(); }
static void Reset(uint32_t tick)
{
    test_gpio_a.IDR = 0xFFFFU;
    test_gpio_d.IDR = 0xFFFFU;
    test_gpio_i.IDR = 0xFFFFU;
    test_tick = tick;
    test_primask = 0U;
    test_clock_mask = 0U;
    test_exti_pending = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_15;
    init_count = 0U;
    init_valid = 1U;
    irq_enabled = 0U;
    BSP_Buttons_Init();
}
static uint32_t Drain(void)
{
    BSP_Buttons_Event event;
    uint32_t count = 0U;
    while (BSP_Buttons_GetEvent(&event) != 0U) { ++count; }
    return count;
}
static void PressAtZero(void)
{
    Reset(0U);
    Raw(BSP_BUTTON_UP, 1U);
    At(0U);
    At(BSP_BUTTONS_DEBOUNCE_MS);
}

static int TestOwnership(void)
{
    BSP_Buttons_Process();
    BSP_Buttons_IRQHandler(GPIO_PIN_12);
    CHECK(g_bsp_buttons.initialized == 0U);
    Reset(100U);
    CHECK(init_count == 3U && init_valid == 1U);
    CHECK(test_clock_mask == 15U);
    CHECK(test_exti_pending == (GPIO_PIN_5 | GPIO_PIN_13));
    CHECK(irq23_priority == 15U && irq40_priority == 5U && irq_enabled == 3U);
    CHECK(g_bsp_buttons.magic == BSP_BUTTONS_DIAGNOSTIC_MAGIC);
    CHECK(g_bsp_buttons.version == 1U && BSP_Buttons_GetPressedMask() == 0U);
    test_primask = 1U;
    BSP_Buttons_Init();
    CHECK(test_primask == 1U);
    BSP_Buttons_IRQHandler(GPIO_PIN_5 | GPIO_PIN_13);
    CHECK(g_bsp_buttons.irq_count == 0U);
    return 0;
}

static int TestBounceAndIRQSpam(void)
{
    BSP_Buttons_Event event;
    Reset(0U);
    Raw(BSP_BUTTON_UP, 1U); At(0U);
    Raw(BSP_BUTTON_UP, 0U); At(10U);
    Raw(BSP_BUTTON_UP, 1U); At(20U);
    for (uint32_t tick = 21U; tick <= 40U; ++tick) {
        test_tick = tick;
        BSP_Buttons_IRQHandler(GPIO_PIN_12);
        BSP_Buttons_Process();
    }
    CHECK(BSP_Buttons_IsPressed(BSP_BUTTON_UP) == 0U);
    At(41U);
    CHECK(BSP_Buttons_IsPressed(BSP_BUTTON_UP) == 1U);
    CHECK(g_bsp_buttons.irq_count == 20U && g_bsp_buttons.press_count[BSP_BUTTON_UP] == 1U);
    CHECK(BSP_Buttons_GetEvent(&event) == 1U && event.type == BSP_BUTTON_EVENT_PRESS);
    CHECK(event.timestamp_ms == 20U && event.duration_ms == 0U);
    CHECK(BSP_Buttons_GetEvent(&event) == 0U);
    Raw(BSP_BUTTON_UP, 0U); At(51U); At(72U);
    CHECK(BSP_Buttons_GetLastDurationMs(BSP_BUTTON_UP) == 31U);
    CHECK(BSP_Buttons_GetLastClassification(BSP_BUTTON_UP) == BSP_BUTTON_CLASS_NONE);
    CHECK(BSP_Buttons_GetEvent(&event) == 1U && event.type == BSP_BUTTON_EVENT_RELEASE);
    CHECK(BSP_Buttons_GetEvent(&event) == 0U);
    return 0;
}

static int TestShortGateAndDelay(void)
{
    PressAtZero(); Raw(BSP_BUTTON_UP, 0U); At(59U); At(80U);
    CHECK(BSP_Buttons_GetLastDurationMs(BSP_BUTTON_UP) == 59U);
    CHECK(BSP_Buttons_GetLastClassification(BSP_BUTTON_UP) == BSP_BUTTON_CLASS_NONE);
    CHECK(Drain() == 2U);
    PressAtZero(); Raw(BSP_BUTTON_UP, 0U); At(60U); At(81U);
    CHECK(BSP_Buttons_GetLastClassification(BSP_BUTTON_UP) == BSP_BUTTON_CLASS_SHORT);
    CHECK(BSP_Buttons_GetLastDurationMs(BSP_BUTTON_UP) == 60U);
    CHECK(Drain() == 3U);
    /* 일정하지 않은 스케줄링이 짧은 잡음을 유효 SHORT로 승격하지 않는다. */
    PressAtZero(); Raw(BSP_BUTTON_UP, 0U); At(22U); At(10000U);
    CHECK(BSP_Buttons_GetLastClassification(BSP_BUTTON_UP) == BSP_BUTTON_CLASS_NONE);
    CHECK(BSP_Buttons_GetLastDurationMs(BSP_BUTTON_UP) == 22U && Drain() == 2U);
    return 0;
}

static int TestReleaseBoundary(void)
{
    BSP_Buttons_Event event;
    PressAtZero(); Drain();
    Raw(BSP_BUTTON_UP, 0U); At(2000U);
    CHECK(BSP_Buttons_GetCurrentDurationMs(BSP_BUTTON_UP) == 2000U);
    At(2010U);
    CHECK(BSP_Buttons_GetCurrentDurationMs(BSP_BUTTON_UP) == 2000U);
    CHECK(BSP_Buttons_GetEvent(&event) == 0U);
    At(2021U);
    CHECK(BSP_Buttons_GetLastClassification(BSP_BUTTON_UP) == BSP_BUTTON_CLASS_SHORT);
    CHECK(BSP_Buttons_GetEvent(&event) == 1U && event.type == BSP_BUTTON_EVENT_RELEASE);
    CHECK(event.duration_ms == 2000U && event.timestamp_ms == 2000U);
    CHECK(BSP_Buttons_GetEvent(&event) == 1U && event.type == BSP_BUTTON_EVENT_SHORT_PRESS);
    CHECK(BSP_Buttons_GetEvent(&event) == 0U);
    /* 임계 직후 release를 처음 보더라도 LONG을 RELEASE보다 먼저 catch-up한다. */
    PressAtZero(); Drain();
    Raw(BSP_BUTTON_UP, 0U); At(2001U); At(2022U);
    CHECK(BSP_Buttons_GetEvent(&event) == 1U && event.type == BSP_BUTTON_EVENT_LONG_PRESS);
    CHECK(event.duration_ms == 2001U && event.timestamp_ms == 2001U);
    CHECK(BSP_Buttons_GetEvent(&event) == 1U && event.type == BSP_BUTTON_EVENT_RELEASE);
    CHECK(event.classification == BSP_BUTTON_CLASS_LONG);
    CHECK(BSP_Buttons_GetEvent(&event) == 0U);
    return 0;
}

static int TestThresholdCatchupAndNoRepeat(void)
{
    BSP_Buttons_Event event;
    BSP_Buttons_State state;
    PressAtZero(); Drain(); At(3000U);
    CHECK(BSP_Buttons_GetState(BSP_BUTTON_UP, &state) == 1U);
    CHECK(state.long_event_sent == 1U && state.very_long_event_sent == 1U);
    CHECK(BSP_Buttons_GetEvent(&event) == 1U && event.type == BSP_BUTTON_EVENT_LONG_PRESS);
    CHECK(BSP_Buttons_GetEvent(&event) == 1U && event.type == BSP_BUTTON_EVENT_VERY_LONG_PRESS);
    CHECK(event.timestamp_ms == 3000U && event.duration_ms == 3000U);
    At(3100U); At(3200U);
    CHECK(BSP_Buttons_GetEvent(&event) == 0U);
    Raw(BSP_BUTTON_UP, 0U); At(3500U); At(3521U);
    CHECK(BSP_Buttons_GetEvent(&event) == 1U && event.type == BSP_BUTTON_EVENT_RELEASE);
    CHECK(event.classification == BSP_BUTTON_CLASS_VERY_LONG && event.duration_ms == 3500U);
    CHECK(BSP_Buttons_GetEvent(&event) == 0U);
    PressAtZero(); Drain(); Raw(BSP_BUTTON_UP, 0U); At(3500U); At(3521U);
    CHECK(BSP_Buttons_GetEvent(&event) == 1U && event.type == BSP_BUTTON_EVENT_LONG_PRESS);
    CHECK(BSP_Buttons_GetEvent(&event) == 1U && event.type == BSP_BUTTON_EVENT_VERY_LONG_PRESS);
    CHECK(BSP_Buttons_GetEvent(&event) == 1U && event.type == BSP_BUTTON_EVENT_RELEASE);
    CHECK(BSP_Buttons_GetEvent(&event) == 0U);
    return 0;
}

static int TestSimultaneous(void)
{
    Reset(100U);
    Raw(BSP_BUTTON_UP, 1U); Raw(BSP_BUTTON_ENTER, 1U); At(100U); At(121U);
    CHECK(BSP_Buttons_GetPressedMask() == (BSP_BUTTONS_MASK(BSP_BUTTON_UP) | BSP_BUTTONS_MASK(BSP_BUTTON_ENTER)));
    Raw(BSP_BUTTON_DOWN, 1U); At(150U); At(171U);
    CHECK(BSP_Buttons_GetPressedMask() == 7U);
    Raw(BSP_BUTTON_ENTER, 0U); At(200U); At(221U);
    CHECK(BSP_Buttons_GetPressedMask() == (BSP_BUTTONS_MASK(BSP_BUTTON_UP) | BSP_BUTTONS_MASK(BSP_BUTTON_DOWN)));
    CHECK(BSP_Buttons_GetCurrentDurationMs(BSP_BUTTON_UP) == 121U);
    CHECK(BSP_Buttons_GetCurrentDurationMs(BSP_BUTTON_DOWN) == 71U);
    CHECK(BSP_Buttons_GetLastDurationMs(BSP_BUTTON_ENTER) == 100U);
    CHECK(g_bsp_buttons.press_count[0] == 1U && g_bsp_buttons.press_count[1] == 1U && g_bsp_buttons.press_count[2] == 1U);
    CHECK(Drain() == 5U);
    return 0;
}

static int TestReleaseBounceRecovery(void)
{
    BSP_Buttons_Event event;
    PressAtZero(); Drain();
    Raw(BSP_BUTTON_UP, 0U); At(1999U); At(2010U);
    CHECK(BSP_Buttons_GetCurrentDurationMs(BSP_BUTTON_UP) == 1999U);
    CHECK(BSP_Buttons_GetEvent(&event) == 0U);
    Raw(BSP_BUTTON_UP, 1U); At(2011U);
    CHECK(BSP_Buttons_GetCurrentDurationMs(BSP_BUTTON_UP) == 2011U);
    CHECK(BSP_Buttons_GetEvent(&event) == 1U && event.type == BSP_BUTTON_EVENT_LONG_PRESS);
    CHECK(g_bsp_buttons.press_count[0] == 1U && g_bsp_buttons.release_count[0] == 0U);
    Raw(BSP_BUTTON_UP, 0U); At(2100U); At(2121U);
    CHECK(BSP_Buttons_GetLastDurationMs(BSP_BUTTON_UP) == 2100U);
    CHECK(Drain() == 1U);
    return 0;
}

static int TestBootHeldAndWrap(void)
{
    BSP_Buttons_Event event;
    const uint32_t start = 0xFFFFFFF0U;
    Reset(start); Raw(BSP_BUTTON_ENTER, 1U); BSP_Buttons_Init();
    CHECK(BSP_Buttons_IsPressed(BSP_BUTTON_ENTER) == 0U);
    At(start + 20U); CHECK(BSP_Buttons_IsPressed(BSP_BUTTON_ENTER) == 0U);
    At(start + 21U); CHECK(BSP_Buttons_IsPressed(BSP_BUTTON_ENTER) == 1U);
    CHECK(BSP_Buttons_GetEvent(&event) == 1U && event.type == BSP_BUTTON_EVENT_PRESS);
    CHECK(event.button == BSP_BUTTON_ENTER && event.timestamp_ms == start);
    Raw(BSP_BUTTON_ENTER, 0U); At(start + 60U); At(start + 81U);
    CHECK(BSP_Buttons_GetLastDurationMs(BSP_BUTTON_ENTER) == 60U);
    CHECK(BSP_Buttons_GetLastClassification(BSP_BUTTON_ENTER) == BSP_BUTTON_CLASS_SHORT);
    CHECK(Drain() == 2U);
    return 0;
}

static int TestSaturation(void)
{
    PressAtZero(); At(0x7FFFFFFFU); At(0xFFFFFFFEU); At(10U);
    CHECK(BSP_Buttons_GetCurrentDurationMs(BSP_BUTTON_UP) == UINT32_MAX);
    Raw(BSP_BUTTON_UP, 0U); At(11U); At(32U);
    CHECK(BSP_Buttons_GetLastDurationMs(BSP_BUTTON_UP) == UINT32_MAX);
    CHECK(BSP_Buttons_GetLastClassification(BSP_BUTTON_UP) == BSP_BUTTON_CLASS_VERY_LONG);
    CHECK(Drain() == 4U);
    return 0;
}

static int TestOverflowAndInvalidArguments(void)
{
    BSP_Buttons_Event event;
    BSP_Buttons_State state;
    Reset(0U);
    for (uint32_t cycle = 0U; cycle < 12U; ++cycle) {
        uint32_t start = cycle * 100U;
        Raw(BSP_BUTTON_UP, 1U); At(start); At(start + 21U);
        Raw(BSP_BUTTON_UP, 0U); At(start + 60U); At(start + 81U);
    }
    CHECK(BSP_Buttons_GetOverflowCount() == 4U);
    CHECK(g_bsp_buttons.event_sequence == 36U && g_bsp_buttons.events_pending == 32U);
    CHECK(BSP_Buttons_GetPressedMask() == 0U && BSP_Buttons_GetLastDurationMs(BSP_BUTTON_UP) == 60U);
    CHECK(BSP_Buttons_GetEvent(0) == 0U && g_bsp_buttons.events_pending == 32U);
    CHECK(Drain() == 32U);
    Raw(BSP_BUTTON_DOWN, 1U); At(1200U); At(1221U);
    CHECK(BSP_Buttons_GetEvent(&event) == 1U && event.sequence == 37U);
    CHECK(event.button == BSP_BUTTON_DOWN && event.type == BSP_BUTTON_EVENT_PRESS);
    CHECK(BSP_Buttons_GetState((BSP_Buttons_Button)-1, &state) == 0U);
    CHECK(BSP_Buttons_GetState(BSP_BUTTON_UP, 0) == 0U);
    CHECK(BSP_Buttons_IsPressed((BSP_Buttons_Button)-1) == 0U);
    CHECK(BSP_Buttons_GetLastDurationMs(BSP_BUTTON_COUNT) == 0U);
    CHECK(BSP_Buttons_GetLastClassification(BSP_BUTTON_COUNT) == BSP_BUTTON_CLASS_NONE);
    BSP_Buttons_ClearEvents();
    CHECK(g_bsp_buttons.events_pending == 0U && BSP_Buttons_GetOverflowCount() == 4U);
    CHECK(BSP_Buttons_IsPressed(BSP_BUTTON_DOWN) == 1U);
    return 0;
}

int Buttons_TestMain(void)
{
    int result;
#define RUN(id, test) do { g_test_case = (id); result = (test)(); if (result != 0) { return result; } } while (0)
    RUN(1U, TestOwnership);
    RUN(2U, TestBounceAndIRQSpam);
    RUN(3U, TestShortGateAndDelay);
    RUN(4U, TestReleaseBoundary);
    RUN(5U, TestThresholdCatchupAndNoRepeat);
    RUN(6U, TestSimultaneous);
    RUN(7U, TestReleaseBounceRecovery);
    RUN(8U, TestBootHeldAndWrap);
    RUN(9U, TestSaturation);
    RUN(10U, TestOverflowAndInvalidArguments);
    return 0;
}
