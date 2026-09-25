#include "test_support.h"
#include "Product_Input.h"
#include "ButtonEvents.h"
#include "ButtonFeedback.h"

volatile Graphics_Diagnostics g_graphics;
volatile BSP_Buttons_Diagnostics g_bsp_buttons;
volatile uint32_t g_test_case, g_test_failure_line, g_test_assertions;
osThreadId_t graphics_owner;
uint32_t lvgl_started, brightness;
lv_display_t *graphics_display;
lv_group_t *graphics_group;

static lv_display_t display;
static lv_group_t group;
static lv_indev_t devices[32];
static uint32_t device_count, deletes, resets, invalid_access, create_fail;
static uint32_t lv_deinits, cache_releases, panel_shutdowns;
static uint32_t raw_mask, pressed_mask, overflow, process_calls;
static uint32_t observed, observer_last_button, observer_last_event, observer_bad_context;
static uint32_t context_token;
static uint32_t ipsr, primask, basepri;
static osThreadId_t current_thread;
static BSP_Buttons_Event events[128];
static uint32_t event_read, event_write;
static UiState product;
static uint32_t product_now;

/* Same owner callback bridge as ProductUI_Button, using the actual reducer.
 * BSP debounce is tested separately; inject its real PRESS/RELEASE/SHORT
 * stream into the actual Graphics queue consumer, not straight into UI. */
static void ObserveProduct(const ButtonEvent *event,void *context)
{
    (void)ProductInput_Dispatch(context,event->button,event->type,event->duration_ms,product_now);
    product_now+=100U;
}

#define CHECK(value) do { ++g_test_assertions; if (!(value)) { \
    g_test_failure_line = __LINE__; return __LINE__; } } while (0)

void *memset(void *destination, int value, size_t length)
{
    unsigned char *p = destination;
    while (length--) { *p++ = (unsigned char)value; }
    return destination;
}
void *memcpy(void *destination, const void *source, size_t length)
{
    unsigned char *out = destination;
    const unsigned char *in = source;
    while (length--) { *out++ = *in++; }
    return destination;
}

/* 삭제한 객체의 RAM은 poison 상태로 남긴다. 오래된 pointer를 reset/delete
 * 하기만 해도 invalid_access에 남으므로 해제 후 접근을 놓치지 않는다. */
static uint32_t Live(lv_indev_t *device)
{
    if (!device || !device->alive) { ++invalid_access; return 0U; }
    return 1U;
}
lv_indev_t *lv_indev_create(void)
{
    if (create_fail) { return NULL; }
    if (device_count >= 32U) { ++invalid_access; return NULL; }
    lv_indev_t *device = &devices[device_count++];
    *device = (lv_indev_t){0};
    device->alive = 1U;
    return device;
}
void lv_indev_delete(lv_indev_t *device) { if (Live(device)) { device->alive = 0U; ++deletes; } }
void lv_indev_reset(lv_indev_t *device, lv_obj_t *object) { (void)object; if (Live(device)) { ++resets; } }
void lv_indev_set_type(lv_indev_t *device, lv_indev_type_t type) { if (Live(device)) { device->type = type; } }
void lv_indev_set_read_cb(lv_indev_t *device, lv_indev_read_cb_t cb) { if (Live(device)) { device->read_cb = cb; } }
void lv_indev_set_display(lv_indev_t *device, lv_display_t *value) { if (Live(device)) { device->display = value; } }
void lv_indev_set_group(lv_indev_t *device, lv_group_t *value) { if (Live(device)) { device->group = value; } }
void lv_deinit(void)
{
    ++lv_deinits;
    /* 실제 lv_deinit이 아직 등록된 indev를 삭제하는 것과 같은 수명 경계다. */
    for (uint32_t i = 0; i < device_count; ++i) { devices[i].alive = 0U; }
}
void Graphics_EveReleaseCache(void) { ++cache_releases; }
void BSP_Display_Shutdown(void) { ++panel_shutdowns; }
uint32_t __get_IPSR(void) { return ipsr; }
uint32_t __get_PRIMASK(void) { return primask; }
uint32_t __get_BASEPRI(void) { return basepri; }
osThreadId_t osThreadGetId(void) { return current_thread; }
Graphics_Status Graphics_RecordError(Graphics_Status error) { g_graphics.last_error = error; return error; }

/* BSP 큐는 이미 debounce된 이벤트다. 실제 debounce 시험은 buttons_host의
 * 책임이며 여기서는 RELEASE 다음 SHORT라는 실제 BSP 전달 순서를 주입한다. */
void BSP_Buttons_Init(void) { g_bsp_buttons.raw_pressed_mask = raw_mask; }
void BSP_Buttons_Process(void) { ++process_calls; }
uint32_t BSP_Buttons_GetPressedMask(void) { return pressed_mask; }
uint32_t BSP_Buttons_GetOverflowCount(void) { return overflow; }
uint32_t BSP_Buttons_GetEvent(BSP_Buttons_Event *event)
{
    if (event_read == event_write) { return 0U; }
    *event = events[event_read++];
    return 1U;
}
static void Event(BSP_Buttons_Button button, BSP_Buttons_EventType type)
{
    if (event_write >= 128U) { ++invalid_access; return; }
    events[event_write++] = (BSP_Buttons_Event){ .button = button, .type = type, .duration_ms = 100U };
}
static void Short(BSP_Buttons_Button button)
{
    Event(button, BSP_BUTTON_EVENT_PRESS);
    Event(button, BSP_BUTTON_EVENT_RELEASE);
    Event(button, BSP_BUTTON_EVENT_SHORT_PRESS);
}
static void Observe(uint32_t button, uint32_t event, uint32_t duration, void *context)
{
    ++observed;
    observer_last_button = button;
    observer_last_event = event;
    if (context != &context_token || duration != 100U) { ++observer_bad_context; }
}
static lv_indev_data_t Poll(void)
{
    lv_indev_data_t data = {0};
    lv_indev_t *device = Graphics_InputGetDevice();
    if (!Live(device) || !device->read_cb) { ++invalid_access; return data; }
    device->read_cb(device, &data);
    return data;
}
static void Fresh(uint32_t held)
{
    Graphics_InputDeinit();
    g_graphics = (Graphics_Diagnostics){0};
    raw_mask = pressed_mask = held;
    event_read = event_write = overflow = observed = process_calls = 0U;
    observer_bad_context = create_fail = 0U;
    Graphics_InputInit(&display, &group);
    Graphics_InputSetCallback(Observe, &context_token);
}
static void LifecycleState(void)
{
    current_thread = graphics_owner = 1U;
    ipsr = primask = basepri = 0U;
    lvgl_started = g_graphics.initialized = 1U;
    graphics_display = &display;
    graphics_group = &group;
    brightness = 25U;
}

int GraphicsInput_TestMain(void)
{
#if NOODOE_PRODUCT
    /* Product still forwards every BSP event with no LVGL encoder owner. */
    g_test_case=100;Fresh(0);CHECK(Graphics_InputGetDevice()==NULL);
    Short(BSP_BUTTON_UP);Short(BSP_BUTTON_DOWN);Short(BSP_BUTTON_ENTER);
    Graphics_InputProcess();CHECK(observed==9&&observer_bad_context==0);
    Graphics_InputSetEnabled(0);Short(BSP_BUTTON_ENTER);Graphics_InputProcess();
    CHECK(observed==12);Graphics_InputDeinit();CHECK(!invalid_access);
    Fresh(0);CHECK(Graphics_InputGetDevice()==NULL);Graphics_InputDeinit();return 0;
#endif
    lv_indev_data_t data;
    uint32_t count, reset_count;
    g_test_case = 1U;
    Fresh(0U);
    CHECK(Graphics_InputGetDevice() != NULL);
    CHECK(Graphics_InputGetDevice()->type == LV_INDEV_TYPE_ENCODER);
    CHECK(Graphics_InputGetDevice()->display == &display && Graphics_InputGetDevice()->group == &group);
    data = Poll(); CHECK(data.enc_diff == 0 && data.state == LV_INDEV_STATE_RELEASED);

    g_test_case = 2U;
    Fresh(BSP_BUTTONS_MASK(BSP_BUTTON_UP));
    Event(BSP_BUTTON_UP, BSP_BUTTON_EVENT_PRESS);
    Event(BSP_BUTTON_UP, BSP_BUTTON_EVENT_RELEASE);
    Event(BSP_BUTTON_UP, BSP_BUTTON_EVENT_SHORT_PRESS);
    pressed_mask = 0U;
    Graphics_InputProcess(); data = Poll();
    CHECK(data.enc_diff == 0);
    CHECK(observed == 3U && observer_bad_context == 0U);
    CHECK(g_graphics.input_boot_held_mask == BSP_BUTTONS_MASK(BSP_BUTTON_UP));
    Short(BSP_BUTTON_UP); Graphics_InputProcess(); data = Poll();
    CHECK(data.enc_diff == -1);
    data = Poll(); CHECK(data.enc_diff == 0);

    g_test_case = 3U;
    Fresh(0U);
    Event(BSP_BUTTON_DOWN, BSP_BUTTON_EVENT_RELEASE);
    Event(BSP_BUTTON_DOWN, BSP_BUTTON_EVENT_SHORT_PRESS);
    Graphics_InputProcess(); data = Poll(); CHECK(data.enc_diff == 0);
    Short(BSP_BUTTON_DOWN); Graphics_InputProcess(); data = Poll(); CHECK(data.enc_diff == 1);
    Event(BSP_BUTTON_DOWN, BSP_BUTTON_EVENT_SHORT_PRESS);
    Graphics_InputProcess(); data = Poll(); CHECK(data.enc_diff == 0);

    g_test_case = 4U;
    Fresh(BSP_BUTTONS_MASK(BSP_BUTTON_ENTER));
    Graphics_InputProcess(); data = Poll(); CHECK(data.state == LV_INDEV_STATE_RELEASED);
    Event(BSP_BUTTON_ENTER, BSP_BUTTON_EVENT_RELEASE);
    pressed_mask = 0U; Graphics_InputProcess();
    Event(BSP_BUTTON_ENTER, BSP_BUTTON_EVENT_PRESS);
    pressed_mask = BSP_BUTTONS_MASK(BSP_BUTTON_ENTER); Graphics_InputProcess();
    data = Poll(); CHECK(data.state == LV_INDEV_STATE_PRESSED && data.enc_diff == 0);
    Event(BSP_BUTTON_ENTER, BSP_BUTTON_EVENT_RELEASE);
    pressed_mask = 0U; Graphics_InputProcess(); data = Poll(); CHECK(data.state == LV_INDEV_STATE_RELEASED);

    g_test_case = 5U;
    Fresh(0U); Graphics_InputSetEnabled(0U);
    Short(BSP_BUTTON_DOWN); Short(BSP_BUTTON_UP);
    Event(BSP_BUTTON_ENTER, BSP_BUTTON_EVENT_PRESS);
    pressed_mask = BSP_BUTTONS_MASK(BSP_BUTTON_ENTER);
    Graphics_InputProcess(); data = Poll();
    CHECK(data.enc_diff == 0 && data.state == LV_INDEV_STATE_RELEASED);
    CHECK(observed == 7U && observer_last_button == BSP_BUTTON_ENTER && observer_last_event == BSP_BUTTON_EVENT_PRESS);
    Graphics_InputSetEnabled(1U); pressed_mask = 0U;
    Short(BSP_BUTTON_DOWN); Graphics_InputProcess(); data = Poll(); CHECK(data.enc_diff == 1);

    g_test_case = 6U;
    Fresh(0U);
    for (uint32_t i = 0; i < 33U; ++i) { Short(BSP_BUTTON_DOWN); }
    overflow = 5U; Graphics_InputProcess(); data = Poll();
    CHECK(data.enc_diff == 32 && g_graphics.input_dropped == 6U);
    CHECK(g_graphics.input_events == 99U && observed == 99U);

    g_test_case = 7U;
    count = deletes; reset_count = resets;
    Graphics_InputDeinit(); CHECK(deletes == count + 1U && Graphics_InputGetDevice() == NULL);
    Graphics_InputDeinit(); Graphics_InputSetEnabled(0U);
    CHECK(deletes == count + 1U && resets == reset_count && invalid_access == 0U);
    Fresh(0U); Short(BSP_BUTTON_UP); Graphics_InputProcess(); data = Poll(); CHECK(data.enc_diff == -1);

    g_test_case = 8U;
    Graphics_InputDeinit(); create_fail = 1U;
    Graphics_InputInit(&display, &group);
    CHECK(Graphics_InputGetDevice() == NULL && g_graphics.last_error == GRAPHICS_ERROR_MEMORY);
    Graphics_InputDeinit(); Graphics_InputSetEnabled(0U); CHECK(invalid_access == 0U);

    g_test_case = 9U;
    Fresh(0U); LifecycleState();
    count = deletes; reset_count = resets;
    CHECK(Graphics_Shutdown() == GRAPHICS_OK);
    CHECK(Graphics_InputGetDevice() == NULL && deletes == count + 1U);
    CHECK(lv_deinits == 1U && cache_releases == 1U && panel_shutdowns == 1U);
    CHECK(!lvgl_started && !g_graphics.initialized && !graphics_display && !graphics_group && !brightness);
    CHECK(Graphics_Shutdown() == GRAPHICS_OK);
    CHECK(lv_deinits == 1U && cache_releases == 1U && panel_shutdowns == 1U);
    CHECK(deletes == count + 1U && resets == reset_count && invalid_access == 0U);
    Fresh(0U); LifecycleState(); Short(BSP_BUTTON_DOWN); Graphics_InputProcess(); data = Poll(); CHECK(data.enc_diff == 1);
    CHECK(Graphics_Shutdown() == GRAPHICS_OK && lv_deinits == 2U && invalid_access == 0U);

    g_test_case = 10U;
    Fresh(0U); LifecycleState(); count = deletes;
    ipsr = 1U; CHECK(Graphics_Shutdown() == GRAPHICS_ERROR_CONTEXT); ipsr = 0U;
    primask = 1U; CHECK(Graphics_Shutdown() == GRAPHICS_ERROR_CONTEXT); primask = 0U;
    basepri = 1U; CHECK(Graphics_Shutdown() == GRAPHICS_ERROR_CONTEXT); basepri = 0U;
    current_thread = 2U; CHECK(Graphics_Shutdown() == GRAPHICS_ERROR_CONTEXT); current_thread = 1U;
    CHECK(deletes == count && Graphics_InputGetDevice()->alive == 1U);
    CHECK(Graphics_Shutdown() == GRAPHICS_OK && invalid_access == 0U);

    g_test_case=11U;
    Fresh(BSP_BUTTONS_MASK(BSP_BUTTON_UP));
    UiConfig config=Ui_DefaultConfig();config.boot_held_mask=UI_BIT(UI_UP);config.preferred_card=UI_TRIP;
    Ui_Init(&product,&config,0U);
    UiEvent ignition={UI_EVT_IGN,0U,1U,0U,0U,0U};Ui_Dispatch(&product,&ignition);
    UiEvent ready={UI_EVT_DISPLAY_READY,0U,product.epoch,0U,0U,0U};Ui_Dispatch(&product,&ready);
    UiEvent tick={UI_EVT_TICK,2400U,0U,0U,0U,0U};Ui_Dispatch(&product,&tick);
    product.dashboard.card=UI_TRIP; /* Input fixture starts after wake. */
    UiEffect effect;while(Ui_TakeEffect(&product,&effect)){}
    product_now=3000U;
    CHECK(ButtonEvents_Subscribe(ObserveProduct,&product));
    CHECK(ButtonEvents_Subscribe(ObserveProduct,&product)); /* Must be idempotent. */
    Graphics_InputSetEnabled(0U); /* Product disables only LVGL encoder routing. */
    CHECK(product.power==UI_RUNNING&&product.dashboard.card==UI_TRIP);
    /* A held/broken UP must not block the independent middle ENTER. */
    const uint32_t order[]={UI_NOTIFICATIONS,UI_MUSIC,
        UI_PHONE_GPS,UI_SYSTEM,UI_BLANK,UI_TRIP};
    for(uint32_t i=0;i<sizeof(order)/sizeof(order[0]);++i){
        Short(BSP_BUTTON_ENTER);Graphics_InputProcess();
        data=Poll();CHECK(data.enc_diff==0&&data.state==LV_INDEV_STATE_RELEASED);
        CHECK(product.dashboard.card==order[i]);
        CHECK(product.dashboard.footer==UI_ODO&&product.power==UI_RUNNING);
    }
    CHECK(product.blocked_buttons==UI_BIT(UI_UP));
    CHECK(observed==18U); /* Both graphics and app saw all6 PRESS/RELEASE/SHORT streams. */
    /* No PRESS means no new intent, even with repeated release/short packets. */
    Event(BSP_BUTTON_ENTER,BSP_BUTTON_EVENT_RELEASE);
    Event(BSP_BUTTON_ENTER,BSP_BUTTON_EVENT_SHORT_PRESS);
    Graphics_InputProcess();CHECK(product.dashboard.card==UI_TRIP);
    Short(BSP_BUTTON_DOWN);Graphics_InputProcess();
    CHECK(product.dashboard.card==UI_TRIP&&product.dashboard.footer==UI_ODO&&product.dashboard.selection==1);
    CHECK(!ProductInput_Dispatch(&product,99U,BSP_BUTTON_EVENT_PRESS,0U,product_now));
    CHECK(!ProductInput_Dispatch(&product,BSP_BUTTON_ENTER,99U,0U,product_now));
    /* OBD becomes a central card only after a real connection fact. */
    UiEvent link={UI_EVT_LINKS,product_now,UI_LINK_OBD,0U,0U,0U};Ui_Dispatch(&product,&link);
    for(uint32_t i=0;i<3U;++i){Short(BSP_BUTTON_ENTER);Graphics_InputProcess();}
    CHECK(product.dashboard.card==UI_PHONE_GPS&&product.dashboard.footer==UI_ODO);
    for(uint32_t i=0;i<3U;++i){Short(BSP_BUTTON_ENTER);Graphics_InputProcess();}CHECK(product.dashboard.card==UI_TRIP);
    /* A long middle press opens the trip context, and never also advances. */
    Short(BSP_BUTTON_UP);Graphics_InputProcess(); /* Clear boot-held UP only. */
    Event(BSP_BUTTON_ENTER,BSP_BUTTON_EVENT_PRESS);
    Event(BSP_BUTTON_ENTER,BSP_BUTTON_EVENT_LONG_PRESS);events[event_write-1U].duration_ms=2100U;
    Event(BSP_BUTTON_ENTER,BSP_BUTTON_EVENT_RELEASE);events[event_write-1U].duration_ms=2200U;
    Graphics_InputProcess();CHECK(product.dashboard.card==UI_TRIP&&!product.modal&&product.pending_id);
    CHECK(invalid_access==0U);
    g_test_case=12U;
    Fresh(BSP_BUTTONS_MASK(BSP_BUTTON_UP));ButtonFeedback_SetContext(4,1,1);
    ButtonEvent e={BSP_BUTTON_UP,BSP_BUTTON_EVENT_PRESS,100,0,1};ButtonEvents_Publish(&e);
    CHECK(ButtonFeedback_KeyPhase(BSP_BUTTON_UP,4)==0);
    e.type=BSP_BUTTON_EVENT_RELEASE;e.duration_ms=3000;ButtonEvents_Publish(&e);
    CHECK(!g_button_feedback.keys[BSP_BUTTON_UP].release_serial);
    e.type=BSP_BUTTON_EVENT_PRESS;e.duration_ms=0;ButtonEvents_Publish(&e);
    CHECK(ButtonFeedback_KeyPhase(BSP_BUTTON_UP,4)==1);
    CHECK(!ButtonFeedback_KeyPhase(BSP_BUTTON_UP,3));
    e.type=BSP_BUTTON_EVENT_LONG_PRESS;e.duration_ms=2001;ButtonEvents_Publish(&e);
    CHECK(ButtonFeedback_KeyPhase(BSP_BUTTON_UP,4)==2);
    e.type=BSP_BUTTON_EVENT_RELEASE;e.timestamp_ms=0xfffffff0U;e.duration_ms=2200;ButtonEvents_Publish(&e);
    CHECK(!ButtonFeedback_KeyPhase(BSP_BUTTON_UP,4));
    CHECK(ButtonFeedback_ActionPhase(BSP_BUTTON_UP,4,1,0xfffffff0U)==2);
    CHECK(ButtonFeedback_ActionPhase(BSP_BUTTON_UP,4,1,983)==2);
    CHECK(!ButtonFeedback_ActionPhase(BSP_BUTTON_UP,4,1,984));
    CHECK(!ButtonFeedback_ActionPhase(BSP_BUTTON_UP,4,0,0xfffffff0U));
    CHECK(!ButtonFeedback_ActionPhase(BSP_BUTTON_UP,3,1,0xfffffff0U));
    uint32_t released=g_button_feedback.keys[BSP_BUTTON_UP].release_serial;
    e.type=BSP_BUTTON_EVENT_SHORT_PRESS;ButtonEvents_Publish(&e);
    e.type=BSP_BUTTON_EVENT_RELEASE;ButtonEvents_Publish(&e);
    CHECK(g_button_feedback.keys[BSP_BUTTON_UP].release_serial==released);
    /* Changing pages while held consumes the gesture, rather than highlighting
     * a new page's action when the old physical press is released. */
    e.type=BSP_BUTTON_EVENT_PRESS;ButtonEvents_Publish(&e);ButtonFeedback_SetContext(5,2,1);
    CHECK(!ButtonFeedback_KeyPhase(BSP_BUTTON_UP,4));
    e.type=BSP_BUTTON_EVENT_RELEASE;ButtonEvents_Publish(&e);
    CHECK(g_button_feedback.keys[BSP_BUTTON_UP].release_serial==released);
    /* Release duration selects long even when a notification was missed. */
    e.button=BSP_BUTTON_ENTER;e.type=BSP_BUTTON_EVENT_PRESS;ButtonEvents_Publish(&e);
    e.type=BSP_BUTTON_EVENT_RELEASE;e.timestamp_ms=5000;e.duration_ms=2001;ButtonEvents_Publish(&e);
    CHECK(ButtonFeedback_ActionPhase(BSP_BUTTON_ENTER,5,1,5000)==2);
    ButtonFeedback_SetContext(5,3,1); /* e.g. switching phone retains the pulse. */
    CHECK(ButtonFeedback_ActionPhase(BSP_BUTTON_ENTER,5,1,5999)==2);
    CHECK(!ButtonFeedback_ActionPhase(BSP_BUTTON_ENTER,5,1,6000));
    e.button=BSP_BUTTON_DOWN;e.type=BSP_BUTTON_EVENT_PRESS;ButtonEvents_Publish(&e);
    e.type=BSP_BUTTON_EVENT_RELEASE;e.timestamp_ms=7000;e.duration_ms=2000;ButtonEvents_Publish(&e);
    CHECK(ButtonFeedback_ActionPhase(BSP_BUTTON_DOWN,5,0,7999)==1);
    CHECK(!ButtonFeedback_ActionPhase(BSP_BUTTON_DOWN,5,1,7999));
    ButtonFeedback_SetContext(5,4,0);e.type=BSP_BUTTON_EVENT_PRESS;ButtonEvents_Publish(&e);
    CHECK(!ButtonFeedback_KeyPhase(BSP_BUTTON_DOWN,5));
    e.type=BSP_BUTTON_EVENT_RELEASE;ButtonEvents_Publish(&e);
    CHECK(g_button_feedback.keys[BSP_BUTTON_DOWN].release_serial==0); /* Disabled scenes clear old pulses. */
    g_test_case=13U;
    Fresh(0);ButtonFeedback_SetContext(4,1,1);
    CHECK(ButtonFeedback_Visibility(100)==0);
    CHECK(ButtonFeedback_Visibility(220)==127);
    CHECK(ButtonFeedback_Visibility(340)==255);
    CHECK(ButtonFeedback_Visibility(5099)==255);
    CHECK(ButtonFeedback_Visibility(5100)==255); /* Start the idle fade. */
    CHECK(ButtonFeedback_Visibility(5220)==128);
    e=(ButtonEvent){BSP_BUTTON_DOWN,BSP_BUTTON_EVENT_PRESS,5220,0,1};ButtonEvents_Publish(&e);
    CHECK(ButtonFeedback_Visibility(5220)==128); /* No opacity jump on reversal. */
    CHECK(ButtonFeedback_Visibility(5340)==191);
    CHECK(ButtonFeedback_Visibility(5460)==255);
    CHECK(ButtonFeedback_Visibility(12000)==255); /* Held input never disappears. */
    e.type=BSP_BUTTON_EVENT_RELEASE;e.timestamp_ms=12000;ButtonEvents_Publish(&e);
    CHECK(ButtonFeedback_Visibility(16999)==255);
    CHECK(ButtonFeedback_Visibility(17000)==255);
    CHECK(ButtonFeedback_Visibility(17240)==0);
    /* Disabled scenes suppress both the overlay and release pulses. */
    ButtonFeedback_SetContext(5,2,0);e.type=BSP_BUTTON_EVENT_PRESS;e.timestamp_ms=18000;ButtonEvents_Publish(&e);
    CHECK(ButtonFeedback_Visibility(18000)==0);
    CHECK(ButtonFeedback_Visibility(18240)==0);
    CHECK(!ButtonFeedback_KeyPhase(BSP_BUTTON_DOWN,5));
    e.type=BSP_BUTTON_EVENT_RELEASE;e.timestamp_ms=18300;ButtonEvents_Publish(&e);
    CHECK(g_button_feedback.keys[BSP_BUTTON_DOWN].release_serial==0);
    uint32_t activity=g_button_feedback.activity_serial;ButtonEvents_Publish(&e);
    CHECK(g_button_feedback.activity_serial==activity);
    /* Broken boot-held input cannot repeatedly keep an idle overlay awake. */
    Fresh(1);ButtonFeedback_SetContext(4,1,1);CHECK(ButtonFeedback_Visibility(0)==0);CHECK(ButtonFeedback_Visibility(240)==255);
    e=(ButtonEvent){BSP_BUTTON_UP,BSP_BUTTON_EVENT_PRESS,4900,0,1};ButtonEvents_Publish(&e);
    CHECK(ButtonFeedback_Visibility(5000)==255);CHECK(ButtonFeedback_Visibility(5240)==0);
    Fresh(0);ButtonFeedback_SetContext(4,1,1);e=(ButtonEvent){BSP_BUTTON_DOWN,BSP_BUTTON_EVENT_PRESS,0xfffff000U,0,1};ButtonEvents_Publish(&e);
    CHECK(ButtonFeedback_Visibility(0xfffff000U)==0);CHECK(ButtonFeedback_Visibility(0xfffff0f0U)==255);
    e.type=BSP_BUTTON_EVENT_RELEASE;e.timestamp_ms=0xfffff100U;ButtonEvents_Publish(&e);
    CHECK(ButtonFeedback_Visibility(0x488)==255);CHECK(ButtonFeedback_Visibility(0x578)==0);
    return 0;
}
