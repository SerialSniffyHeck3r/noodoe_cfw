#include "ui_state_internal.h"

/* SettingsUI is the only settings/editor owner. The parent stores its open
 * flag for power/input arbitration; it must never run a second menu engine.
 * Holding a key across an overlay therefore cannot navigate the dashboard. */
void Ui_Navigate(UiState *s,uint32_t button,uint32_t hold)
{
    if(s->menu||s->modal)return;
    Ui_DashboardNavigate(s,button,hold);
}
