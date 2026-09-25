#ifndef STORAGE_DISK_H
#define STORAGE_DISK_H
#include <stdint.h>
/* Logical FAT adapter only. Raw NOR backup/staging APIs retain physical byte
 * order. Stock mode is read-only: its FAT overlaps the CFW NVM reservation. */
uint32_t StorageDisk_Detect(void);
uint32_t StorageDisk_IsStock(void);
uint32_t StorageDisk_CanWrite(void);
/* Only StorageService's mutex-protected, create-only photo import may enter
 * this scope. General file/NVM APIs continue rejecting stock media. */
void StorageDisk_ImportScope(uint32_t active);
/* A resource replacement may write only its audited inactive slot. */
void StorageDisk_ImportRange(uint32_t first_sector,uint32_t count);
/* Fixed-container page adapter. Performs the same logical-to-wire transform
 * as disk_write but has no FAT/general-write permission. Returns CFW status. */
uint32_t StorageDisk_ContainerProgram(uint32_t file,uint32_t address,const void *data,uint32_t bytes);
#endif
