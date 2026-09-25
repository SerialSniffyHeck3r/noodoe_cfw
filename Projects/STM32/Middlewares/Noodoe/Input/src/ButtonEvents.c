#include "ButtonEvents.h"
#include "BSP_Buttons.h"
#include <string.h>
static struct {ButtonEventHandler handler;void *context;} listeners[4];
static uint32_t publishing;
void ButtonEvents_Init(void){memset(listeners,0,sizeof(listeners));publishing=0;}
/* Registration/removal is owner-context only and cannot mutate iteration. A
 * duplicate subscription is idempotent, not a second physical key action. */
uint32_t ButtonEvents_Subscribe(ButtonEventHandler handler,void *context)
{
    if(!handler||publishing)return 0;
    for(uint32_t i=0;i<4;++i)if(listeners[i].handler==handler&&listeners[i].context==context)return 1;
    for(uint32_t i=0;i<4;++i)if(!listeners[i].handler){listeners[i].handler=handler;listeners[i].context=context;return 1;}
    return 0;
}
void ButtonEvents_Unsubscribe(ButtonEventHandler handler,void *context)
{if(!publishing)for(uint32_t i=0;i<4;++i)if(listeners[i].handler==handler&&listeners[i].context==context)listeners[i].handler=0;}
void ButtonEvents_Publish(const ButtonEvent *event)
{
    if(!event||publishing||event->button>=BSP_BUTTON_COUNT||event->type<BSP_BUTTON_EVENT_PRESS||event->type>BSP_BUTTON_EVENT_VERY_LONG_PRESS)return;
    publishing=1;for(uint32_t i=0;i<4;++i)if(listeners[i].handler)listeners[i].handler(event,listeners[i].context);publishing=0;
}
void ButtonEvents_Process(void)
{
    if(publishing)return;
    BSP_Buttons_Process();BSP_Buttons_Event source;
    while(BSP_Buttons_GetEvent(&source)){
        ButtonEvent event={source.button,source.type,source.timestamp_ms,source.duration_ms,source.sequence};ButtonEvents_Publish(&event);
    }
}
