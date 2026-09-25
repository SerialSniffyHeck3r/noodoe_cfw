#ifndef RECOVERY_CORE_H
#define RECOVERY_CORE_H
#include "Update_Service.h"
#include "Recovery_Target.h"
#define RECOVERY_STOCK_VERSION 0x00100005U
#define RECOVERY_STOCK_BYTES 0x70000U
#define RECOVERY_CONFIRM_TOKEN 0x53544F43U
extern const uint8_t recovery_stock_sha256[32];
/* Exact immutable BL code0x08000000..0x08007FFF; mutable metadata excluded. */
uint32_t RecoveryResident_Verify(const uint8_t code[32768]);
typedef struct {void *context;uint32_t (*read)(void *,uint32_t,void *,uint32_t);} RecoverySource;
typedef struct {
 void *context;
 uint32_t (*read_stage)(void *,uint32_t,void *,uint32_t);
 uint32_t (*enable)(void *);
 uint32_t (*erase4k)(void *,uint32_t);
 uint32_t (*program)(void *,uint32_t,const void *,uint32_t);
 void (*disable)(void *);
 uint32_t (*metadata_read)(void *,uint32_t[5]);
 uint32_t (*metadata_commit)(void *,uint32_t,uint32_t);
 void (*reset)(void *);
} RecoveryPlatform;
typedef enum {RECOVERY_IDLE=0,RECOVERY_SOURCE_HASH,RECOVERY_ERASE,
 RECOVERY_SOURCE_READ,RECOVERY_PROGRAM,RECOVERY_STAGE_HASH,
 RECOVERY_VERIFIED,RECOVERY_COMMITTED,RECOVERY_ERROR} RecoveryState;
typedef enum {RECOVERY_OK=0,RECOVERY_ARGUMENT,RECOVERY_IO,RECOVERY_HASH,
 RECOVERY_PENDING,RECOVERY_RESIDENT,RECOVERY_STATE,RECOVERY_AMBIGUOUS,
 RECOVERY_CANCELLED,RECOVERY_VECTOR} RecoveryResult;
typedef struct {
 RecoveryPlatform platform;RecoverySource source;UpdateSha256 sha;
 uint32_t state,result,offset,page,crc,stock_crc,destructive,commit_attempted,resident_version;
 uint8_t buffer[4096],digest[32];
} RecoveryCore;
/* Allocation-free core. Callbacks own canonical/pair-swapped adaptation and
 * exclusive storage ownership. Source and all 448KiB of staging are separately
 * hashed against a compiled-in approved image, never a caller-supplied hash. */
void RecoveryCore_Init(RecoveryCore *,const RecoveryPlatform *);
uint32_t RecoveryCore_Request(RecoveryCore *,const RecoverySource *);
/* One bounded physical operation per invocation, except final metadata commit.
 * VERIFIED never installs implicitly; a second explicit commit is necessary. */
void RecoveryCore_Process(RecoveryCore *);
uint32_t RecoveryCore_Commit(RecoveryCore *,uint32_t token);
uint32_t RecoveryCore_Reset(RecoveryCore *,uint32_t token);
void RecoveryCore_Cancel(RecoveryCore *);
uint32_t RecoveryStock_Matches(uint32_t version,const uint8_t sha[32]);
#endif
