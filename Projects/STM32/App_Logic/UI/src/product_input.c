#include "Product_Input.h"
#include "BSP_Buttons.h"
_Static_assert(UI_LONG_MS==BSP_BUTTONS_LONG_PRESS_MS,"UI action and feedback long thresholds must agree");

/* Keep the established board mapping: UP PD12, DOWN PI6, middle ENTER PA15.
 * Translate by named constants instead of silently assuming BSP and UI enums
 * keep identical numbers. Invalid inputs leave the state completely untouched. */
uint32_t ProductInput_Dispatch(UiState *state,uint32_t button,uint32_t event,
    uint32_t duration,uint32_t now)
{
    uint32_t key,type;
    if(!state)return 0U;
    switch(button){
    case BSP_BUTTON_UP:key=UI_UP;break;
    case BSP_BUTTON_DOWN:key=UI_DOWN;break;
    case BSP_BUTTON_ENTER:key=UI_ENTER;break;
    default:return 0U;
    }
    switch(event){
    case BSP_BUTTON_EVENT_PRESS:type=UI_PRESS;break;
    case BSP_BUTTON_EVENT_RELEASE:type=UI_RELEASE;break;
    case BSP_BUTTON_EVENT_SHORT_PRESS:type=UI_SHORT;break;
    case BSP_BUTTON_EVENT_LONG_PRESS:type=UI_LONG;break;
    case BSP_BUTTON_EVENT_VERY_LONG_PRESS:type=UI_VERY_LONG;break;
    default:return 0U;
    }
    /* Forward the whole stream once. UI_RELEASE is the decision point, so
     * the following BSP SHORT cannot advance a second card. LONG notifications
     * remain available for remote exit; the bridge never invents a short press. */
    UiEvent input={UI_EVT_BUTTON,now,key,type,duration,0U};
    Ui_Dispatch(state,&input);return 1U;
}
