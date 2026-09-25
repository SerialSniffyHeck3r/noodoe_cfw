#ifndef STORAGE_BACKUP_H
#define STORAGE_BACKUP_H
#include <stdint.h>

/* Layout is an address contract only. This service never mounts/formats media
 * or writes filesystem/NVM/staging; raw backup always includes all128MiB. */
#define STORAGE_FS_BYTES 0x07F70000UL
#define STORAGE_NVM_ADDRESS 0x07F70000UL
#define STORAGE_NVM_BYTES 0x00010000UL
#define STORAGE_BL_STAGING_ADDRESS 0x07F80000UL
#define STORAGE_APP_STAGING_ADDRESS 0x07F90000UL
#define STORAGE_APP_STAGING_BYTES 0x00070000UL

typedef struct {
    uint32_t magic,version,initialized,transport_verified,result,streaming;
    uint32_t sequence,address,remaining,requests,frames,bytes_exported,bad_requests;
} StorageBackup_Diagnostics;
extern volatile StorageBackup_Diagnostics g_storage_backup;
/* Historical filename/record retained for existing SWD tooling and storage
 * layout constants. CDC export has been removed; use StorageSWD for backup.
 * Read-only polling/DMA qualification returns zero only on successful NOR IO. */
uint32_t StorageTransport_Init(void);
#endif
