#ifndef POPUP_NOTIFICATIONS_H
#define POPUP_NOTIFICATIONS_H
#include <stdint.h>
#define TOAST_MESSAGE_CAPACITY 64U
#define TOAST_MAX_SECONDS 60U
/* Nonblocking task-context facade. Copies printable ASCII,1..63 characters;
 * seconds1..60 includes fade-in/out. Returns1 accepted,0 invalid/busy/not ready.
 * An accepted new message replaces the active one on the next UI tick. No
 * allocation, sleep, device access or caller-buffer lifetime dependency. */
uint32_t ShowToastMessages(const char *message,uint32_t seconds);
uint32_t HideToastMessages(void);
typedef struct {
    uint32_t visible,elapsed_ms,duration_ms,revision;
    char text[TOAST_MESSAGE_CAPACITY];
} PopupNotificationSnapshot;
/* The mailbox is diagnostic/development RAM only. Writers publish command,
 * seconds,request_text first and request_id last, and wait forack_id. Commands
 *1=show,2=hide. UI alone updates visible/source/timing/text; source1=trip hint,
 *2=caller. The seq field covers active state, not an unfinished producer copy. */
typedef struct {
    uint32_t magic,version,seq,request_id,command,seconds,ack_id,result;
    uint32_t visible,elapsed_ms,duration_ms,source,shown_count,now_ms;
    char request_text[TOAST_MESSAGE_CAPACITY],text[TOAST_MESSAGE_CAPACITY];
} PopupNotificationDiagnostics;
extern volatile PopupNotificationDiagnostics g_popup_notifications;
/* Pure owner/model functions. Request requires caller serialization; public
 * application code uses Show/Hide above. NULL Request means hide. Process is
 * monotonic-time driven; trip_reset_context rising edge shows the3s hint.
 * Leaving that context cancels only its hint, never an unrelated caller toast. */
void PopupNotifications_Init(void);
uint32_t PopupNotifications_Request(const char *message,uint32_t seconds);
void PopupNotifications_Process(uint32_t now_ms,uint32_t trip_reset_context);
uint32_t PopupNotifications_Get(PopupNotificationSnapshot *snapshot);
#endif
