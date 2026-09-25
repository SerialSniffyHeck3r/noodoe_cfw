#ifndef WALLPAPER_RUNTIME_H
#define WALLPAPER_RUNTIME_H
#include "Dashboard_Pages.h"
/* Called once each product owner tick before LVGL renders. Takes a stable
 * copy of ephemeral phone art, applies App source policy, advances upload. */
void WallpaperRuntime_Process(const DashboardPage *page,const UiState *state);
/* Provisional OFF freezes the exact frame/source/brightness without I/O. */
void WallpaperRuntime_HoldShutdown(uint32_t hold);
#endif
