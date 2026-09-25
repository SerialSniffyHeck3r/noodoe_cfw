#ifndef SETTINGS_SERVICE_H
#define SETTINGS_SERVICE_H
#include <stdint.h>
#include "NoodoeBluetooth.h"
#include "Rider_Name.h"

#define SETTINGS_PROVISION_TOKEN 0x53455431UL
typedef enum {SETTINGS_OK=0,SETTINGS_ARGUMENT,SETTINGS_NOT_READY,
    SETTINGS_NOT_PROVISIONED,SETTINGS_BUSY,SETTINGS_DENIED,SETTINGS_STORAGE_ERROR,
    SETTINGS_INVALID_RECORD,SETTINGS_BT_IMPORT_ERROR} Settings_Status;
typedef enum {SETTINGS_STATE_OFF=0,SETTINGS_STATE_UNPROVISIONED,
    SETTINGS_STATE_READY,SETTINGS_STATE_SAVING,SETTINGS_STATE_ERROR} Settings_State;
/* Array index0 is ELM; index1 is a retired GPS record kept for disk compatibility.
 * It is never mapped to PHONE2 or used to initiate a connection. enabled remembers intent;
 * this service never initiates radio connections. channel0 requests future SDP. */
typedef struct {uint8_t address[6],channel,enabled;} Settings_RoleBinding;
typedef struct {uint32_t backlight_percent,units;Settings_RoleBinding roles[2];uint64_t oil_on_ms;uint32_t oil_usage_valid;char rider_name[RIDER_NAME_CAPACITY];} Settings_Values;
typedef struct {
    uint32_t magic,version,initialized,state,last_result,storage_result;
    uint32_t provisioned,record_generation,pending_request,completed_request;
    uint32_t completed_result,writes,write_failures,key_generation;
} Settings_Diagnostics;
extern volatile Settings_Diagnostics g_settings;

/* Product: StorageTask calls this after ConfigStore is ready and before BT.
 * It imports stable-field settings/keys from CFWCFG.DAT; no raw-NVM permit.
 * Other profiles: after StorageTransport_Init and before first Bluetooth_Start.
 * Scans/mounts/read-validates only. A valid UID/schema/layout/CRC provisioning
 * record imports keys and enables normal writes; missing/invalid records never
 * format, create a marker, or grant permission. No device settings are applied. */
Settings_Status SettingsService_Init(void);
/* The same storage worker calls Process periodically. BT keys are copied using
 * its atomic export API and persisted outside BT callbacks. Dirty/error retries
 * are spaced by1second. Acknowledge a key generation only after verified commit. */
void SettingsService_Process(uint32_t now_ms);
/* Nonblocking task-context requests copied into a single bounded slot. Success
 * means queued, not committed. Watch completed_request/completed_result in
 * diagnostics. A full slot returns BUSY without modifying either request. */
Settings_Status SettingsService_RequestProvision(uint32_t token,uint32_t *request_id);
Settings_Status SettingsService_RequestPreferences(uint32_t backlight_percent,uint32_t units,uint32_t *request_id);
Settings_Status SettingsService_RequestRoleBinding(Bluetooth_Role role,const uint8_t address[6],uint8_t channel,uint8_t enabled,uint32_t *request_id);
/* Product copies the current RAM settings (which may still be dirty). Query
 * diagnostics for durable completion. Other profiles copy committed values.
 * Defaults25%/metric/no bindings
 * are returned with NOT_PROVISIONED until a valid marker has been committed. */
Settings_Status SettingsService_GetValues(Settings_Values *out);
/* Product returns DENIED: lifetime usage belongs to RideStore and its live
 * counter. Use RideStore_RequestCheckpoint(), never replace an absolute total.
 * Other profiles retain the legacy provisioned-journal checkpoint API. */
Settings_Status SettingsService_RequestOilUsage(uint64_t on_ms,uint32_t *request_id);
void SettingsService_GetDiagnostics(Settings_Diagnostics *out);
/* Product applies the RAM name immediately and asynchronously persists field
 * 0x0201 in CFWCFG.DAT. Acceptance is not commit; inspect completed_request and
 * completed_result. Failure preserves the previous durable name. Empty clears.
 * Other profiles retain the legacy journal and explicit session override.
 * Neither path creates or formats storage. Product provisioning is host-only. */
Settings_Status SettingsService_RequestRiderName(const char *utf8,uint32_t bytes,uint32_t *request_id);
Settings_Status SettingsService_GetRiderName(char *out,uint32_t capacity);
#endif
