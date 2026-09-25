#ifndef POWER_VIEW_H
#define POWER_VIEW_H
#include "lvgl.h"
#define POWER_VIEW_NAME_CAPACITY 49U
typedef struct {uint32_t kind,alpha;int32_t offset_y;char distance[32],ride[32],unit[4],oil[8],rider_name[POWER_VIEW_NAME_CAPACITY];} PowerViewModel;
uint32_t PowerView_Create(lv_obj_t *parent);
void PowerView_Render(const PowerViewModel *model);
#endif
