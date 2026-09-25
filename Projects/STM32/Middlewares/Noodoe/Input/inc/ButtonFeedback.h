#ifndef BUTTON_FEEDBACK_H
#define BUTTON_FEEDBACK_H
#include "ButtonEvents.h"
#define BUTTON_FEEDBACK_RELEASE_MS 1000U
#define BUTTON_HINT_IDLE_MS 5000U
#define BUTTON_HINT_FADE_MS 240U
typedef struct {
    uint32_t pressed,long_press,press_scope,press_generation;
    uint32_t released_ms,release_serial,release_long,release_scope;
} ButtonFeedbackKey;
typedef struct {
    uint32_t magic,version,blocked,scope,generation,enabled;
    ButtonFeedbackKey keys[3];
    uint32_t activity_ms,activity_serial,visibility_alpha,visibility_from;
    uint32_t visibility_target,visibility_start,visibility_initialized;
} ButtonFeedbackState;
extern ButtonFeedbackState g_button_feedback;
/* Owner task only. Context numbers are opaque application tokens. A context
 * change invalidates held intent without deleting the previous release pulse. */
void ButtonFeedback_Init(uint32_t boot_held_mask);
void ButtonFeedback_SetContext(uint32_t scope,uint32_t generation,uint32_t enabled);
void ButtonFeedback_Handle(const ButtonEvent *event);
uint32_t ButtonFeedback_KeyPhase(uint32_t button,uint32_t scope);
uint32_t ButtonFeedback_ActionPhase(uint32_t button,uint32_t scope,uint32_t hold,uint32_t now_ms);
/* Shared idle visibility; observes activity but does not consume a gesture or
 * enable an action. Presses while fading retarget from the current opacity. */
uint32_t ButtonFeedback_Visibility(uint32_t now_ms);
#endif
