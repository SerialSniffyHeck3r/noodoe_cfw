#ifndef UI_HOME_H
#define UI_HOME_H
#include "Dashboard_Pages.h"
#define UI_HOME_VIEWS 3U
#define UI_HOME_STRIP_TIMEOUT_MS 5000U
/* App semantics only: pure photo, date, then date plus live UART speed. */
void UiHome_Present(const UiState *s,const DashboardPageFacts *facts,DashboardPage *page);
uint32_t UiHome_StripVisible(uint32_t category,uint32_t now);
#endif
