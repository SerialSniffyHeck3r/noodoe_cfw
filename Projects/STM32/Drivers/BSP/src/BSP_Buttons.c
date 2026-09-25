#include "BSP_Buttons.h"
#include "stm32f4xx_hal.h"

/* 순정 근거: descriptor 0x08076D98=[0x3C,0x0F,0x86]은 PD12/PA15/PI6이다.
 * 0x08036E7A..0x08036EC0의 GPIO/EXTI 설정과 OQC short dispatch가 각각
 * UP/ENTER/DOWN임을 확인한다. PI3/PI4/PI5는 버튼으로 추측하여 사용하지 않는다.
 * 0x08043CE0 TIM2 1ms 서비스는 >80ms decision, >2000ms long, release후
 * >=21ms를 사용한다. 본 서비스는 polling으로 독립 상태를 유지하므로 다음이
 * 의도적 차이다: press도 debounce, raw release에서 duration 동결, very-long,
 * PH9/IGN 정책 제외. 순정과 마찬가지로 버튼별 상태는 독립적이다. 접촉 시각은 ISR 시각이
 * 아니라 Process가 raw level 변화를 관측한 시각이므로 정밀도는 polling 주기다. */

_Static_assert(BSP_BUTTONS_DEBOUNCE_MS > 0U, "Button debounce must be positive");
_Static_assert(BSP_BUTTONS_DEBOUNCE_MS < BSP_BUTTONS_LONG_PRESS_MS,
               "Debounce must be shorter than long threshold");
_Static_assert(BSP_BUTTONS_MIN_DECISION_MS < BSP_BUTTONS_LONG_PRESS_MS,
               "Minimum decision gate must precede long threshold");
_Static_assert(BSP_BUTTONS_LONG_PRESS_MS < BSP_BUTTONS_VERY_LONG_PRESS_MS,
               "Very-long threshold must follow long threshold");
_Static_assert(BSP_BUTTONS_VERY_LONG_PRESS_MS < 0x80000000UL,
               "Thresholds must fit the modular tick interval");
_Static_assert(BSP_BUTTONS_EVENT_QUEUE_SIZE > 0U, "Event queue must not be empty");

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} ButtonPin;

/* 공개 enum 순서는 UP/DOWN/ENTER다. 순정 descriptor 순서를 그대로 배열에 넣어
 * DOWN과 ENTER를 뒤집지 않도록 지정 initializer로 각 핀의 의미를 고정한다. */
static const ButtonPin button_pins[BSP_BUTTON_COUNT] = {
    [BSP_BUTTON_UP] = {GPIOD, GPIO_PIN_12},
    [BSP_BUTTON_DOWN] = {GPIOI, GPIO_PIN_6},
    [BSP_BUTTON_ENTER] = {GPIOA, GPIO_PIN_15}
};

typedef struct {
    uint32_t stable_pressed;
    uint32_t candidate_pressed;
    uint32_t candidate_since_ms;
    uint32_t candidate_hold_ms;
    uint32_t press_tick_ms;
    uint32_t held_elapsed_ms;
    uint32_t current_duration_ms;
    uint32_t last_duration_ms;
    BSP_Buttons_Classification last_classification;
    uint32_t long_sent;
    uint32_t very_long_sent;
} ButtonState;

volatile BSP_Buttons_Diagnostics g_bsp_buttons;
static ButtonState buttons[BSP_BUTTON_COUNT];
static BSP_Buttons_Event event_queue[BSP_BUTTONS_EVENT_QUEUE_SIZE];
static uint32_t queue_read;
static uint32_t queue_count;
static uint32_t previous_process_ms;

/* 32비트 누름 시간을 더할 때 wrap해 0으로 돌아가지 않고 UINT32_MAX에서 멈춘다.
 * tick 차이 자체는 unsigned 뺄셈으로 wrap을 허용한다. Process 사이에 tick이
 * 한 바퀴 이상 도는 부재는 복원할 수 없으며 공개 호출 주기 계약에서 제외한다. */
static uint32_t SaturatingAdd(uint32_t left, uint32_t right)
{
    return (right > UINT32_MAX - left) ? UINT32_MAX : left + right;
}

/* GPIO 입력을 HIGH=0/LOW=1로 정규화한다. 내부 pull-up을 임의 추가하지 않는다.
 * 차량 보드의 기존 active-low 회로가 안정된 HIGH를 공급하는 것이 핀 계약이다. */
static uint32_t ReadPressed(uint32_t index)
{
    return HAL_GPIO_ReadPin(button_pins[index].port, button_pins[index].pin)
           == GPIO_PIN_RESET ? 1U : 0U;
}

/* ISR와 task가 공유하는 IRQ 진단의 짧은 RMW 구간만 보호한다. 원래 PRIMASK를
 * 되돌려 호출자가 이미 막아 둔 IRQ를 실수로 다시 열지 않는다. RTOS API는 없다. */
static uint32_t LockIRQ(void)
{
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    return mask;
}

static void UnlockIRQ(uint32_t mask)
{
    __set_PRIMASK(mask);
}

/* task 전용 큐 삽입. 큐가 가득 차면 새 이벤트만 버린다. 상태 전이를 되돌리지
 * 않으므로 overflow 후에도 getter는 실제 상태이며 소비자는 overflow 수/sequence
 * 공백을 보고 자신의 이벤트 기반 상태를 getter와 다시 동기화할 수 있다. */
static void Emit(uint32_t index, BSP_Buttons_EventType type, uint32_t tick,
                 uint32_t duration, BSP_Buttons_Classification classification)
{
    uint32_t sequence = ++g_bsp_buttons.event_sequence;
    if (queue_count == BSP_BUTTONS_EVENT_QUEUE_SIZE) {
        ++g_bsp_buttons.event_overflow_count;
        return;
    }
    uint32_t write = (queue_read + queue_count) % BSP_BUTTONS_EVENT_QUEUE_SIZE;
    event_queue[write].button = (BSP_Buttons_Button)index;
    event_queue[write].type = type;
    event_queue[write].timestamp_ms = tick;
    event_queue[write].duration_ms = duration;
    event_queue[write].classification = classification;
    event_queue[write].sequence = sequence;
    ++queue_count;
    ++g_bsp_buttons.events_enqueued;
    g_bsp_buttons.events_pending = queue_count;
}

/* 지연된 polling이 두 임계시간을 동시에 넘었다면 LONG 다음 VERY_LONG을 각각
 * 한 번 발생시킨다. 임계 이벤트는 큐 overflow로 버려져도 다시 반복하지 않는다.
 * timestamp는 논리적 임계 시각이며 관측/소비 시각과 다를 수 있다. */
static void EmitThresholds(uint32_t index, uint32_t duration)
{
    ButtonState *state = &buttons[index];
    if (state->long_sent == 0U && duration >= BSP_BUTTONS_LONG_PRESS_MS) {
        state->long_sent = 1U;
        Emit(index, BSP_BUTTON_EVENT_LONG_PRESS,
             state->press_tick_ms + BSP_BUTTONS_LONG_PRESS_MS,
             BSP_BUTTONS_LONG_PRESS_MS, BSP_BUTTON_CLASS_LONG);
    }
    if (state->very_long_sent == 0U && duration >= BSP_BUTTONS_VERY_LONG_PRESS_MS) {
        state->very_long_sent = 1U;
        Emit(index, BSP_BUTTON_EVENT_VERY_LONG_PRESS,
             state->press_tick_ms + BSP_BUTTONS_VERY_LONG_PRESS_MS,
             BSP_BUTTONS_VERY_LONG_PRESS_MS, BSP_BUTTON_CLASS_VERY_LONG);
    }
}

/* long/very-long은 실제 hold 기간만 사용한다. short의 80ms gate는 순정의 정상
 * 1ms 서비스처럼 논리적 release확인 시각(hold+21ms)을 기준으로 한다. 태스크가
 * 늦게 호출됐다는 이유만으로 짧은 잡음을 SHORT로 승격하지 않도록 실제 polling
 * 지연은 더하지 않는다. 80ms는 물리적 최소 hold가 아니며 작은 입력은 NONE이다. */
static BSP_Buttons_Classification Classify(uint32_t duration)
{
    if (duration >= BSP_BUTTONS_VERY_LONG_PRESS_MS) { return BSP_BUTTON_CLASS_VERY_LONG; }
    if (duration >= BSP_BUTTONS_LONG_PRESS_MS) { return BSP_BUTTON_CLASS_LONG; }
    return SaturatingAdd(duration, BSP_BUTTONS_DEBOUNCE_MS) > BSP_BUTTONS_MIN_DECISION_MS
           ? BSP_BUTTON_CLASS_SHORT : BSP_BUTTON_CLASS_NONE;
}

void BSP_Buttons_Init(void)
{
    uint32_t mask = LockIRQ();
    GPIO_InitTypeDef gpio = {0};

    /* early BL 인계가 EXTI를 막았더라도 버튼에 필요한 라인만 다시 구성한다.
     * GPIO/SYSCFG clock enable은 포트 reset과 다르며 전원 출력 레벨을 바꾸지 않는다. */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOI_CLK_ENABLE();
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    gpio.Mode = GPIO_MODE_IT_RISING_FALLING;
    gpio.Pull = GPIO_NOPULL;
    for (uint32_t index = 0U; index < BSP_BUTTON_COUNT; ++index) {
        gpio.Pin = button_pins[index].pin;
        HAL_GPIO_Init(button_pins[index].port, &gpio);
    }
    /* EXTI PR는 write-one-to-clear다. 소유한 6/12/15만 지우고 공유 그룹의
     * PI5, IGN PG13 등 다른 pending을 HAL_NVIC_ClearPendingIRQ로 날리지 않는다. */
    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_6 | GPIO_PIN_12 | GPIO_PIN_15);
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5U, 0U);
    HAL_NVIC_SetPriority(EXTI9_5_IRQn, 15U, 0U);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

    /* 재초기화 시 오래된 눌림/이벤트/overflow를 이어받지 않는다. 구조체를 같은
     * 타입의 0 값으로 초기화하여 struct를 배열로 간주하는 alias 위반도 피한다. */
    g_bsp_buttons = (BSP_Buttons_Diagnostics){0};
    uint32_t now = HAL_GetTick();
    for (uint32_t index = 0U; index < BSP_BUTTON_COUNT; ++index) {
        buttons[index] = (ButtonState){0};
        buttons[index].candidate_pressed = ReadPressed(index);
        buttons[index].candidate_since_ms = now;
        if (buttons[index].candidate_pressed != 0U) {
            g_bsp_buttons.raw_pressed_mask |= BSP_BUTTONS_MASK(index);
        }
    }
    queue_read = 0U;
    queue_count = 0U;
    previous_process_ms = now;
    g_bsp_buttons.magic = BSP_BUTTONS_DIAGNOSTIC_MAGIC;
    g_bsp_buttons.version = BSP_BUTTONS_DIAGNOSTIC_VERSION;
    g_bsp_buttons.last_process_ms = now;
    g_bsp_buttons.irq_raw_pressed_mask = g_bsp_buttons.raw_pressed_mask;
    g_bsp_buttons.initialized = 1U;
    UnlockIRQ(mask);
}

void BSP_Buttons_IRQHandler(uint16_t pin)
{
    if (g_bsp_buttons.initialized == 0U) { return; }
    uint32_t mask = LockIRQ();
    for (uint32_t index = 0U; index < BSP_BUTTON_COUNT; ++index) {
        if ((pin & button_pins[index].pin) == 0U) { continue; }
        uint32_t bit = BSP_BUTTONS_MASK(index);
        ++g_bsp_buttons.irq_count;
        ++g_bsp_buttons.irq_per_button[index];
        g_bsp_buttons.irq_pending_mask |= bit;
        if (ReadPressed(index) != 0U) { g_bsp_buttons.irq_raw_pressed_mask |= bit; }
        else { g_bsp_buttons.irq_raw_pressed_mask &= ~bit; }
    }
    UnlockIRQ(mask);
}

void BSP_Buttons_Process(void)
{
    if (g_bsp_buttons.initialized == 0U) { return; }
    uint32_t now = HAL_GetTick();
    uint32_t delta = now - previous_process_ms;
    previous_process_ms = now;
    uint32_t mask = LockIRQ();
    g_bsp_buttons.last_processed_irq_mask = g_bsp_buttons.irq_pending_mask;
    g_bsp_buttons.irq_pending_mask = 0U;
    UnlockIRQ(mask);

    uint32_t pressed_mask = 0U;
    uint32_t raw_mask = 0U;
    for (uint32_t index = 0U; index < BSP_BUTTON_COUNT; ++index) {
        ButtonState *state = &buttons[index];
        uint32_t raw = ReadPressed(index);
        if (raw != 0U) { raw_mask |= BSP_BUTTONS_MASK(index); }

        /* release 후보 중에도 내부 총경과 시간은 진행한다. bounce로 다시 LOW가
         * 되면 같은 누름이 이어진 것이므로 그 간격까지 포함한 기간으로 복귀한다.
         * 공개 duration/threshold만 candidate_hold_ms로 고정하여 거짓 long을 막는다. */
        if (state->stable_pressed != 0U) {
            state->held_elapsed_ms = SaturatingAdd(state->held_elapsed_ms, delta);
        }
        if (raw != state->candidate_pressed) {
            state->candidate_pressed = raw;
            state->candidate_since_ms = now;
            state->candidate_hold_ms = state->held_elapsed_ms;
        }
        uint32_t candidate_age = now - state->candidate_since_ms;

        if (state->stable_pressed == 0U && raw != 0U
            && candidate_age >= BSP_BUTTONS_DEBOUNCE_MS) {
            state->stable_pressed = 1U;
            state->press_tick_ms = state->candidate_since_ms;
            state->held_elapsed_ms = candidate_age;
            state->long_sent = 0U;
            state->very_long_sent = 0U;
            ++g_bsp_buttons.press_count[index];
            Emit(index, BSP_BUTTON_EVENT_PRESS, state->press_tick_ms, 0U,
                 BSP_BUTTON_CLASS_NONE);
        }

        if (state->stable_pressed != 0U) {
            uint32_t duration = raw != 0U ? state->held_elapsed_ms : state->candidate_hold_ms;
            state->current_duration_ms = duration;
            /* 이 순서면 지연된 Process가 release와 임계시간을 한 번에 발견해도
             * 임계 이벤트가 빠지지 않는다. raw release 뒤 debounce는 duration에서 뺀다. */
            EmitThresholds(index, duration);
            if (raw == 0U && candidate_age >= BSP_BUTTONS_DEBOUNCE_MS) {
                state->last_duration_ms = duration;
                state->last_classification = Classify(duration);
                state->stable_pressed = 0U;
                state->current_duration_ms = 0U;
                ++g_bsp_buttons.release_count[index];
                Emit(index, BSP_BUTTON_EVENT_RELEASE, state->candidate_since_ms, duration,
                     state->last_classification);
                if (state->last_classification == BSP_BUTTON_CLASS_SHORT) {
                    Emit(index, BSP_BUTTON_EVENT_SHORT_PRESS, state->candidate_since_ms,
                         duration, BSP_BUTTON_CLASS_SHORT);
                }
            }
        }
        if (state->stable_pressed != 0U) { pressed_mask |= BSP_BUTTONS_MASK(index); }
        g_bsp_buttons.current_duration_ms[index] = state->current_duration_ms;
        g_bsp_buttons.last_duration_ms[index] = state->last_duration_ms;
        g_bsp_buttons.last_classification[index] = (uint32_t)state->last_classification;
    }
    g_bsp_buttons.pressed_mask = pressed_mask;
    g_bsp_buttons.raw_pressed_mask = raw_mask;
    g_bsp_buttons.last_process_ms = now;
    ++g_bsp_buttons.process_count;
}

uint32_t BSP_Buttons_GetEvent(BSP_Buttons_Event *event)
{
    if (event == 0 || queue_count == 0U) { return 0U; }
    *event = event_queue[queue_read];
    queue_read = (queue_read + 1U) % BSP_BUTTONS_EVENT_QUEUE_SIZE;
    --queue_count;
    g_bsp_buttons.events_pending = queue_count;
    return 1U;
}

void BSP_Buttons_ClearEvents(void)
{
    queue_read = 0U;
    queue_count = 0U;
    g_bsp_buttons.events_pending = 0U;
}

uint32_t BSP_Buttons_GetOverflowCount(void) { return g_bsp_buttons.event_overflow_count; }
uint32_t BSP_Buttons_GetPressedMask(void) { return g_bsp_buttons.pressed_mask; }

/* 잘못된 enum을 배열 첨자로 쓰지 않는다. 초기화 전에는 BSS의 released/0/NONE을
 * 읽지만, 실제 운용 계약은 Init 이후 같은 서비스 태스크에서 읽는 것이다. */
uint32_t BSP_Buttons_IsPressed(BSP_Buttons_Button button)
{
    return (uint32_t)button < BSP_BUTTON_COUNT ? buttons[button].stable_pressed : 0U;
}

uint32_t BSP_Buttons_GetCurrentDurationMs(BSP_Buttons_Button button)
{
    return (uint32_t)button < BSP_BUTTON_COUNT ? buttons[button].current_duration_ms : 0U;
}

uint32_t BSP_Buttons_GetLastDurationMs(BSP_Buttons_Button button)
{
    return (uint32_t)button < BSP_BUTTON_COUNT ? buttons[button].last_duration_ms : 0U;
}

BSP_Buttons_Classification BSP_Buttons_GetLastClassification(BSP_Buttons_Button button)
{
    return (uint32_t)button < BSP_BUTTON_COUNT ? buttons[button].last_classification
                                              : BSP_BUTTON_CLASS_NONE;
}

uint32_t BSP_Buttons_GetState(BSP_Buttons_Button button, BSP_Buttons_State *snapshot)
{
    if ((uint32_t)button >= BSP_BUTTON_COUNT || snapshot == 0) { return 0U; }
    ButtonState *state = &buttons[button];
    snapshot->pressed = state->stable_pressed;
    snapshot->raw_pressed = state->candidate_pressed;
    snapshot->duration_ms = state->current_duration_ms;
    snapshot->last_duration_ms = state->last_duration_ms;
    snapshot->last_classification = state->last_classification;
    snapshot->long_event_sent = state->long_sent;
    snapshot->very_long_event_sent = state->very_long_sent;
    return 1U;
}
