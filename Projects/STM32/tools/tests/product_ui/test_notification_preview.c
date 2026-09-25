#include "Notification_Preview.h"
#define CHECK(x) do{if(!(x))return __LINE__;}while(0)
unsigned test_notification_preview(void)
{
 NotificationPreview p={0};UiState s={0};PhoneContentSlot phone={.connected=1,.token=1,.status_revision=1};
 s.power=IGN_ON;s.ign_on=1;s.dashboard.card=UI_TRIP;s.dashboard.selection=2;
 NotificationPreview_Step(&p,&s,&phone,1,5,1,100);
 phone.status.notifications_valid=1;phone.status.count=1;phone.status.notifications[0].id=10;phone.status.notifications[0].revision=1;phone.status_revision++;
 NotificationPreview_Step(&p,&s,&phone,1,5,1,200);CHECK(p.active&&s.dashboard.card==UI_NOTIFICATIONS&&s.notification_id==10);
 phone.status_revision++;NotificationPreview_Step(&p,&s,&phone,1,5,1,4000);CHECK(p.started==200);
 NotificationPreview_Step(&p,&s,&phone,1,5,1,5200);CHECK(!p.active&&s.dashboard.card==UI_TRIP&&s.dashboard.selection==2);
 phone.status.notifications[0].revision++;phone.status_revision++;NotificationPreview_Step(&p,&s,&phone,0,5,1,5300);CHECK(!p.active);
 NotificationPreview_Step(&p,&s,&phone,1,5,1,5301);CHECK(!p.active);
 phone.status.notifications[0].revision++;phone.status_revision++;NotificationPreview_Step(&p,&s,&phone,1,5,1,5400);CHECK(p.active);
 NotificationPreview_Input(&p);NotificationPreview_Step(&p,&s,&phone,1,5,1,20000);CHECK(!p.active&&s.dashboard.card==UI_NOTIFICATIONS);
 s.dashboard.card=UI_MUSIC;phone.status.notifications[0].revision++;phone.status_revision++;NotificationPreview_Step(&p,&s,&phone,1,5,1,21000);CHECK(p.active);
 s.calls.return_valid=1;s.calls.return_card=UI_NOTIFICATIONS;s.calls.phone.state=CALL_RINGING;s.dashboard.card=UI_CALLS;
 NotificationPreview_Step(&p,&s,&phone,1,5,1,21100);CHECK(!p.active&&s.dashboard.card==UI_CALLS&&s.calls.return_card==UI_MUSIC);
 s.calls.phone.state=0;s.dashboard.card=UI_PHONE_GPS;phone.token=2;phone.status_revision=1;
 NotificationPreview_Step(&p,&s,&phone,1,5,1,21200);CHECK(!p.active&&s.dashboard.card==UI_PHONE_GPS);
 phone.status.notifications[0].revision++;phone.status_revision++;NotificationPreview_Step(&p,&s,&phone,1,5,0,21300);CHECK(!p.active);
 phone.status.notifications[0].revision++;phone.status_revision++;NotificationPreview_Step(&p,&s,&phone,1,5,1,0xfffffff0);CHECK(p.active);
 s.dashboard.footer=UI_RESV;s.reserve_active=1;NotificationPreview_Step(&p,&s,&phone,1,5,1,4990);
 CHECK(!p.active&&s.dashboard.card==UI_PHONE_GPS&&s.dashboard.footer==UI_RESV&&s.reserve_active);
 phone.status.notifications[0].revision++;phone.status_revision++;NotificationPreview_Step(&p,&s,&phone,1,5,1,5000);CHECK(p.active);
 s.power=IGN_STOPPING;s.ign_on=0;NotificationPreview_Step(&p,&s,&phone,1,5,1,5001);CHECK(!p.active&&s.dashboard.card==UI_PHONE_GPS);
 return 0;
}
