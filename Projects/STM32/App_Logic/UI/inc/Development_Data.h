#ifndef DEVELOPMENT_DATA_H
#define DEVELOPMENT_DATA_H
#include "App_DataConfig.h"
#include <stdint.h>
/* Independent per-phone fixture players; never send Bluetooth commands. */
void DevelopmentData_SelectPhone(uint32_t slot);
uint32_t DevelopmentData_MediaAction(uint32_t slot,uint32_t action,uint32_t now);
#include "Dashboard_Pages.h"
#define DEVELOPMENT_REMOTE 1U
#define DEVELOPMENT_TRIPS 2U
/* Deliberately contains NO vehicle speed/ODO/IGN/RTC or outbound commands.
 * This compact producer is separate from future BT's PhoneContent payloads.
 * Units: minutes,0.1km,km/h,milliseconds,latitude/longitude in1e-7 degrees. */
typedef struct {
    uint32_t mask,moving_minutes[4],stopped_minutes[4],distance_tenths_km[4],max_kph[4];
    uint32_t battery_percent,notification_count,position_ms,duration_ms;
    int32_t latitude_e7,longitude_e7;
    uint32_t reserved_obd_rpm; /* ABI2 reserved, ignored. */
    char notification_title[48],notification_body[96],music_title[64],music_artist[48];
} DevelopmentSample;
/* Version2: request_id commits last. command1=preset,2=sample,3=clear override.
 * Clearing/expiry restores DATA_DEBUG's default source, not always live. Host waits
 * for ack before modifying fields again. UI owner validates and consumes;
 * no flash, bus, link emulation, trip reset, or persistent settings writes. */
typedef struct {
    uint32_t magic,version,request_id,command,ttl_ms,ack_id,result,active_mask,expires_ms,now_ms;
    DevelopmentSample sample;
} DevelopmentMailbox;
extern volatile DevelopmentMailbox g_product_data;
void DevelopmentData_Init(void);
/* UI-owner poll and resolve; the live facts/caches remain untouched. Output
 * phone is a private UI scratch snapshot, never the real BT slot. */
void DevelopmentData_Poll(uint32_t now_ms);
/* Caller-owned ~720byte scratch lives until rendering returns. Reuses the
 * UI task's existing stack instead of reserving a second domain cache in SRAM. */
typedef struct {TripComputer trips;PhoneTrail trail;} DevelopmentScratch;
void DevelopmentData_Resolve(DashboardPageFacts *facts,PhoneContentSlot *scratch,DevelopmentScratch *work,uint32_t now_ms);
void DevelopmentData_Default(DevelopmentSample *sample);
/* Task caller serializes this bounded mailbox publication with a critical
 * section. NULL restores the configured default. DATA_DEBUG=0 refuses all
 * overrides. Busy/invalid input returns0, never waits. */
uint32_t DevelopmentData_Request(const DevelopmentSample *sample,uint32_t ttl_ms);
#endif
