#ifndef BUTTON_HINTS_H
#define BUTTON_HINTS_H
#include "lvgl.h"
/* Three rows in physical order, each mapped to a BSP button ID. NULL actions
 * have no marker/pulse. Assets are immutable24px Material Round A4 masks. */
typedef struct {uint32_t button;const lv_image_dsc_t *key,*tap,*hold;} ButtonHintBinding;
uint32_t ButtonHints_Create(lv_obj_t *shell);
/* Owner-task, no allocation after Create. A scope identifies a page's bindings;
 * same-scope artwork/icon updates preserve released-action feedback. Alpha
 * shares the existing page fade. This renders feedback, never runs an action. */
void ButtonHints_Update(const ButtonHintBinding rows[3],uint32_t scope,uint32_t alpha,uint32_t now_ms);
void ButtonHints_Reparent(lv_obj_t *parent);
/* Non-music pages expose only physical UP/O/DOWN keys. Both variants share
 * the middleware's five-second idle fade and never swallow the wake press. */
void ButtonHints_ShowKeys(uint32_t scope,uint32_t alpha,uint32_t now_ms);
#endif
