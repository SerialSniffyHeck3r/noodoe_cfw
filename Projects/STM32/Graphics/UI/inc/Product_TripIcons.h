#ifndef PRODUCT_TRIP_ICONS_H
#define PRODUCT_TRIP_ICONS_H
#include "lvgl.h"
#define PRODUCT_TRIP_ICON_COUNT 4U
enum {PRODUCT_TRIP_MAXIMUM,PRODUCT_TRIP_AVERAGE,PRODUCT_TRIP_MOTORCYCLE,PRODUCT_TRIP_SIGNPOST};
/* Arrow: pinned Google Material Icons Round. Average: mathematical circle/
 * slash fallback drawn by this project. Same24px/A4 EVE path as mode icons. */
const lv_image_dsc_t *Product_TripIcon(uint32_t icon);
#endif
