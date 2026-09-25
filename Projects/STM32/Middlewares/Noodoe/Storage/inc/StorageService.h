#ifndef STORAGE_SERVICE_H
#define STORAGE_SERVICE_H
#include <stdint.h>
#include "ff.h"
/* API calls are task-only and internally serialized. File names currently use
 * FAT8.3 syntax. Full names and a product-level file-transfer protocol can be
 * added without exposing the disk driver or arbitrary NOR commands to callers.
 * Ordinary writes accept either a validated provisioning runtime permit or the
 * host backup gate; format always requires the separate current host gate.
 */
typedef struct {
    uint32_t magic,version,initialized,mounted,last_result,formats,file_reads,file_writes;
    uint32_t nvm_valid,nvm_sequence,nvm_length,nvm_slot,nvm_commits,nvm_invalid;
} StorageService_Diagnostics;
extern volatile StorageService_Diagnostics g_storage_service;
#define STORAGE_NVM_MAX_PAYLOAD 2048U
/* Incremental IEEE CRC32. Start at FFFFFFFF, XOR FFFFFFFF after the last part. */
uint32_t StorageService_Crc32(const uint8_t *data,uint32_t length,uint32_t seed);
/* Initialization mounts only: an old/unrecognized volume is never formatted.
 * FR_NO_FILESYSTEM is a normal first-bring-up result and leaves USB backup live. */
FRESULT StorageService_Init(void);
FRESULT StorageService_Mount(void);
/* Destructive format requires the explicit host backup unlock in this boot.
 * It touches only FS range; NVM and both staging reservations are preserved.
 * FAT itself is not transactional across power loss. Re-mount/check after loss. */
FRESULT StorageService_Format(void);
FRESULT StorageService_ReadFile(const char *path,uint32_t offset,void *destination,uint32_t capacity,uint32_t *received);
FRESULT StorageService_WriteFile(const char *path,const void *source,uint32_t length);
/* Returns one JPEG's FAT short alias from album/0..2. Enumeration and reads
 * run on the storage worker; a missing/ambiguous folder is reported, not made. */
/* Explicit host import only: creates CFW.JPG in an empty stock album folder,
 * validates readback, never replaces any file. token acknowledges the host's
 * verified backup. FAT updates are not power-fail atomic. Not an auto-provision. */
FRESULT StorageService_CreateAlbumPhoto(uint32_t slot,const uint8_t *jpeg,uint32_t length,uint32_t backup_token);
FRESULT StorageService_FindWallpaper(uint32_t slot,char *path,uint32_t capacity,uint32_t *length);
FRESULT StorageService_CreateWallpaper(uint32_t slot,const uint8_t *jpeg,uint32_t length,uint32_t backup_token);
FRESULT StorageService_FindAlbumPhoto(uint32_t slot,char *path,uint32_t capacity,uint32_t *length);
FRESULT StorageService_Stat(const char *path,uint32_t *length);
FRESULT StorageService_Mkdir(const char *path);
FRESULT StorageService_Remove(const char *path);
/* NVM is one application-defined versioned blob. Sixteen4KiB journal slots
 * rotate inside the64KiB reservation; old committed data survives an interrupted
 * next write. Callers own schema/version migration within the payload itself. */
FRESULT StorageService_NVMGet(void *destination,uint32_t capacity,uint32_t *received);
FRESULT StorageService_NVMPut(const void *source,uint32_t length);
#endif
