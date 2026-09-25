#include "Settings_QuickPresenter.h"
#include <stdio.h>
#include <string.h>
void SettingsQuick_Present(DashboardPage *p,const SettingsUI *s)
{
    if(!p||!s||p->kind!=UI_SYSTEM)return;
    uint32_t automatic=AppSettings_Value(SK_MODE);
    p->grey=0;p->known=1;p->ratio_permille=g_app_settings->effective_brightness*10U;
    strcpy(p->title,"Quick Settings");
    strcpy(p->lines[0],automatic?"Auto adjustment":"");
    AppSettings_Format(automatic?SK_BIAS:SK_BRIGHTNESS,p->numbers[0],sizeof(p->numbers[0]));
    if(!automatic)snprintf(p->numbers[0],sizeof(p->numbers[0]),"%ld",(long)AppSettings_Value(SK_BRIGHTNESS));
    strcpy(p->lines[1],automatic?"":"%");
    if(automatic)SettingsDisplay_SensorText(p->lines[2],sizeof(p->lines[2]));
    else strcpy(p->lines[2],"UP / DOWN to adjust");
    snprintf(p->note,sizeof(p->note),"%s",s->message[0]?s->message:s->motion.ready?"Hold O for Settings":"Stop for 5s, then hold O");
}
