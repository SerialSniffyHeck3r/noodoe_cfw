#ifndef NOODOE_UI_STATE_H
#define NOODOE_UI_STATE_H
#include <stdint.h>
#include "Ui_Dashboard.h"
#include "Phone_Calls.h"

/* 전체 제품의 순수 상태 계약. ms는 동일한 uint32 단조 시계이며 한번에
 * 2^31ms 이상 건너뛰지 않는다. 한 UI task만 소유하고 ISR/LVGL/BSP/저장을
 * 참조하지 않는다. Dispatch는 상태+effect만 변경하며 장치 작업을 실행하지 않는다. */
#define UI_BIT(x) (1UL << (x))
#define UI_EFFECT_CAPACITY 16U
#define UI_LONG_MS 800U
#define UI_EXIT_MS 3000U
/* Preserve existing diagnostic values; the final OFF stage is appended. */
typedef enum { UI_BOOT, IGN_STARTING, IGN_ON, IGN_STOPPING,
    OFF_DISPLAY_HOLD, OFF_BT_HOLD, UI_FAULT, OFF_DEEP_SLEEP } UiPower;
#define IGN_OFF_AWAKE OFF_DISPLAY_HOLD
#define IGN_OFF_SLEEPING OFF_BT_HOLD
#define UI_OFF_STAGE_DISPLAY 1U
#define UI_OFF_STAGE_BT 2U
#define UI_OFF_STAGE_DEEP 4U
#define UI_OFF_STAGE_ALL 7U
#define UI_WELCOME IGN_STARTING
#define UI_RUNNING IGN_ON
#define UI_STOP_SUMMARY IGN_STOPPING
#define UI_PHONE_WAIT IGN_OFF_AWAKE
#define UI_OFF IGN_OFF_SLEEPING
/* Physical OFF becomes a committed shutdown only after1000ms unchanged. */
#define UI_IGN_OFF_DELAY_MS 1000U
/* HIDING/SETTLING remain reserved for diagnostic enum compatibility. Ring
 * exit and summary fade now start together in SUMMARY after the1s delay. */
typedef enum { UI_OFF_DELAY, UI_OFF_HIDING, UI_OFF_SETTLING, UI_OFF_SUMMARY } UiOffPhase;
/* One permanent speed shell: OBD is a child card, never another home.
 * Link bit1 belonged to retired external GPS; never reinterpret it as a phone. */
#define UI_LINK_PHONE1 1U
#define UI_LINK_OBD 4U
#define UI_LINK_PHONE2 8U
#define UI_LINK_PHONES UI_LINK_PHONE1
#define UI_LINK_SUPPORTED UI_LINK_PHONE1
typedef enum { UI_MENU_NONE, UI_MENU_SETTINGS, UI_MENU_TIME,
    UI_MENU_BLUETOOTH, UI_MENU_LANGUAGE, UI_MENU_VEHICLE, UI_MENU_DISPLAY,
    UI_MENU_POWER, UI_MENU_QUICK, UI_MENU_VEHICLE_INFO, UI_MENU_NOTIFICATIONS,
    UI_MENU_PAIR_PHONE, UI_MENU_PAIR_OBD, UI_MENU_COUNT } UiMenu;
typedef enum { UI_MODAL_NONE, UI_MODAL_EDIT, UI_MODAL_CONFIRM } UiModal;
typedef enum { UI_WARN_NONE, UI_WARN_FUEL, UI_WARN_OIL, UI_WARN_BELT, UI_WARN_SERV } UiWarning;
typedef enum { UI_WARN_IDLE, UI_WARN_BLINK, UI_WARN_MOVE, UI_WARN_TEXT } UiWarningPhase;
typedef enum { UI_UP, UI_DOWN, UI_ENTER } UiButton;
typedef enum { UI_MEDIA_TOGGLE,UI_MEDIA_PREVIOUS,UI_MEDIA_NEXT,UI_MEDIA_PHONE } UiMediaAction;
typedef enum { UI_PRESS=1, UI_RELEASE, UI_SHORT, UI_LONG, UI_VERY_LONG } UiButtonEvent;
typedef enum { UI_EVT_TICK, UI_EVT_IGN, UI_EVT_LINKS, UI_EVT_BUTTON,
    UI_EVT_RESERVE_ENTER, UI_EVT_REFUEL, UI_EVT_MAINTENANCE,
    UI_EVT_EFFECT_DONE, UI_EVT_SPEED, UI_EVT_PHONE_COUNT, UI_EVT_FAULT,
    UI_EVT_RING_HIDDEN, UI_EVT_DISPLAY_READY } UiEventKind;
/* Commands require an adapter acknowledgement; SCREEN events are desired
 * display state. epoch/id distinguish old sessions and duplicate completions. */
typedef enum { UI_FX_SCREEN, UI_FX_SESSION_START, UI_FX_SESSION_END,
    UI_FX_BLOCK_EXTERNAL, UI_FX_ALLOW_EXTERNAL, UI_FX_DISCONNECT_EXTERNAL,
    UI_FX_SAVE, UI_FX_POWER_OFF, UI_FX_REMOTE, UI_FX_RESET_TRIP,
    UI_FX_MAINT_RESET, UI_FX_SETTING, UI_FX_PAIR, UI_FX_MEDIA,
    UI_FX_RESERVE_START, UI_FX_RESERVE_CLEAR, UI_FX_OPEN_EDITOR, UI_FX_PHONE_REPLY, UI_FX_CALL, UI_FX_COUNT } UiEffectKind;
typedef struct { uint32_t kind,arg,value,id,epoch; } UiEffect;
typedef struct { uint32_t kind,now_ms,a,b,c,d; } UiEvent;
typedef struct {
    uint32_t card_mask,footer_mask;
    uint32_t welcome_ms,summary_ms,standby_ms,bt_retention_ms;
    uint32_t preferred_card,preferred_footer,boot_held_mask;
    uint32_t reserve_active; /* Restored domain latch; never derived from a UI label. */
    uint32_t off_stage_mask; /* UI_OFF_STAGE_*; at least one stage stays enabled. */
} UiConfig;
typedef struct {
    UiConfig config;
    uint32_t power,menu,selection,modal;
    UiDashboardState dashboard;
    UiCalls calls;
    uint32_t warning,warning_phase,warning_ms,pending_warnings;
    uint32_t reserve_active,maintenance_due,maintenance_shown;
    uint32_t ign_known,ign_on,links,epoch,revision,entered_ms,off_ms;
    uint32_t saved,save_id,off_id,last_error,ignored_completions;
    uint32_t display_asleep,welcome_active;
    uint32_t session_open,off_phase,off_phase_ms;
    uint32_t blocked_buttons,pressed_buttons,consumed_buttons,remote_press_mask;
    uint32_t press_ms[3],remote_token[3],context_token;
    uint32_t edit_key,edit_value,edit_min,edit_max,edit_step,confirm_kind,confirm_arg;
    uint32_t pending_id,pending_kind,pending_value,edit_focus;
    uint32_t now_ms,pending_since,save_since,off_since;
    uint32_t speed_valid,speed_kph,notification_count;
    uint32_t notification_open,notification_reply,notification_id,reply_selection,reply_count;
    uint32_t reply_id,reply_revision,reply_config,reply_sending,reply_sequence;
    uint32_t effect_head,effect_count,next_id,effect_overflows;
    UiEffect effects[UI_EFFECT_CAPACITY];
    /* Ring self-test and Welcome are different policies. A lit standby wake
     * runs the sweep; only a dark-panel wake runs Welcome. */
    uint32_t startup_sweep,display_wait;
} UiState;

/* Defaults describe one dashboard and its central cards. Device facts never
 * create a new top-level screen or silently steal the currently selected card. */
UiConfig Ui_DefaultConfig(void);
void Ui_Init(UiState *state,const UiConfig *config,uint32_t now_ms);
void Ui_Dispatch(UiState *state,const UiEvent *event);
uint32_t Ui_TakeEffect(UiState *state,UiEffect *out);
/* Stable IDs, with retired OBD excluded from the active seven-card mask. */
uint32_t Ui_CardAvailable(const UiState *state,uint32_t card);
uint32_t Ui_AvailableCards(const UiState *state);
/* Retired editor API: always returns0 without modifying state.
 * SettingsUI and AppSettings are the sole settings/commit route. */
uint32_t Ui_BeginEdit(UiState *state,uint32_t key,uint32_t value,
    uint32_t minimum,uint32_t maximum,uint32_t step);
#endif
