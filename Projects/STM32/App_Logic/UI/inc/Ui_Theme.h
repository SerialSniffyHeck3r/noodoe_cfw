#ifndef UI_THEME_H
#define UI_THEME_H
#include <stdint.h>
typedef struct {uint32_t mode,source,day_minute,night_minute,dark_lux,light_lux;} UiThemeConfig;
typedef struct {uint32_t light,candidate,since,armed;} UiThemeState;
/* Invalid inputs retain the last decision. Caller advances only on frames
 * allowed by the power owner, preserving provisional IGN OFF's frozen frame. */
uint32_t UiTheme_Step(UiThemeState*,const UiThemeConfig*,uint32_t now,
 uint32_t clock_valid,uint32_t minute,uint32_t sensor_valid,uint32_t millilux);
#endif
