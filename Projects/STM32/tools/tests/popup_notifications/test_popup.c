#include "Popup_Notifications.h"
#include <string.h>
#include <stddef.h>
static uint32_t assertions;
#define CHECK(x) do {++assertions;if(!(x))return __LINE__;} while(0)
uint32_t get_assertions(void){return assertions;}
uint32_t test_hint(void)
{
    PopupNotificationSnapshot s;
    PopupNotifications_Init();CHECK(!PopupNotifications_Get(NULL));
    CHECK(sizeof(g_popup_notifications)==184&&offsetof(PopupNotificationDiagnostics,request_text)==56);
    PopupNotifications_Process(100,0);CHECK(PopupNotifications_Get(&s)&&!s.visible);
    PopupNotifications_Process(110,1);CHECK(PopupNotifications_Get(&s)&&s.visible);
    CHECK(!strcmp(s.text,"Hold O to reset")&&s.duration_ms==3000&&s.revision==1);
    for(uint32_t elapsed=0;elapsed<3000;++elapsed){
        PopupNotifications_Process(110+elapsed,1);CHECK(PopupNotifications_Get(&s));
        CHECK(s.visible&&s.elapsed_ms==elapsed&&s.revision==1);
    }
    PopupNotifications_Process(3110,1);CHECK(PopupNotifications_Get(&s)&&!s.visible&&s.elapsed_ms==3000);
    PopupNotifications_Process(5000,1);CHECK(PopupNotifications_Get(&s)&&!s.visible&&s.revision==1);
    PopupNotifications_Process(5010,0);PopupNotifications_Process(5020,1);
    CHECK(PopupNotifications_Get(&s)&&s.visible&&s.revision==2);
    PopupNotifications_Process(5030,0);CHECK(PopupNotifications_Get(&s)&&!s.visible);
    return 0;
}
uint32_t test_request_ownership(void)
{
    PopupNotificationSnapshot s;char message[]="Message A";
    PopupNotifications_Init();CHECK(PopupNotifications_Request(message,3));
    message[0]='X';CHECK(!PopupNotifications_Request("busy",1));
    PopupNotifications_Process(100,1);CHECK(PopupNotifications_Get(&s));
    CHECK(s.visible&&!strcmp(s.text,"Message A")&&g_popup_notifications.source==2);
    PopupNotifications_Process(200,0);CHECK(PopupNotifications_Get(&s)&&s.visible);
    CHECK(PopupNotifications_Request("Replacement",1));PopupNotifications_Process(500,0);
    CHECK(PopupNotifications_Get(&s)&&s.visible&&!strcmp(s.text,"Replacement")&&s.elapsed_ms==0&&s.revision==2);
    PopupNotifications_Process(1499,0);CHECK(PopupNotifications_Get(&s)&&s.visible);
    PopupNotifications_Process(1500,0);CHECK(PopupNotifications_Get(&s)&&!s.visible);
    CHECK(PopupNotifications_Request("Hide me",60));PopupNotifications_Process(1600,0);
    CHECK(PopupNotifications_Request(NULL,0));PopupNotifications_Process(1700,0);
    CHECK(PopupNotifications_Get(&s)&&!s.visible);
    /* An explicit cancellation wins over a coincident automatic entry hint. */
    CHECK(PopupNotifications_Request(NULL,0));PopupNotifications_Process(1800,1);
    CHECK(PopupNotifications_Get(&s)&&!s.visible);
    return 0;
}
uint32_t test_validation_wrap(void)
{
    PopupNotificationSnapshot s;char text[65];
    PopupNotifications_Init();CHECK(!PopupNotifications_Request("",1));
    CHECK(!PopupNotifications_Request("bad\ntext",3));CHECK(!PopupNotifications_Request("text",0));
    CHECK(!PopupNotifications_Request("text",61));CHECK(!PopupNotifications_Request("text",UINT32_MAX));
    memset(text,'A',sizeof(text));text[64]=0;CHECK(!PopupNotifications_Request(text,3));
    text[63]=0;CHECK(PopupNotifications_Request(text,3));
    PopupNotifications_Process(0xFFFFFF00U,0);CHECK(PopupNotifications_Get(&s)&&strlen(s.text)==63);
    PopupNotifications_Process(2743U,0);CHECK(PopupNotifications_Get(&s)&&s.visible&&s.elapsed_ms==2999);
    PopupNotifications_Process(2744U,0);CHECK(PopupNotifications_Get(&s)&&!s.visible);
    /* Development RAM requests are independently validated by their consumer. */
    g_popup_notifications.command=1;g_popup_notifications.seconds=0;
    ++g_popup_notifications.request_id;PopupNotifications_Process(3000,0);
    CHECK(g_popup_notifications.ack_id==g_popup_notifications.request_id&&g_popup_notifications.result==1);
    g_popup_notifications.seconds=3;memset((void*)g_popup_notifications.request_text,'A',64);
    ++g_popup_notifications.request_id;PopupNotifications_Process(3001,0);
    CHECK(g_popup_notifications.result==1&&!g_popup_notifications.visible);
    g_popup_notifications.seq|=1U;CHECK(!PopupNotifications_Get(&s));
    return 0;
}
