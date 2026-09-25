#ifndef NOODOE_PRODUCT_UI_H
#define NOODOE_PRODUCT_UI_H
#include <stdint.h>
#include "Phone_Calls.h"
uint32_t ProductUI_PublishCalls(const PhoneCallsSnapshot *value);
uint32_t ProductUI_CallTarget(void);
#include "Ui_Dashboard.h"
#include "Phone_Content.h"
#include "Trip_Computer.h"
#include "Development_Data.h"
#include "Popup_Notifications.h"
/* One permanent speed shell with central child cards (including OBD). Future
 * power/storage/remote effects are observed, not secretly actuated. */
typedef struct {
    uint32_t magic,version,seq,ready,error,process_count,now_ms;
    uint32_t power,home,card,footer,menu,modal,warning,remote,epoch;
    uint32_t speed_valid,odo_valid,clock_valid,speed_kph,odo_km;
    uint32_t arc_value,arc_color,max_kph,units,hour,minute;
    uint32_t state_revision,effects_observed,effects_unsupported,effect_overflows;
    uint32_t scope_speed_only,input_count,ign_valid,ign_on,links;
} ProductUI_Diagnostics;
extern volatile ProductUI_Diagnostics g_product_ui;
uint32_t ProductUI_Init(uint32_t now_ms,uint32_t boot_held_mask);
void ProductUI_Process(uint32_t now_ms);
/* Task-context, nonblocking snapshot publication. Caller owns accumulated
 * millimetres and validity, including persistence. Latest snapshot wins;
 * valid local TripComputer A/B override those slots. ODO slot is ignored.
 * Returns0 for NULL/not initialized;1 means copied, not persisted. */
uint32_t ProductUI_PublishDistances(const UiDashboardDistances *distances);
/* Same nonblocking task-only snapshot contract. Valid bits0/1/2 select
 * OIL/BELT/SERV; invalid data displays an unknown track. No reset/save action. */
uint32_t ProductUI_PublishMaintenance(const UiDashboardMaintenance *maintenance);
void ProductUI_Button(uint32_t button,uint32_t event,uint32_t duration_ms,uint32_t now_ms);
/* LIVE BT producer only; development injection never writes these slots.
 * Nonblocking companion-facing publications. Current link token prevents
 * previous-peer data from reappearing after a disconnect/reconnect. */
uint32_t ProductUI_PhoneToken(uint32_t slot);
uint32_t ProductUI_RideSession(void);
uint32_t ProductUI_PublishPhone(uint32_t slot,uint32_t token,const PhoneStatus *status,uint32_t now_ms);
uint32_t ProductUI_PublishMusic(uint32_t slot,uint32_t token,const PhoneMusic *music,uint32_t now_ms);
uint32_t ProductUI_SelectPhone(uint32_t slot);
/* sample=NULL restores DATA_DEBUG's default source. TTL1000..180000ms;
 * overrides require DATA_DEBUG and a development viewport build. Vehicle
 * speed/ODO/IGN always use actual UART/board input. */
uint32_t ProductUI_InjectDevelopmentData(const DevelopmentSample *sample,uint32_t ttl_ms);
extern TripComputer g_product_trips; /* Read while g_product_ui.seq is even/stable. */
void ProductUI_InitModel(uint32_t now);
#endif
