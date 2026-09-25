#ifndef NOODOE_ROLLBACK_VIEW_H
#define NOODOE_ROLLBACK_VIEW_H
#include "lvgl.h"
uint32_t RollbackView_Create(lv_obj_t *screen);
void RollbackView_Render(uint32_t visible,uint32_t failed,uint32_t restored,uint32_t reason,uint32_t log_id);
#endif
