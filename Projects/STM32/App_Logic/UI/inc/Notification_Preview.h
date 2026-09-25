#ifndef NOTIFICATION_PREVIEW_H
#define NOTIFICATION_PREVIEW_H
#include "Ui_State.h"
#include "Phone_Content.h"
typedef struct {
 uint32_t token,revision,count,seen[10][2];
 uint32_t active,started,card,selection;
} NotificationPreview;
/* UI owner only. Observe every snapshot, including while disabled, so changing
 * a setting never replays old history. Input cancellation keeps the user's page. */
void NotificationPreview_Input(NotificationPreview *p);
void NotificationPreview_Step(NotificationPreview *p,UiState *s,
 const PhoneContentSlot *phone,uint32_t enabled,uint32_t seconds,uint32_t allowed,uint32_t now);
#endif
