#include "App_Settings.h"
#include "NoodoeBluetooth.h"
#include "AmbientService.h"
#include "DashService.h"
#include "NoodoeRuntime.h"
#include "BSP_Power.h"
#include "InputMode.h"
#include <stdio.h>

/* Read-only catalog: IDs never index values[] or enter the persistence codec.
 * Getters copy owner snapshots only; browsing never probes a bus, restarts a
 * radio, writes a setting or exposes addresses/bond secrets. */
static const SettingItem items[]={
 {.key=SD_BT,.name="Bluetooth state",.kind=SETTING_READONLY,.min=0,.max=0,.step=1},
 {.key=SD_BT_CHIP,.name="BT controller / patch",.kind=SETTING_READONLY,.min=0,.max=0,.step=1},
 {.key=SD_BT_IO,.name="SPP traffic",.kind=SETTING_READONLY,.min=0,.max=0,.step=1},
 {.key=SD_BT_QUEUE,.name="SPP queue / errors",.kind=SETTING_READONLY,.min=0,.max=0,.step=1},
 {.key=SD_AMBIENT,.name="Ambient light",.kind=SETTING_READONLY,.min=0,.max=0,.step=1},
 {.key=SD_AMBIENT_RAW,.name="Ambient RAW / age",.kind=SETTING_READONLY,.min=0,.max=0,.step=1},
 {.key=SD_AMBIENT_ID,.name="Ambient chip / config",.kind=SETTING_READONLY,.min=0,.max=0,.step=1},
 {.key=SD_AMBIENT_ERROR,.name="Ambient I2C status",.kind=SETTING_READONLY,.min=0,.max=0,.step=1},
 {.key=SD_DASH,.name="Dashboard UART",.kind=SETTING_READONLY,.min=0,.max=0,.step=1},
 {.key=SD_DASH_IO,.name="Dashboard frames",.kind=SETTING_READONLY,.min=0,.max=0,.step=1},
 {.key=SD_DASH_LIGHT,.name="Dashboard light output",.kind=SETTING_READONLY,.min=0,.max=0,.step=1},
 {.key=SD_VEHICLE,.name="UART speed / fuel RAW",.kind=SETTING_READONLY,.min=0,.max=0,.step=1},
 {.key=SD_GPS,.name="Phone GPS",.kind=SETTING_READONLY,.min=0,.max=0,.step=1},
 {.key=SD_POWER,.name="Ignition input",.kind=SETTING_READONLY,.min=0,.max=0,.step=1},
 {.key=SD_INPUT_MODE,.name="PH9 input mode",.kind=SETTING_READONLY,.min=0,.max=0,.step=1}
};
const SettingItem *SettingsDebug_Items(uint32_t *n)
{if(n)*n=sizeof(items)/sizeof(items[0]);return items;}

/* Values retain their validity/error evidence. A disconnected or stale sensor
 * is never displayed as a successful zero-lux measurement. */
uint32_t SettingsDebug_Format(uint32_t key,char *out,uint32_t size)
{
 if(key<SD_BT||key>SD_INPUT_MODE||!out||!size)return 0;
 if(key==SD_INPUT_MODE){snprintf(out,size,"PH9 %s / %s / blocked %lu",g_input_mode.raw_high?"HIGH":"LOW",
  g_input_mode.allowed?"Allowed":"Dash mode",(unsigned long)g_input_mode.blocked_presses);return 1;}
 if(key==SD_VEHICLE){VehicleSnapshot v;
  if(!NoodoeRuntime_GetVehicle(&v)||!v.sequence)snprintf(out,size,"No telemetry");
  else snprintf(out,size,"%lu km/h / %02lX / %lu ms%s",(unsigned long)v.speed_kph,(unsigned long)v.status_raw,(unsigned long)v.age_ms,v.stale?" stale":"");
  return 1;}
 if(key==SD_GPS){GnssSnapshot g;
  if(!NoodoeRuntime_GetGnss(&g)||!g.fix.has_sample)snprintf(out,size,"No phone fix");
  else snprintf(out,size,"%s / flags %lX / %lu ms",g.stale?"Stale":g.valid?"Live":"No fix",(unsigned long)g.fix.fields,(unsigned long)g.age_ms);
  return 1;}
 if(key==SD_POWER){snprintf(out,size,"RAW %lu / %s / edges %lu",(unsigned long)g_bsp_power.raw_ign_off,
  !g_bsp_power.ign_valid?"Unknown":g_bsp_power.ign_on?"ON":"OFF",(unsigned long)g_bsp_power.changes);return 1;}
 if(key<=SD_BT_QUEUE){
  Bluetooth_Diagnostics b;Bluetooth_GetDiagnostics(&b);
  const Bluetooth_LinkState *l=&b.links[0];
  switch(key){
  case SD_BT: {
   static const char *const names[]={"Off","Starting","Ready","Fault","Stopping"};
   snprintf(out,size,"%s / link %u / err %lX",b.state<5?names[b.state]:"Unknown",l->status,(unsigned long)b.last_error);break;}
  case SD_BT_CHIP:snprintf(out,size,"rev %lX / %lu B / %lu baud",(unsigned long)b.lmp_subversion,(unsigned long)b.patch_bytes,(unsigned long)b.baud);break;
  case SD_BT_IO:snprintf(out,size,"RX %lu / TX %lu B",(unsigned long)l->rx_bytes,(unsigned long)l->tx_bytes);break;
  default:snprintf(out,size,"RX %lu TX %lu / HCI %lu",(unsigned long)l->rx_queued,(unsigned long)l->tx_queued,(unsigned long)b.hci_errors);break;
  }
 }else if(key<=SD_AMBIENT_ERROR){
  Ambient_Snapshot a;AmbientService_GetSnapshot(&a);const BSP_Ambient_Diagnostics *d=&a.driver;
  switch(key){
  case SD_AMBIENT:if(!d->valid)snprintf(out,size,"Unavailable / error %lu",(unsigned long)d->error);
   else snprintf(out,size,"%lu.%03lu lux / %s",(unsigned long)(d->millilux/1000),(unsigned long)(d->millilux%1000),a.stale?"STALE":"Live");
   break;
  case SD_AMBIENT_RAW:if(!d->samples)snprintf(out,size,"No sample / state %lu",(unsigned long)a.state);
   else snprintf(out,size,"0x%04lX / %lu ms%s",(unsigned long)d->raw,(unsigned long)a.sample_age_ms,a.stale?" stale":"");
   break;
  case SD_AMBIENT_ID:snprintf(out,size,"ID %04lX:%04lX / cfg %04lX",(unsigned long)d->manufacturer,(unsigned long)d->device,(unsigned long)d->configuration);break;
  default:snprintf(out,size,"err %lu / HAL %lX / phase %lu",(unsigned long)d->error,(unsigned long)d->hal_error,(unsigned long)d->phase);break;
  }
 }else{
  Dash_Snapshot d;DashService_GetSnapshot(&d);
  switch(key){
  case SD_DASH:snprintf(out,size,"%s / phase %lu / err %lu",d.link_up?"Linked":"Offline",(unsigned long)d.phase,(unsigned long)d.error);break;
  case SD_DASH_IO:snprintf(out,size,"RX %lu TX %lu / bad %lu",(unsigned long)d.rx_frames,(unsigned long)d.tx_frames,(unsigned long)(d.uart_errors+d.checksum_errors));break;
  default:snprintf(out,size,"raw %lu / offset %+ld / TX %lu / src %lu",(unsigned long)d.raw_light_index,(long)d.light_bias,(unsigned long)d.light_index,(unsigned long)d.light_source);break;
  }
 }
 return 1;
}
