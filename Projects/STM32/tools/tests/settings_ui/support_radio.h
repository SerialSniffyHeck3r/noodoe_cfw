#include "NoodoeBluetooth.h"
#include "SettingsService.h"
#include "AmbientService.h"
#include "DashService.h"
#include "NoodoeRuntime.h"
#include "BSP_Power.h"
volatile BSP_Power_Diagnostics g_bsp_power;
uint32_t NoodoeRuntime_GetVehicle(VehicleSnapshot *out){(void)out;return 0;}
uint32_t NoodoeRuntime_GetGnss(GnssSnapshot *out){(void)out;return 0;}
volatile Bluetooth_Diagnostics g_bluetooth;
void Bluetooth_GetDiagnostics(Bluetooth_Diagnostics *out){*out=g_bluetooth;}
volatile Settings_Diagnostics g_settings;
static Bluetooth_ControlMailbox control;
static uint32_t pairing,bonds,stops,clears,starts,install_hold,radio_error,mode;
static Ambient_Snapshot ambient;
static Dash_Snapshot dash;
uint32_t PowerService_RunRequired(void){return install_hold;}
uint32_t PowerService_Mode(void){return mode;}
uint32_t Bluetooth_PairingSeconds(void){return pairing;}
uint32_t Bluetooth_BondCount(void){return bonds;}
int Bluetooth_SetPairingWindow(uint32_t seconds){if(radio_error)return -3;pairing=seconds;return 0;}
int Bluetooth_Disconnect(Bluetooth_Role role){(void)role;if(radio_error)return -3;g_bluetooth.links[0].status=0;return 0;}
int Bluetooth_Stop(void){++stops;g_bluetooth.state=BLUETOOTH_STATE_STOPPING;pairing=0;return 0;}
int Bluetooth_ForgetAll(void){if(radio_error)return -2;++clears;bonds=0;++g_bluetooth.key_generation;return 0;}
void Bluetooth_GetControl(Bluetooth_ControlMailbox *out){*out=control;}
int Bluetooth_RequestStart(uint32_t seq){++starts;control.request_sequence=seq;return 0;}
void AmbientService_GetSnapshot(Ambient_Snapshot *out){*out=ambient;}
void DashService_GetSnapshot(Dash_Snapshot *out){*out=dash;}
Ambient_Status AmbientService_RequestProbe(uint32_t hz,uint32_t *id){(void)hz;*id=19;return AMBIENT_ACCEPTED;}
