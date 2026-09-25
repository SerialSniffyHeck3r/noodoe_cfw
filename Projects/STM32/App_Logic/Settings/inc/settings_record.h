#ifndef SETTINGS_RECORD_H
#define SETTINGS_RECORD_H
#include "SettingsService.h"
#define SETTINGS_RECORD_LEGACY_BYTES 244U
#define SETTINGS_RECORD_V2_BYTES 260U
#define SETTINGS_RECORD_BYTES 312U
typedef struct {uint32_t generation;Settings_Values values;Bluetooth_KeyStore keys;} Settings_Record;
/* Pure format boundary: explicit LE serialization, no HAL/NOR/RTOS calls.
 * Decode accepts schema1/244, schema2/260 or schema3/312 bytes; old records have
 * an empty rider name. New explicit commits use v3, with no boot-time rewrite.
 * Checks matching UID, known schema/layout, a valid
 * provisioned flag, CRC, and bounded fields. Output is untouched on failure. */
uint32_t SettingsRecord_Encode(uint8_t out[SETTINGS_RECORD_BYTES],const Settings_Record *record,const uint32_t uid[3]);
uint32_t SettingsRecord_Decode(Settings_Record *out,const uint8_t *data,uint32_t length,const uint32_t uid[3]);
uint32_t SettingsRecord_ValuesValid(const Settings_Values *values);
#endif
