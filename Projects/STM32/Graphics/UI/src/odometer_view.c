#include "Product_Overlay.h"
#include "Odometer_View.h"
#include "Product_Fonts.h"
#include "BSP_Buttons.h"
#include <stdio.h>
static lv_obj_t *panel,*label;
static OdometerStatus status;
static uint32_t showing,choice,dismissed,armed;
static char text[192];
/* Allocate once; this bounded modal shares the current typography and never
 * touches UART/storage. Motion/PH9 are enforced by the app before input. */
uint32_t OdometerView_Create(lv_obj_t *screen)
{
 static const ProductOverlayLayout layout={48,145,384,225,10,12,364,203,16,245,0x102030};
 return ProductOverlay_Create(screen,&panel,&label,&layout);
}
uint32_t OdometerView_Visible(void){return showing;}
void OdometerView_Render(const OdometerStatus *s,uint32_t visible,uint32_t now)
{
 (void)now;if(!panel||!s)return;
 uint32_t open=visible&&s->pending&&dismissed!=s->revision;
 if(open&&!showing){choice=0;armed=0;}
 showing=open;status=*s;lv_obj_set_flag(panel,LV_OBJ_FLAG_HIDDEN,!open);if(!open)return;
 static const char *const choices[]={"Keep saved distance","Use dashboard value","Ask me later"};
 snprintf(text,sizeof(text),"ODO changed\nSaved %lu km\nDashboard %lu km\n\n%s",(unsigned long)s->display,(unsigned long)s->raw,
  s->ready?(s->reason==ODO_RANGE?"Invalid reading. Wait.":choices[choice]):"Stop to review this reading.");
 lv_label_set_text(label,text);lv_obj_move_foreground(panel);
}
/* Require a fresh press after opening; no held key can accept the dialog.
 * Returning true consumes every part of the gesture, including BSP SHORT. */
uint32_t OdometerView_Button(uint32_t button,uint32_t event)
{
 if(!showing)return 0;
 if(event==BSP_BUTTON_EVENT_PRESS)armed|=1U<<button;
 if(event==BSP_BUTTON_EVENT_SHORT_PRESS&&(armed&(1U<<button))){
  armed&=~(1U<<button);if(!status.ready)return 1;
  if(button==BSP_BUTTON_UP)choice=(choice+2)%3;
  else if(button==BSP_BUTTON_DOWN)choice=(choice+1)%3;
  else if(button==BSP_BUTTON_ENTER){
   if(!OdometerGuard_RequestDecision(status.revision,choice+1)&&choice==2)dismissed=status.revision;
  }
 }
 return 1;
}
