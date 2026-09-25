#include "App_Settings.h"
#include "NoodoeBluetooth.h"
#include "PowerService.h"
#include "SettingsService.h"
#include <stdio.h>
static const SettingItem items[]={
 {.key=SK_NOTIFICATION_PREVIEW,.name="Notification preview",.kind=SETTING_CHOICE,.min=0,.max=1,.step=1},
 {.key=SK_NOTIFICATION_SECONDS,.name="Preview seconds",.kind=SETTING_NUMBER,.min=3,.max=30,.step=1},
 {.key=SK_PHONE1,.name="Phone",.kind=SETTING_READONLY,.min=0,.max=0,.step=1},
 {.key=SK_BT_STATUS,.name="Bluetooth",.kind=SETTING_READONLY,.min=0,.max=0,.step=1},
 {.key=SK_PAIR,.name="Pair phone",.kind=SETTING_ACTION,.min=1,.max=1,.step=1},
 {.key=SK_BT_CLOSE,.name="Close pairing",.kind=SETTING_ACTION,.min=1,.max=1,.step=1},
 {.key=SK_BT_DISCONNECT,.name="Disconnect phone",.kind=SETTING_ACTION,.min=1,.max=1,.step=1},
 {.key=SK_BT_REPAIR,.name="Re-pair phone",.kind=SETTING_CONFIRM,.min=0,.max=1,.step=1},
 {.key=SK_BT_RESTART,.name="Restart Bluetooth",.kind=SETTING_ACTION,.min=1,.max=1,.step=1},
};
const SettingItem *SettingsConnections_Items(uint32_t *n)
{if(n)*n=sizeof(items)/sizeof(items[0]);return items;}

/* UI-owned operation, never a saved setting. One AppSettings request owns
 * Stop -> erase RAM keys -> durable erase -> start -> pair. The BT task owns
 * HCI; StorageTask alone writes CFG. Failed writes leave the radio stopped. */
static struct {uint32_t phase,started,generation,sequence;} operation;
enum {BEGIN,STOPPED,SAVED,STARTED,WINDOW,CLOSED,DISCONNECTED};
uint32_t SettingsConnections_Apply(uint32_t key,int32_t value,uint32_t now)
{
    (void)value;
    uint32_t result=APP_SETTINGS_BUSY,state=g_bluetooth.state;
    if(PowerService_RunRequired()||PowerService_Mode()!=POWER_RUN){result=APP_SETTINGS_UNAVAILABLE;goto done;}
    if(operation.phase&&now-operation.started>=35000U){result=APP_SETTINGS_DEVICE_ERROR;goto done;}
    switch(operation.phase){
    case BEGIN:
        operation.started=now;
        if(key==SK_BT_CLOSE){
            if(Bluetooth_SetPairingWindow(0))goto failed;
            operation.phase=CLOSED;
        }else if(key==SK_BT_DISCONNECT){
            if(Bluetooth_Disconnect(BLUETOOTH_PHONE))goto failed;
            operation.phase=DISCONNECTED;
        }else if(key==SK_BT_REPAIR||key==SK_BT_RESTART){
            if(key==SK_BT_REPAIR&&!g_settings.provisioned){result=APP_SETTINGS_UNAVAILABLE;goto done;}
            if(state!=BLUETOOTH_STATE_OFF&&state!=BLUETOOTH_STATE_FAULT&&Bluetooth_Stop())goto failed;
            operation.phase=STOPPED;
        }else operation.phase=STARTED;
        break;
    case STOPPED:
        if(state!=BLUETOOTH_STATE_OFF&&state!=BLUETOOTH_STATE_FAULT)break;
        if(key==SK_BT_REPAIR){
            if(Bluetooth_ForgetAll())break; /* Actual HCI OFF, including DMA drain. */
            operation.generation=g_bluetooth.key_generation;operation.phase=SAVED;
        }else operation.phase=STARTED;
        break;
    case SAVED:
        if(g_bluetooth.key_persisted_generation!=operation.generation)break;
        operation.phase=STARTED;
        break;
    case STARTED:
        if(state==BLUETOOTH_STATE_OFF||state==BLUETOOTH_STATE_FAULT){
            Bluetooth_ControlMailbox c;Bluetooth_GetControl(&c);
            /* A matching failed attempt is terminal; no automatic retry. */
            if(operation.sequence){
                if(c.ack_sequence==operation.sequence&&(c.accept_result||c.phase==4))goto failed;
                break;
            }
            operation.sequence=c.request_sequence+1U;if(!operation.sequence)++operation.sequence;
            if(Bluetooth_RequestStart(operation.sequence))goto failed;
        }else if(state==BLUETOOTH_STATE_READY){
            if(key==SK_BT_RESTART){result=APP_SETTINGS_OK;goto done;}
            if(Bluetooth_SetPairingWindow(120))goto failed;
            operation.phase=WINDOW;
        }
        break;
    case WINDOW:
        if(state==BLUETOOTH_STATE_FAULT)goto failed;
        if(Bluetooth_PairingSeconds()){result=APP_SETTINGS_OK;goto done;}
        break;
    case CLOSED:
        if(!Bluetooth_PairingSeconds()){result=APP_SETTINGS_OK;goto done;}
        break;
    case DISCONNECTED:
        if(g_bluetooth.links[0].status==BLUETOOTH_LINK_OFF){result=APP_SETTINGS_OK;goto done;}
        break;
    }
    return result;
failed:result=APP_SETTINGS_DEVICE_ERROR;
done:operation.phase=operation.sequence=0;return result;
}

/* Cached live radio state, never DATA_DEBUG phone placeholders or bond keys.
 * READY means initialized; only LINK_UP means a connected phone. */
uint32_t SettingsConnections_Format(uint32_t key,char *out,uint32_t size)
{
    const char *text=0;
    if(key==SK_BT_STATUS){
        if(g_bluetooth.state==BLUETOOTH_STATE_FAULT){snprintf(out,size,"Radio error 0x%lX",(unsigned long)g_bluetooth.last_error);return 1;}
        text=g_bluetooth.state==BLUETOOTH_STATE_READY?"Ready":g_bluetooth.state==BLUETOOTH_STATE_STARTING?"Starting...":
             g_bluetooth.state==BLUETOOTH_STATE_STOPPING?"Stopping...":"Off";
    }else if(key==SK_PHONE1){
        if(g_bluetooth.links[0].status==BLUETOOTH_LINK_UP)text="Connected";
        else {snprintf(out,size,"Disconnected / %lu saved",(unsigned long)Bluetooth_BondCount());return 1;}
    }else if(key==SK_PAIR){
        uint32_t seconds=Bluetooth_PairingSeconds();
        if(seconds){snprintf(out,size,"Visible for %lu s",(unsigned long)seconds);return 1;}
        text="Open for 2 minutes";
    }else if(key==SK_BT_REPAIR)text="Clear saved phone pairings";
    else if(key==SK_BT_CLOSE)text="Keep existing connection";
    else if(key==SK_BT_DISCONNECT)text="Keep saved pairing";
    else if(key==SK_BT_RESTART)text="Disconnect and restart radio";
    if(!text)return 0;
    snprintf(out,size,"%s",text);return 1;
}
