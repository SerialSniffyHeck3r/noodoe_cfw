#include "Product_Overlay.h"
#include "Install_View.h"
#include "Product_Fonts.h"
#include "MaintenanceCopy.h"
#include "App_Recovery.h"
#include <stdio.h>
#include "stm32f4xx_hal.h"
static lv_obj_t *panel,*label;
static char text[320];
static uint32_t updated,shown;
/* One bounded overlay is reused for transfer and the post-reset health lease.
 * No storage access, command dispatch or heap allocation while rendering. */
uint32_t InstallView_Create(lv_obj_t *screen)
{
 static const ProductOverlayLayout layout={0,0,480,480,48,110,384,290,0,255,0x101820};
 return ProductOverlay_Create(screen,&panel,&label,&layout);
}
void InstallView_Render(uint32_t visible,const InstallSession *s,uint32_t now)
{
 if(!panel)return;
 lv_obj_set_flag(panel,LV_OBJ_FLAG_HIDDEN,!visible);
 if(!visible){shown=0;return;}
 if(shown&&now-updated<200U){lv_obj_move_foreground(panel);return;}
 shown=1;updated=now;
 if(!s){
  static const char *const states[]={"Starting Bluetooth.","Connecting to phone.","ACTION NEEDED\nConfirm on your phone.","Checking new version.","Saving boot result.","Check failed.\nSee phone logs."};
  snprintf(text,sizeof(text),"CFW UPDATE\n\n%s",states[AppRecovery_TrialPhase()-1U]);
  lv_label_set_text_static(label,text);lv_obj_move_foreground(panel);return;
 }
 InstallSession copy;uint32_t mask=__get_PRIMASK();__disable_irq();copy=*s;__set_PRIMASK(mask);s=&copy;
 uint32_t w[20]={0};InstallSession_Snapshot(s,w);
 const char *phase=s->state==INSTALL_DISCONNECTED?"Reconnect to check the result.":
  s->state==INSTALL_CANCELLING?COPY_WRITE_WAIT:s->state==INSTALL_UNKNOWN?COPY_UNKNOWN:
  s->state==INSTALL_ERROR?"Well, shit. Check phone logs.":s->state>=INSTALL_COMMIT&&s->state<=INSTALL_HEALTH?"Restarting. Hang tight.":COPY_WORKING;
 snprintf(text,sizeof(text),"CFW UPDATE\n%s\n\nStage %lu / 8\n%lu / %lu bytes\nSector %lu / %lu\n%s",phase,
  (unsigned long)w[5],(unsigned long)w[10],(unsigned long)w[11],(unsigned long)w[13],(unsigned long)w[14],
  w[17]?"Hold O 3 sec: cancel":COPY_POWER);
 lv_label_set_text_static(label,text);lv_obj_move_foreground(panel);
}
