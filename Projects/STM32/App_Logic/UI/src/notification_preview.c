#include "Notification_Preview.h"
#include "ui_state_internal.h"
void NotificationPreview_Input(NotificationPreview *p){p->active=0;}
/* Restore navigation only: never roll back fuel locks, trip values or the
 * footer selected by a newer safety event. Calls inherit the original return. */
static void Restore(NotificationPreview *p,UiState *s)
{
 if(!p->active)return;
 if(s->calls.return_valid&&s->calls.return_card==UI_NOTIFICATIONS){
  s->calls.return_card=p->card;s->calls.return_selection=p->selection;
 }
 if(s->dashboard.card==UI_NOTIFICATIONS){
  s->dashboard.card=p->card;s->dashboard.selection=p->selection;
  s->notification_id=s->notification_reply=0;Ui_ContextChanged(s);
 }
 p->active=0;
}
void NotificationPreview_Step(NotificationPreview *p,UiState *s,
 const PhoneContentSlot *phone,uint32_t enabled,uint32_t seconds,uint32_t allowed,uint32_t now)
{
 uint32_t fresh=0;const PhoneStatus *v=&phone->status;
 if(p->token!=phone->token){Restore(p,s);p->token=phone->token;p->revision=0;p->count=0;}
 if(phone->status_revision!=p->revision){
  /* The first nonempty snapshot on a new connection is existing history.
   * A fresh empty/header snapshot arms subsequent live arrivals. */
  if(p->revision&&v->notifications_valid&&v->count){
   fresh=1;
   for(uint32_t i=0;i<p->count;i++)if(p->seen[i][0]==v->notifications[0].id&&p->seen[i][1]==v->notifications[0].revision)fresh=0;
  }
  p->revision=phone->status_revision;p->count=v->count;
  for(uint32_t i=0;i<v->count;i++){p->seen[i][0]=v->notifications[i].id;p->seen[i][1]=v->notifications[i].revision;}
 }
 allowed=allowed&&enabled&&phone->connected&&s->power==IGN_ON&&s->ign_on&&
  !s->menu&&!s->modal&&!s->warning&&!s->calls.phone.state&&!s->notification_reply&&!s->reply_sending;
 if(!allowed){Restore(p,s);return;}
 if(p->active&&s->dashboard.card!=UI_NOTIFICATIONS)p->active=0;
 if(fresh&&!s->pressed_buttons&&(p->active||s->dashboard.card!=UI_NOTIFICATIONS)){
  if(!p->active){p->card=s->dashboard.card;p->selection=s->dashboard.selection;}
  p->active=1;p->started=now;
  s->dashboard.card=UI_NOTIFICATIONS;s->dashboard.selection=0;
  s->notification_id=v->notifications[0].id;s->notification_open=1;Ui_ContextChanged(s);
 }
 if(seconds<3)seconds=3;
 if(seconds>30)seconds=30;
 if(p->active&&(uint32_t)(now-p->started)>=seconds*1000U)Restore(p,s);
}
