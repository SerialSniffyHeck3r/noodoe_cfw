#ifndef PRODUCT_MAINTENANCE_BAR_H
#define PRODUCT_MAINTENANCE_BAR_H
#include "lvgl.h"
#include <stdint.h>
/* Oil remaining arc: centered X240, bottom centerline Y458.5, 136px span.
 * Calendar/hour calculations remain in the model, with no auxiliary text. */
#define PRODUCT_MAINTENANCE_LINE_X 172
#define PRODUCT_MAINTENANCE_LINE_Y 454
#define PRODUCT_MAINTENANCE_LINE_WIDTH 136
#define PRODUCT_MAINTENANCE_LINE_HEIGHT 9
typedef struct {lv_obj_t *track,*fill;uint32_t visible,valid,ratio;} ProductMaintenanceBar;
uint32_t ProductMaintenanceBar_Create(ProductMaintenanceBar *bar,lv_obj_t *parent);
/* Graphics-owner only. Continuous0..1000 fill; no segments or heap work.
 * Distance modes and OIL show oil remaining; BELT/SERV hide this gauge. */
void ProductMaintenanceBar_Set(ProductMaintenanceBar *bar,uint32_t visible,uint32_t valid,uint32_t ratio);
#endif
