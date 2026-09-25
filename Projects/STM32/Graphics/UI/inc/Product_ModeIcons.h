#ifndef PRODUCT_MODE_ICONS_H
#define PRODUCT_MODE_ICONS_H
#include "lvgl.h"
#define PRODUCT_MODE_ICON_COUNT 8U
/* Fixed page order: home, trip, phone, music, OBD, remote, phoneGPS, settings.
 * Separate from footer SERV/fuel icons; their enum/font/layout stay intact. */
const lv_image_dsc_t *Product_ModeIcon(uint32_t mode);
#endif
