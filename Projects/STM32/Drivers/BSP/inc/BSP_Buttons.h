#ifndef BSP_BUTTONS_H
#define BSP_BUTTONS_H

#include <stdint.h>

#define BSP_BUTTONS_DIAGNOSTIC_MAGIC   0x42544E31UL /* BTN1 */
#define BSP_BUTTONS_DIAGNOSTIC_VERSION 1U

/* 모든 시간 단위는 ms다. 순정은 release후 >20ms, decision elapsed >80ms,
 * long elapsed >2000ms를 사용한다. 따라서 >= 비교용 long 기본값은 2001이다.
 * press debounce와 very-long 이벤트는 이 BSP의 확장이다. 순정의 long 판정이
 * release 대기시간까지 누적하는 경계 오류는 복제하지 않고 hold 기간을 동결한다. */
#ifndef BSP_BUTTONS_DEBOUNCE_MS
#define BSP_BUTTONS_DEBOUNCE_MS       21U
#endif
#ifndef BSP_BUTTONS_MIN_DECISION_MS
#define BSP_BUTTONS_MIN_DECISION_MS   80U
#endif
#ifndef BSP_BUTTONS_LONG_PRESS_MS
#define BSP_BUTTONS_LONG_PRESS_MS     800U
#endif
#ifndef BSP_BUTTONS_VERY_LONG_PRESS_MS
#define BSP_BUTTONS_VERY_LONG_PRESS_MS 3000U
#endif
#ifndef BSP_BUTTONS_EVENT_QUEUE_SIZE
#define BSP_BUTTONS_EVENT_QUEUE_SIZE  32U
#endif

typedef enum {
    BSP_BUTTON_UP = 0,
    BSP_BUTTON_DOWN = 1,
    BSP_BUTTON_ENTER = 2,
    BSP_BUTTON_COUNT = 3
} BSP_Buttons_Button;

#define BSP_BUTTONS_MASK(button) (1UL << (uint32_t)(button))

typedef enum {
    BSP_BUTTON_CLASS_NONE = 0,
    BSP_BUTTON_CLASS_SHORT = 1,
    BSP_BUTTON_CLASS_LONG = 2,
    BSP_BUTTON_CLASS_VERY_LONG = 3
} BSP_Buttons_Classification;

typedef enum {
    BSP_BUTTON_EVENT_PRESS = 1,
    BSP_BUTTON_EVENT_RELEASE = 2,
    BSP_BUTTON_EVENT_SHORT_PRESS = 3,
    BSP_BUTTON_EVENT_LONG_PRESS = 4,
    BSP_BUTTON_EVENT_VERY_LONG_PRESS = 5
} BSP_Buttons_EventType;

/* 이벤트는 Process()가 발견한 순서대로 큐에 들어간다. 같은 Process에서 여러
 * 버튼을 처리한 순서는 UP, DOWN, ENTER이며 서로의 상태를 초기화하지 않는다.
 * timestamp_ms는 논리적 엣지/임계 시각(HAL tick, wrap 가능), duration_ms는
 * 실제 확인된 누름 시간이다. sequence는 큐 삽입 시도마다 증가하므로 overflow로
 * 빠진 이벤트를 sequence의 공백으로도 발견할 수 있다. */
typedef struct {
    BSP_Buttons_Button button;
    BSP_Buttons_EventType type;
    uint32_t timestamp_ms;
    uint32_t duration_ms;
    BSP_Buttons_Classification classification;
    uint32_t sequence;
} BSP_Buttons_Event;

/* GetState()는 마지막 Process 결과를 복사한다. duration_ms는 release debounce
 * 중에도 최초 release 후보 시각에서 멈추며, 확정 release 뒤에는 0이다.
 * last_duration_ms와 last_classification은 다음 release까지 보존된다.
 * threshold 플래그는 가장 최근 누름에서 해당 이벤트를 이미 시도했는지 나타낸다. */
typedef struct {
    uint32_t pressed;
    uint32_t raw_pressed;
    uint32_t duration_ms;
    uint32_t last_duration_ms;
    BSP_Buttons_Classification last_classification;
    uint32_t long_event_sent;
    uint32_t very_long_event_sent;
} BSP_Buttons_State;

/* SWD에서 별도 타입 해석 없이 읽도록 진단 구조체는 uint32_t만 사용한다.
 * irq_*는 ISR에서, 그 밖은 단일 서비스 태스크에서 갱신한다. 디버거가 읽는 중
 * 필드가 갱신될 수 있으므로 전체 구조체의 원자적 스냅샷이라고 해석하지 않는다. */
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t initialized;
    uint32_t process_count;
    uint32_t last_process_ms;
    uint32_t pressed_mask;
    uint32_t raw_pressed_mask;
    uint32_t irq_count;
    uint32_t irq_pending_mask;
    uint32_t irq_raw_pressed_mask;
    uint32_t last_processed_irq_mask;
    uint32_t events_enqueued;
    uint32_t events_pending;
    uint32_t event_overflow_count;
    uint32_t event_sequence;
    uint32_t irq_per_button[BSP_BUTTON_COUNT];
    uint32_t press_count[BSP_BUTTON_COUNT];
    uint32_t release_count[BSP_BUTTON_COUNT];
    uint32_t current_duration_ms[BSP_BUTTON_COUNT];
    uint32_t last_duration_ms[BSP_BUTTON_COUNT];
    uint32_t last_classification[BSP_BUTTON_COUNT];
} BSP_Buttons_Diagnostics;

extern volatile BSP_Buttons_Diagnostics g_bsp_buttons;

/* 단일 태스크에서 HAL timebase 시작 후 호출한다. UP=PD12, DOWN=PI6,
 * ENTER=PA15만 active-low/NOPULL/양에지 EXTI로 설정하고 필요한 GPIO/SYSCFG
 * clock과 shared NVIC(15_10 priority5, 9_5 priority15)을 켠다. 다른 핀/EXTI,
 * 포트 reset/전원 출력은 바꾸지 않는다. 켜질 때 이미 눌린 버튼도 현재 관측
 * 시각부터 debounce한 뒤 PRESS를 발생시키며 전원 전의 누름 시간은 추측하지 않는다. */
void BSP_Buttons_Init(void);

/* 같은 태스크에서 약 5ms 주기로 호출한다. HAL_GetTick과 모든 버튼 GPIO를
 * 읽고 debounce/지속시간/이벤트를 처리한다. IRQ가 없거나 falling-only여도
 * release는 polling으로 발견한다. 이 함수 자체는 기다리거나 RTOS API를 부르지
 * 않는다. 두 호출 사이를 2^31ms 미만으로 유지한다. 관측 사이의 짧은 펄스는
 * 복원할 수 없으며, 누름/뗌 시간 정밀도는 실제 호출 주기에 제한된다. */
void BSP_Buttons_Process(void);

/* HAL_GPIO_EXTI_Callback(pin)에서 그대로 전달한다. 해당 active-low GPIO의
 * 원시 레벨과 IRQ 진단만 기록한다. debounce 시작 시각/이벤트/RTOS를 만지지
 * 않아 접점 떨림 IRQ가 유효한 후보의 안정화 시간을 계속 초기화하지 않는다. */
void BSP_Buttons_IRQHandler(uint16_t pin);

/* 나머지 API는 Init/Process와 같은 태스크에서 호출한다. ISR 및 여러 태스크의
 * 동시 소비를 지원하지 않는다. GetEvent는 성공 1/빈 큐 또는 NULL이면 0이다.
 * 큐가 차면 새 이벤트를 버리고 overflow를 증가시키며 기존 큐 순서는 보존한다.
 * 이벤트가 버려져도 버튼 상태·기간·분류는 계속 정확히 갱신한다. */
uint32_t BSP_Buttons_GetEvent(BSP_Buttons_Event *event);
void BSP_Buttons_ClearEvents(void);
uint32_t BSP_Buttons_GetOverflowCount(void);
uint32_t BSP_Buttons_GetPressedMask(void);
uint32_t BSP_Buttons_IsPressed(BSP_Buttons_Button button);
uint32_t BSP_Buttons_GetCurrentDurationMs(BSP_Buttons_Button button);
uint32_t BSP_Buttons_GetLastDurationMs(BSP_Buttons_Button button);
BSP_Buttons_Classification BSP_Buttons_GetLastClassification(BSP_Buttons_Button button);
uint32_t BSP_Buttons_GetState(BSP_Buttons_Button button, BSP_Buttons_State *state);

/* PRESS는 debounce 완료 후 한 번, LONG/VERY_LONG은 누르고 있는 동안 각각
 * 임계시간에 한 번 발생한다. 지연된 Process가 release까지 한번에 처리해도
 * 누락된 임계 이벤트를 먼저 보낸다. RELEASE는 항상 최종 duration/classification을
 * 싣는다. SHORT_PRESS는 short로 끝난 release 직후에만 발생한다. long/very-long
 * release에서 임계 이벤트를 중복 발생시키지 않으며 자동 반복 이벤트는 없다.
 * 순정 >80ms decision gate는 누름+논리적 release확인 21ms를 기준으로 한다.
 * Process가 늦게 호출되어도 추가 대기시간으로 짧은 입력을 SHORT로 승격하지 않는다.
 * gate 이하의 짧은 입력은 RELEASE classNONE으로 기록하고 SHORT_PRESS는 없다.
 * 저수준 BSP에는 순정 상위 정책인 PH9/IGN gate를 적용하지 않는다. */

#endif
