#ifndef APP_PERSISTENCE_H
#define APP_PERSISTENCE_H
#include "Trip_Computer.h"
#include "Ui_State.h"
#include "Cfw_Store.h"
typedef struct {uint32_t magic,ready,config_restored,ride_restored,ride_status,
    dirty,saving,request,last_success_ms,overdue,failures,force,restore_count;
    uint64_t saved_ign_ms,base_ign_ms,reserve_mm;} AppPersistenceStatus;
extern volatile AppPersistenceStatus g_app_persistence;
/* UI-owner only: applies stable field IDs, restores durable base once and
 * publishes a bounded ride snapshot. StorageTask alone performs NOR work. */
void AppPersistence_UI(uint32_t now,TripComputer *trips,UiState *ui);
void AppPersistence_Process(uint32_t now);
uint32_t AppPersistence_Busy(void);
/* Called on official SESSION_END and explicit trip/service reset, not raw IGN. */
uint32_t RideStore_RequestCheckpoint(uint32_t *id);
uint32_t RideStore_GetResult(uint32_t id);
uint32_t RideStore_GetUsage(uint64_t *base,uint64_t *committed,uint32_t *known);
/* RAM apply completion remains AppSettings_GetResult. This second result
 * reports durability for the same ID (last eight requests), CFW_EXPIRED when
 * retired, CFW_ARGUMENT for actions such as RTC which do not use the journal. */
uint32_t AppSettings_GetSaveResult(uint32_t request_id);
uint32_t AppPersistence_SettingsReady(void);
void AppPersistence_SettingsApplied(uint32_t id,uint32_t key,uint32_t apply_result);
uint32_t AppPersistence_FieldKey(uint32_t field);
uint32_t AppPersistence_FieldAt(uint32_t index);
uint32_t AppPersistence_FieldCount(void);
#endif
