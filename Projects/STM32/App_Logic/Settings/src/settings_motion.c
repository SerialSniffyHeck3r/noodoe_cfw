#include "Settings_Motion.h"
void SettingsMotion_Tick(SettingsMotion *s,uint32_t now,uint32_t valid,uint32_t speed,uint32_t inside)
{
    if(!s)return;
    valid=valid&&speed<=400U;
    if(valid&&speed<=3U){if(!s->entry_tracking){s->entry_tracking=1;s->entry_since=now;}}
    else s->entry_tracking=0;
    s->ready=s->entry_tracking&&now-s->entry_since>=SETTINGS_ENTRY_DWELL_MS;
    s->unknown=!valid;
    if(!inside){s->latched=s->recovery_tracking=s->expired=s->elapsed_ms=0;return;}
    if(!s->latched&&(!valid||speed>3U)){s->latched=1;s->started_ms=now;s->recovery_tracking=0;}
    if(!s->latched)return;
    if(valid&&speed<3U){if(!s->recovery_tracking){s->recovery_tracking=1;s->recovery_since=now;}}
    else s->recovery_tracking=0;
    if(s->recovery_tracking&&now-s->recovery_since>=SETTINGS_RECOVERY_DWELL_MS){
        s->latched=s->recovery_tracking=s->expired=s->elapsed_ms=0;return;
    }
    uint32_t elapsed=now-s->started_ms;
    s->elapsed_ms=elapsed>SETTINGS_MOTION_TIMEOUT_MS?SETTINGS_MOTION_TIMEOUT_MS:elapsed;
    s->expired=elapsed>=SETTINGS_MOTION_TIMEOUT_MS;
}
