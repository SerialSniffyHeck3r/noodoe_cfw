#include "Product_Overlay.h"
#include "Rollback_View.h"
#include "Product_Fonts.h"
#include <stdio.h>
static lv_obj_t *panel,*label;
static char message[192];
static uint32_t previous[4];
/* Persistent, bounded center overlay. Display-only: storage and recovery
 * decisions remain outside Graphics. Four fixed lines fit the circular panel. */
uint32_t RollbackView_Create(lv_obj_t *screen)
{
 static const ProductOverlayLayout layout={60,150,360,200,8,8,344,184,16,255,0x101820};
 return ProductOverlay_Create(screen,&panel,&label,&layout);
}
void RollbackView_Render(uint32_t visible,uint32_t failed,uint32_t restored,uint32_t reason,uint32_t log_id)
{
 if(!panel)return;
 lv_obj_set_flag(panel,LV_OBJ_FLAG_HIDDEN,!visible);if(!visible)return;
 if(!message[0]||previous[0]!=failed||previous[1]!=restored||previous[2]!=reason||previous[3]!=log_id){
  previous[0]=failed;previous[1]=restored;previous[2]=reason;previous[3]=log_id;
  snprintf(message,sizeof(message),"Update rolled back\n%lu -> %lu\nReason %lu / Log %lu\nPrevious version restored.\nPress O; allow 30s to save",
    (unsigned long)failed,(unsigned long)restored,(unsigned long)reason,(unsigned long)log_id);
  lv_label_set_text_static(label,message);
 }
 lv_obj_move_foreground(panel);
}
