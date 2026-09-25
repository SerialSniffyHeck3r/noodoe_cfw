#ifndef CONFIG_STORE_H
#define CONFIG_STORE_H
#include "Cfw_Store.h"
/* Registered field IDs, independent of C enums and struct layouts. Field
 * payloads have their own documented LE/UTF-8 formats. Unknown fields survive. */
#define CONFIG_FIELD_DEVICE 0x0200U
#define CONFIG_FIELD_RIDER 0x0201U
#define CONFIG_FIELD_BT_KEYS 0x0202U
#define CONFIG_FIELD_ELM 0x0203U
#define CONFIG_FIELD_RESET_EPOCH 0x02FEU
void ConfigStore_SetTrialView(uint32_t enabled);
uint32_t ConfigStore_ResetPreferences(uint32_t epoch,uint32_t *revision);
typedef struct {uint32_t ready,error,revision,saved_revision,saving_revision,
    dirty_since,changed_ms,last_success_ms,failures,last_result;} ConfigStoreStatus;
extern volatile ConfigStoreStatus g_config_store;
void ConfigStore_Process(uint32_t now);
uint32_t ConfigStore_Busy(void);
uint32_t ConfigStore_Get(uint32_t field,void *out,uint32_t capacity,uint32_t *bytes);
uint32_t ConfigStore_Set(uint32_t field,const void *data,uint32_t bytes,uint32_t *revision);
/* Atomically publish a bounded group of LE32 fields; capacity/IDs are checked
 * before any RAM change. Used by combined preference commands. */
uint32_t ConfigStore_SetWords(const uint16_t *fields,const uint32_t *values,uint32_t count,uint32_t *revision);
/* Result of a coalesced snapshot revision, not a promise every intermediate
 * slider value was written. Saved revision denotes the applied latest intent. */
uint32_t ConfigStore_Result(uint32_t revision);
/* App semantic validation can make a supported container read-only. Unknown
 * values remain intact for a newer firmware or explicit offline recovery. */
void ConfigStore_Reject(uint32_t error);
#endif
