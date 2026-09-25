#ifndef CFW_FILES_H
#define CFW_FILES_H
#include <stdint.h>
enum {CFW_CONFIG,CFW_RIDE,CFW_PHOTOS,CFW_BOOT_A,CFW_BOOT_B,CFW_BOOT_JOURNAL,CFW_LOG,CFW_TEXT,CFW_FILE_COUNT};
enum {CFW_OK,CFW_PENDING,CFW_MISSING,CFW_CORRUPT,CFW_VERSION,CFW_IO,CFW_MEMORY,CFW_ARGUMENT,CFW_BUSY,CFW_EXPIRED};
#define CFW_SECTOR 4096U
#define CFW_SAFE_END 0x07F70000U
/* StorageTask only: full FAT ownership audit, bounded to one directory sector
 * per call. No allocation on disk, repair or write capability during audit. */
void CfwFiles_Begin(void);
uint32_t CfwFiles_Process(void);
uint32_t CfwFiles_Status(void);
uint32_t CfwFiles_Size(uint32_t file);
uint32_t CfwFiles_Present(uint32_t file);
uint32_t CfwFiles_Grant(uint32_t file);
uint32_t CfwFiles_Read(uint32_t file,uint32_t offset,void *data,uint32_t bytes);
uint32_t CfwFiles_Program(uint32_t file,uint32_t offset,const void *data,uint32_t bytes);
uint32_t CfwFiles_Erase(uint32_t file,uint32_t offset);
/* Explicit little-endian helpers used by stable formats, never struct dumps. */
uint32_t Cfw_Get32(const uint8_t *p);
void Cfw_Put32(uint8_t *p,uint32_t v);
uint32_t Cfw_Crc(const void *data,uint32_t bytes);
#endif
