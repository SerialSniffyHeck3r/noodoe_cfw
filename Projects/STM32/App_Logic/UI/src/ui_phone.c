#include "ui_state_internal.h"
/* Notification browsing starts with the newest entry. Reply selection never changes recipients:
 * the Product adapter captures notification ID/revision when opening it. */
uint32_t Ui_PhoneNavigate(UiState *s,uint32_t b,uint32_t hold)
{
 if(s->dashboard.card!=UI_NOTIFICATIONS)return 0;
 if(!s->speed_valid||s->speed_kph>50U){
  s->notification_reply=0;s->notification_id=0;s->dashboard.selection=0;
  return b!=UI_ENTER||hold;
 }
 if(s->notification_reply){
  if(s->reply_sending)return 1;
  if(b==UI_ENTER){
   if(hold||s->reply_selection==s->reply_count){s->notification_reply=0;Ui_ContextChanged(s);}
   else (void)Ui_Emit(s,UI_FX_PHONE_REPLY,1,s->reply_selection);
  }else{
   uint32_t count=s->reply_count+1;
   s->reply_selection=b==UI_DOWN?(s->reply_selection+1)%count:(s->reply_selection+count-1)%count;
  }return 1;
 }
 if(b==UI_ENTER){
  if(hold){if(s->notification_count)(void)Ui_Emit(s,UI_FX_PHONE_REPLY,0,s->dashboard.selection);return 1;}
  s->notification_open=0;s->notification_id=0;return 0;
 }
 if(hold)return 0; /* Preserve distance-footer hold navigation. */
 s->notification_id=0;
 s->notification_open=1;
 if(s->notification_count){uint32_t n=s->notification_count;
  s->dashboard.selection=b==UI_DOWN?(s->dashboard.selection+1)%n:(s->dashboard.selection+n-1)%n;}
 return 1;
}
