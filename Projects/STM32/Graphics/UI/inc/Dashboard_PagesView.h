#ifndef DASHBOARD_PAGES_VIEW_H
#define DASHBOARD_PAGES_VIEW_H
#include "lvgl.h"
#include "Dashboard_Pages.h"
typedef struct {uint32_t magic,seq,frames,transitions,active,key,target,alpha,elapsed_ms,axis;int32_t direction,outgoing_x,outgoing_y,incoming_x,incoming_y;} DashboardViewDiagnostics;
extern volatile DashboardViewDiagnostics g_dashboard_view;
/* Two persistent banks share the shell's fixed content viewport. All pages,
 * their submodes and future parent menus use the same transition component. */
uint32_t DashboardPagesView_Create(lv_obj_t *content);
void DashboardPagesView_Render(const DashboardPage *page,uint32_t now);
#endif
