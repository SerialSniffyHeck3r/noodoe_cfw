#include "graphics_internal.h"
#include "BSP_Buttons.h"
#include "ButtonEvents.h"
#include "ButtonFeedback.h"
#include <stddef.h>

/* 세 물리 버튼을 LVGL encoder 입력으로 노출한다. 방향 SHORT 한 번은 한 칸,
 * ENTER는 실제 debounced press/release 상태다. 화면/시험 코드는 같은 BSP 큐를
 * 경쟁 소비하지 않고 아래 callback에서 모든 시간 이벤트를 받을 수 있다. */
static lv_indev_t *input_device;
static uint32_t input_enabled;
static uint32_t boot_held;
static uint32_t press_armed;
static uint32_t short_allowed;
static int32_t pending_steps;
static uint32_t enter_pressed;
static uint32_t step_overflow;
static Graphics_ButtonCallback observer;
static void *observer_context;
static void InputEvent(const ButtonEvent *incoming,void *context);

/* LVGL은 Graphics_Process의 timer-handler 안에서 이 콜백을 호출한다.
 * IRQ에서 LVGL을 부르지 않으며 큰 누적 입력은 signed overflow 전에 제한한다. */
#if !NOODOE_PRODUCT
static void InputRead(lv_indev_t *device, lv_indev_data_t *data)
{
    (void)device;
    data->enc_diff = input_enabled ? pending_steps : 0;
    data->state = input_enabled && enter_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    pending_steps = 0;
}
#endif

void Graphics_InputInit(lv_display_t *display, lv_group_t *group)
{
    BSP_Buttons_Init();ButtonEvents_Init();
    (void)ButtonEvents_Subscribe(InputEvent,NULL);
    boot_held = g_bsp_buttons.raw_pressed_mask;
    ButtonFeedback_Init(boot_held);
    press_armed = short_allowed = 0U;
    g_graphics.input_boot_held_mask = boot_held;
    pending_steps = 0;
    enter_pressed = 0U;
    step_overflow = 0U;
    observer = NULL;
    observer_context = NULL;
    input_enabled = 1U;
#if NOODOE_PRODUCT
    /* ProductInput/SettingsUI already consume the global button events.
     * An unused LVGL encoder created a second input state machine, including
     * pointer/scroll processing, then ProductUI disabled it immediately. */
    (void)display;(void)group;input_enabled=0;
#else
    input_device = lv_indev_create();
    if (!input_device) { Graphics_RecordError(GRAPHICS_ERROR_MEMORY); return; }
    lv_indev_set_type(input_device, LV_INDEV_TYPE_ENCODER);
    lv_indev_set_read_cb(input_device, InputRead);
    lv_indev_set_display(input_device, display);
    lv_indev_set_group(input_device, group);
#endif
}

static void InputEvent(const ButtonEvent *incoming,void *context)
{
    (void)context;ButtonEvent event=*incoming;
        const uint32_t button = (uint32_t)event.button;
        const uint32_t bit = 1UL << button;
        const uint32_t was_blocked = boot_held & bit;
        ++g_graphics.input_events;
        /* 부팅 때 LOW였던 버튼은 첫 release까지 UI 이동 입력으로 쓰지 않는다.
         * BSP 원시 진단은 그대로 남기므로 하드웨어 고장을 정상으로 숨기지 않는다. */
        if (was_blocked) {
            press_armed &= ~bit;
            short_allowed &= ~bit;
        } else if (event.type == BSP_BUTTON_EVENT_PRESS) {
            press_armed |= bit;
            short_allowed &= ~bit;
        }
        if (event.type == BSP_BUTTON_EVENT_RELEASE) {
            /* RELEASE 직후 같은 큐에 SHORT가 들어온다. 부팅 당시 누름을
             * 놓은 것만으로 SHORT를 허용하지 않고 새 PRESS를 요구한다. */
            if (!was_blocked && (press_armed & bit)) { short_allowed |= bit; }
            press_armed &= ~bit;
            boot_held &= ~bit;
        }
        if (!was_blocked && input_enabled) {
            if (event.type == BSP_BUTTON_EVENT_SHORT_PRESS && (short_allowed & bit) && button != BSP_BUTTON_ENTER) {
                int32_t step = button == BSP_BUTTON_UP ? -1 : 1;
                if (pending_steps > -32 && pending_steps < 32) { pending_steps += step; }
                else { ++step_overflow; }
            }
        }
        if (event.type == BSP_BUTTON_EVENT_SHORT_PRESS) { short_allowed &= ~bit; }
        if (observer) { observer(button, (uint32_t)event.type, event.duration_ms, observer_context); }
}
void Graphics_InputProcess(void)
{
    ButtonEvents_Process();
    g_graphics.input_pressed_mask=BSP_Buttons_GetPressedMask();
    enter_pressed = ((BSP_Buttons_GetPressedMask() & ~boot_held & BSP_BUTTONS_MASK(BSP_BUTTON_ENTER)) != 0U);
    g_graphics.input_dropped = BSP_Buttons_GetOverflowCount() + step_overflow;
}

lv_indev_t *Graphics_InputGetDevice(void) { return input_device; }
/* lv_deinit 전 입력 객체를 반환하고 모든 소유 포인터를 지운다. init 실패 뒤
 * 재시도 또는 중복 shutdown에서도 freed indev를 다시 reset하지 않는다. */
void Graphics_InputDeinit(void)
{
    ButtonEvents_Unsubscribe(InputEvent,NULL);
    observer = NULL;
    observer_context = NULL;
    input_enabled = 0U;
    if (input_device) { lv_indev_delete(input_device); input_device = NULL; }
    pending_steps = 0;
    enter_pressed = press_armed = short_allowed = boot_held = 0U;
}
void Graphics_InputSetEnabled(uint32_t enabled)
{
    input_enabled = enabled != 0U;
    pending_steps = 0;
    enter_pressed = 0U;
    if (input_device) { lv_indev_reset(input_device, NULL); }
}
void Graphics_InputSetCallback(Graphics_ButtonCallback callback, void *context)
{
    observer = callback;
    observer_context = context;
}
