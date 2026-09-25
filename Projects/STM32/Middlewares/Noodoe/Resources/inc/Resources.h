#ifndef NOODOE_RESOURCES_H
#define NOODOE_RESOURCES_H
#include <stdint.h>
#include "Resources_Expected.h"
#define RESOURCES_SLOT_BYTES (512U*1024U)
#define RESOURCES_HEADER_BYTES 4096U
#define RESOURCES_COMMIT 0x434D5431U
#define RESOURCES_MAGIC 0x31534352U
typedef enum { RESOURCES_IDLE,RESOURCES_QUEUED,RESOURCES_HEADER,RESOURCES_LOADING,
    RESOURCES_READY,RESOURCES_FAILED } ResourcesState;
typedef enum { RESOURCE_OK,RESOURCE_MISSING,RESOURCE_SIZE,RESOURCE_FORMAT,
    RESOURCE_VERSION,RESOURCE_CRC,RESOURCE_SHA,RESOURCE_RAM,RESOURCE_IO } ResourcesError;
typedef struct { const uint8_t *data;uint32_t bytes; } ResourceView;
typedef struct {uint32_t magic,version,state,error,slot,loaded,total,address,requests,failures;} ResourcesDiagnostics;
extern volatile ResourcesDiagnostics g_resources;
/* Nonblocking request/snapshot interfaces. A READY arena is immutable until
 * reset; installing a package never changes pointers retained by a consumer. */
uint32_t Resources_RequestLoad(void);
ResourcesState Resources_GetStatus(void);
uint32_t Resources_Get(uint32_t id,ResourceView *out);
/* StorageTask only: at most one4KiB file read/hash step per call. */
void Resources_Process(void);
/* Shared pure verifier, also used by installer and host fault injection.
 * Header success validates bounds, entry layout, CRC and expected asset ID. */
ResourcesError Resources_CheckHeader(const uint8_t *header,const uint8_t required[32]);
/* APP requirement record has a fixed flash offset and survives LTO/GC. */
typedef struct {uint32_t magic,version,required;uint8_t sha256[32];} ResourceRequirement;
extern const ResourceRequirement g_resource_requirement;
#endif
