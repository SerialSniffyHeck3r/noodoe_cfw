#ifndef RECOVERY_STORE_H
#define RECOVERY_STORE_H
#include <stdint.h>
#include "Update_Service.h"
#define RECOVERY_STORE_BYTES 0x80000U
#define RECOVERY_IMAGE_BYTES 0x70000U
#define RECOVERY_IMAGE_OFFSET 4096U
#define RECOVERY_STORE_SAFE_END 0x07F70000U
enum { RECOVERY_STORE_IDLE,RECOVERY_STORE_AUDITING,RECOVERY_STORE_HASHING,
       RECOVERY_STORE_READY,RECOVERY_STORE_MISSING,RECOVERY_STORE_FAILED };
enum { RECOVERY_STORE_OK,RECOVERY_STORE_IO,RECOVERY_STORE_FORMAT,
       RECOVERY_STORE_UID,RECOVERY_STORE_HASH,RECOVERY_STORE_BOUNDS };
/* read_raw returns zero on success and preserves physical NOR wire order.
 * Caller owns this fixed SRAM context; no heap, SDRAM, RTOS or FatFs needed.
 * Process reads at most one 4KiB sector. Initialization never writes. */
typedef uint32_t (*RecoveryStoreRawRead)(void *,uint32_t,void *,uint32_t);
typedef struct {
    RecoveryStoreRawRead read_raw;void *io;
    uint32_t uid[3],state,error,phase,current,sector,root,head,tail,found,position;
    const uint8_t *resident;
    uint32_t image_map[16];
    uint8_t fat[8192],claimed[510],block[4096];
    uint16_t directories[4080];
    UpdateSha256 sha;
} RecoveryStore;
void RecoveryStore_Init(RecoveryStore *,RecoveryStoreRawRead,void *,const uint32_t uid[3]);
uint32_t RecoveryStore_Process(RecoveryStore *);
/* Canonical APP bytes only; permitted only after complete identity/SHA audit. */
uint32_t RecoveryStore_Read(void *,uint32_t offset,void *destination,uint32_t bytes);
/* Shared installer/header validator. The approved stock identity is pinned,
 * never supplied by the sending peer. Header contains target UID. */
uint32_t RecoveryStore_CheckHeader(const uint8_t *,const uint32_t uid[3]);
uint32_t RecoveryStore_CheckTarget(const uint8_t *,const uint8_t lower[65536]);
extern const uint8_t recovery_stock_sha256[32];
#endif
