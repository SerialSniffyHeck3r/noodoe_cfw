#ifndef BUTTON_EVENTS_H
#define BUTTON_EVENTS_H
#include <stdint.h>
typedef struct {uint32_t button,type,timestamp_ms,duration_ms,sequence;} ButtonEvent;
typedef void (*ButtonEventHandler)(const ButtonEvent *event,void *context);
/* One UI owner task pumps the existing debounced BSP queue. Subscribers see
 * the same immutable event, without competing consumers or application rules
 * in the ISR/driver. Callbacks must be bounded/nonblocking; no recursive pump. */
void ButtonEvents_Init(void);
uint32_t ButtonEvents_Subscribe(ButtonEventHandler handler,void *context);
void ButtonEvents_Unsubscribe(ButtonEventHandler handler,void *context);
void ButtonEvents_Process(void);
void ButtonEvents_Publish(const ButtonEvent *event);
#endif
