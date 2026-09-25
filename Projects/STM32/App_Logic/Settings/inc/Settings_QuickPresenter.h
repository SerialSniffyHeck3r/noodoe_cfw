#ifndef SETTINGS_QUICK_PRESENTER_H
#define SETTINGS_QUICK_PRESENTER_H
#include "Settings_UI.h"
#include "Dashboard_Pages.h"
/* Copy current quick-setting values into the same immutable page model used
 * by all category transitions. No LVGL objects or device operations. */
void SettingsQuick_Present(DashboardPage *page,const SettingsUI *settings);
#endif
