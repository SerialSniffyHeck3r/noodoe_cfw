#include "Graphics_BringupHUD.h"
volatile uint32_t g_graphics_bringup_page;
uint32_t GraphicsBringupHUD_SelectPage(uint32_t page)
{ if(page>3U)return 0U;g_graphics_bringup_page=page;return 1U; }
#if defined(NOODOE_INTEGRATED) && NOODOE_INTEGRATED
#include "Graphics.h"
#include "NoodoeRuntime.h"
#include "RuntimeUpdate.h"
#include "NoodoeBluetooth.h"
#include "BSP_Dash.h"
#include "DashService.h"
#include "BSP_NOR.h"
#include "AmbientService.h"
#include "BSP_Clock.h"
#include "BSP_RAM.h"
#include "BSP_Power.h"
#include "BSP_Buttons.h"
#include "StorageService.h"
#include "StorageBackup.h"
#include <stdio.h>
#include <string.h>

#define ROWS 6U
static lv_obj_t *root,*title,*rows[ROWS],*uart_line;
static uint32_t last_update,last_page;
static char previous[ROWS][48];
static uint32_t previous_color[ROWS];
static char previous_uart[48];
static uint32_t previous_uart_color;
enum { COLOR_PASS=0x63E89AU,COLOR_FAIL=0xFF7980U,COLOR_WAIT=0xFFD166U,COLOR_INFO=0xACC9D4U };

/* Deletion of the enclosing arc also invalidates the result view; no global
 * pointer may survive a transition back to the full graphics test profile. */
static void Deleted(lv_event_t *event)
{if(lv_event_get_target(event)==root){root=title=uart_line=NULL;memset(rows,0,sizeof(rows));}}

static lv_obj_t *MakeLabel(int32_t y,int32_t height)
{
    lv_area_t area={100,y,379,y+height-1};
    if(!Graphics_IsAreaVisible(&area))return NULL;
    lv_obj_t *label=lv_label_create(root);if(!label)return NULL;
    lv_obj_remove_style_all(label);lv_obj_set_pos(label,100,y);lv_obj_set_size(label,280,height);
    lv_obj_set_style_text_font(label,&lv_font_montserrat_14,0);
    lv_obj_set_style_text_color(label,lv_color_hex(COLOR_INFO),0);
    lv_obj_set_style_text_opa(label,LV_OPA_COVER,0);
    lv_obj_set_style_text_align(label,LV_TEXT_ALIGN_CENTER,0);
    lv_label_set_long_mode(label,LV_LABEL_LONG_DOT);
    lv_obj_remove_flag(label,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    return label;
}

/* PASS means the named operation has actually completed; absence of an
 * external device stays WAIT. Mounted filesystem and full verified backup
 * are deliberately separate facts. Host parser tests are not radio tests. */
static uint32_t Describe(uint32_t index,char text[48])
{
    switch(index){
    case 0:
        snprintf(text,48,"LCD %s  BL %lu%%",Graphics_IsReady()?"PASS":"FAIL",(unsigned long)Graphics_GetBrightnessPercent());
        return Graphics_IsReady()?COLOR_PASS:COLOR_FAIL;
    case 1:{
        uint32_t state=g_bluetooth.state;
        if(state==BLUETOOTH_STATE_READY){
            unsigned count=0;Bluetooth_LinkState link;
            for(unsigned i=0;i<3U;++i)if(Bluetooth_GetLinkState((Bluetooth_Role)i,&link)==0 && link.status==BLUETOOTH_LINK_UP)++count;
            snprintf(text,48,"BT READY  %u/3 links",count);return COLOR_PASS;
        }
        snprintf(text,48,state==BLUETOOTH_STATE_FAULT?"BT FAIL  %04lX":"BT START  %04lX",(unsigned long)g_bluetooth.last_error);
        return state==BLUETOOTH_STATE_FAULT?COLOR_FAIL:COLOR_WAIT;
    }
    case 2:
        snprintf(text,48,"NOR %s  %06lX",g_bsp_nor.ready?"READ PASS":"FAIL",(unsigned long)g_bsp_nor.jedec_id);
        return g_bsp_nor.ready && !g_noodoe_runtime.storage_result?COLOR_PASS:COLOR_FAIL;
    case 3:
        snprintf(text,48,"NOR IO %s",g_storage_backup.transport_verified?"PASS":"FAIL");
        return g_storage_backup.transport_verified?COLOR_PASS:COLOR_FAIL;
    case 4:{
        /* Snapshot is a cached service copy: rendering never starts an I2C
         * transfer and an old valid sample cannot hide loss of fresh data. */
        Ambient_Snapshot ambient;AmbientService_GetSnapshot(&ambient);
        const BSP_Ambient_Diagnostics *sensor=&ambient.driver;
        if(sensor->valid && !ambient.stale){snprintf(text,48,"ALS %lu.%03lu lux",(unsigned long)(sensor->millilux/1000U),(unsigned long)(sensor->millilux%1000U));return COLOR_PASS;}
        if(ambient.state==AMBIENT_STATE_DISABLED){snprintf(text,48,"ALS DISABLED");return COLOR_INFO;}
        snprintf(text,48,"ALS %s %04lX @%luk",sensor->error?"FAIL":"WAIT",(unsigned long)sensor->error,(unsigned long)(ambient.requested_hz/1000U));
        return sensor->error?COLOR_FAIL:COLOR_WAIT;
    }
    case 5:
        snprintf(text,48,"RTC %s  %02lu:%02lu:%02lu",g_bsp_clock.time.valid?"READ PASS":"UNSET",(unsigned long)g_bsp_clock.time.hour,(unsigned long)g_bsp_clock.time.minute,(unsigned long)g_bsp_clock.time.second);
        return g_bsp_clock.time.valid?COLOR_PASS:COLOR_WAIT;
    case 6:
        snprintf(text,48,"RAM BASIC %s  %lu MiB",g_bsp_ram.ready?"PASS":(g_bsp_ram.result?"FAIL":"WAIT"),(unsigned long)(g_bsp_ram.capacity_bytes>>20));
        return g_bsp_ram.ready?COLOR_PASS:(g_bsp_ram.result?COLOR_FAIL:COLOR_WAIT);
    case 7:{
        VehicleSnapshot v;NoodoeRuntime_GetVehicle(&v);
        if(v.stale){snprintf(text,48,"UART DATA STALE");return COLOR_WAIT;}
        if(v.valid_fields&VEHICLE_VALID_FUEL_OBSERVED)
            snprintf(text,48,"ODO %lu SPD %lu F%lu",(unsigned long)v.odometer_km,(unsigned long)v.speed_kph,(unsigned long)v.fuel_observed);
        else snprintf(text,48,"ODO %lu SPD %lu F?",(unsigned long)v.odometer_km,(unsigned long)v.speed_kph);
        return COLOR_PASS;
    }
    case 8:{
        GnssSnapshot gps;NoodoeRuntime_GetGnss(&gps);
        snprintf(text,48,"GPS %s %s",gps.source==GNSS_SOURCE_EXTERNAL?"EXT":"PHONE",gps.valid?"FIX":(gps.stale?"STALE":"NO FIX"));return gps.valid?COLOR_PASS:COLOR_WAIT;
    }
    case 9:{
        Bluetooth_LinkState elm;Bluetooth_GetLinkState(BLUETOOTH_ELM,&elm);
        snprintf(text,48,"ELM %s",elm.status==BLUETOOTH_LINK_UP?"LINK UP":"DISCONNECTED");return elm.status==BLUETOOTH_LINK_UP?COLOR_PASS:COLOR_WAIT;
    }
    case 10:
        snprintf(text,48,"BUTTONS  held=%lX  boot=%lX",(unsigned long)g_graphics.input_pressed_mask,(unsigned long)g_graphics.input_boot_held_mask);
        return g_graphics.input_boot_held_mask?COLOR_WAIT:COLOR_INFO;
    case 11:
        snprintf(text,48,"IGN %s",!g_bsp_power.ign_valid?"WAIT":(g_bsp_power.ign_on?"ON":"OFF"));return COLOR_INFO;
    case 12:
        snprintf(text,48,"FS %s  result=%lu",g_storage_service.mounted?"MOUNTED":"UNMOUNTED",(unsigned long)g_storage_service.last_result);return g_storage_service.mounted?COLOR_PASS:COLOR_WAIT;
    case 13:
        snprintf(text,48,"NVM %s  seq=%lu",g_storage_service.nvm_valid?"VALID":"EMPTY",(unsigned long)g_storage_service.nvm_sequence);return g_storage_service.nvm_valid?COLOR_PASS:COLOR_WAIT;
    case 14:
        if(!g_runtime_update.ready){snprintf(text,48,"OTA WAIT  init=%lu",(unsigned long)g_runtime_update.result);return COLOR_WAIT;}
        if(g_runtime_update.state==UPDATE_RECEIVING || g_runtime_update.state==UPDATE_VERIFYING){
            uint32_t done=g_runtime_update.state==UPDATE_RECEIVING?g_runtime_update.received_bytes:g_runtime_update.verified_bytes;
            snprintf(text,48,"OTA %s %lu/448 KiB",g_runtime_update.state==UPDATE_RECEIVING?"RX":"VERIFY",(unsigned long)(done/1024U));return COLOR_INFO;
        }
        if(g_runtime_update.state==UPDATE_FAILED){snprintf(text,48,"OTA FAIL  %lu",(unsigned long)g_runtime_update.service_result);return COLOR_FAIL;}
        if(g_runtime_update.state>=UPDATE_VERIFIED){snprintf(text,48,"OTA %s",g_runtime_update.state==UPDATE_VERIFIED?"VERIFIED":(g_runtime_update.state==UPDATE_COMMITTED?"COMMITTED":"RESET WAIT"));return COLOR_INFO;}
        snprintf(text,48,"OTA READY / %s",g_runtime_update.authorized?"ARMED":"LOCKED");return COLOR_WAIT;
    case 15:
        snprintf(text,48,"TASKS  IO:%lu STORE:%lu",(unsigned long)g_noodoe_runtime.io_heartbeat,(unsigned long)g_noodoe_runtime.storage_heartbeat);return g_noodoe_runtime.start_error?COLOR_FAIL:COLOR_INFO;
    case 16:
        snprintf(text,48,"HEAP free %lu  min %lu",(unsigned long)g_noodoe_runtime.heap_free,(unsigned long)g_noodoe_runtime.heap_min);return g_noodoe_runtime.heap_min>=8192U?COLOR_PASS:COLOR_WAIT;
    default:
        snprintf(text,48,"MFi / iAP: EXCLUDED");return COLOR_INFO;
    }
}

uint32_t GraphicsBringupHUD_Init(lv_obj_t *parent)
{
    if(root)lv_obj_delete(root);
    root=lv_obj_create(parent);if(!root)return 0U;
    lv_obj_remove_style_all(root);lv_obj_set_pos(root,0,0);lv_obj_set_size(root,480,480);
    lv_obj_remove_flag(root,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(root,Deleted,LV_EVENT_DELETE,NULL);
    title=MakeLabel(213,19);if(!title)goto failed;
    for(uint32_t i=0;i<ROWS;++i){rows[i]=MakeLabel(241+(int32_t)i*20,18);if(!rows[i])goto failed;}
    /* Permanent UART status fits between the six result rows and FPS label.
     * Page rotation must not hide link state; counters mean valid RX and TC TX. */
    uart_line=MakeLabel(370,18);if(!uart_line)goto failed;
    previous_uart[0]='\0';previous_uart_color=0U;
    memset(previous,0,sizeof(previous));memset(previous_color,0,sizeof(previous_color));
    last_update=UINT32_MAX-500U;last_page=UINT32_MAX;
    return 1U;
failed:lv_obj_delete(root);return 0U;
}
void GraphicsBringupHUD_Process(uint32_t now_ms)
{
    if(!root || (uint32_t)(now_ms-last_update)<500U)return;
    last_update=now_ms;
    uint32_t request=g_graphics_bringup_page;
    uint32_t page=request>=1U && request<=3U?request-1U:(now_ms/6000U)%3U;
    if(page!=last_page){lv_label_set_text_fmt(title,"BRING-UP RESULTS  %lu/3",(unsigned long)(page+1U));last_page=page;}
    for(uint32_t i=0;i<ROWS;++i){
        char text[48];uint32_t color=Describe(page*ROWS+i,text);
        if(strcmp(previous[i],text)){lv_label_set_text(rows[i],text);strcpy(previous[i],text);}
        if(previous_color[i]!=color){lv_obj_set_style_text_color(rows[i],lv_color_hex(color),0);previous_color[i]=color;}
    }
    Dash_Snapshot uart;DashService_GetSnapshot(&uart);
    const char *name=uart.error?"FAIL":(!uart.enabled?"OFF":(uart.link_up?"UP":
                     (uart.phase==DASH_PHASE_GAP?"RETRY":"WAIT")));
    char text[48];snprintf(text,sizeof(text),"UART %s R%lu T%lu",name,(unsigned long)uart.link_frames,(unsigned long)uart.tx_frames);
    uint32_t color=uart.error?COLOR_FAIL:(uart.link_up?COLOR_PASS:COLOR_WAIT);
    if(strcmp(text,previous_uart)){lv_label_set_text(uart_line,text);strcpy(previous_uart,text);}
    if(color!=previous_uart_color){lv_obj_set_style_text_color(uart_line,lv_color_hex(color),0);previous_uart_color=color;}
}
#else
uint32_t GraphicsBringupHUD_Init(lv_obj_t *parent){(void)parent;return 1U;}
void GraphicsBringupHUD_Process(uint32_t now_ms){(void)now_ms;}
#endif
