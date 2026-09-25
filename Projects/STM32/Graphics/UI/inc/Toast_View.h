#ifndef PRODUCT_TOAST_VIEW_H
#define PRODUCT_TOAST_VIEW_H
#include "lvgl.h"
#include "Popup_Notifications.h"
/* One persistent overlay inside the page viewport, above its animated banks.
 * It never changes page layout or captures button input. EVE opacity is set
 * on individual background/border/text primitives, never an offscreen layer. */
uint32_t ToastView_Create(lv_obj_t *content);
void ToastView_Render(const PopupNotificationSnapshot *toast);
#endif
