#ifndef PHOTO_SERVICE_H
#define PHOTO_SERVICE_H
#include <stdint.h>
#define PHOTO_SLOTS 3U
#define PHOTO_IMPORT_WALLPAPER 0x100U
#define PHOTO_MAX_SIDE 480U
typedef struct {const uint8_t *pixels;uint32_t width,height,bytes,revision;} PhotoImage;
typedef struct {uint32_t magic,version,pending,active,ready_mask,failed_mask,slot,result,decoded_mcus;uint32_t file_bytes[3],width[3],height[3];} PhotoDiagnostics;
extern volatile PhotoDiagnostics g_photos;
/* Nonblocking requests, task context. Each published generation stays immutable
 * until the graphics owner releases it, including STOP/SDRAM retention. No NOR writes. */
uint32_t PhotoService_RequestLoad(uint32_t slot);
uint32_t PhotoService_Get(uint32_t slot,PhotoImage *image);
/* UI owner retires a previous SDRAM source only after neither the selected
 * image nor an in-flight GPU upload borrows it. Fixed two-buffer reuse, no
 * per-replacement allocation. Getter/ack perform RAM operations only. */
const void *PhotoService_Retiring(uint32_t slot);
void PhotoService_Release(uint32_t slot,const void *pixels);
/* Single storage worker only. At most eight JPEG MCUs per call after bounded
 * file loading. A failed file does not expose partially decoded pixels. */
void PhotoService_Process(void);
/* SWD development import. Host writes only slot/length/crc32/arm then sequence
 * LAST, after uploading to the advertised input buffer while idle. arm=BAK2
 * acknowledges a verified full NOR backup. No generic memory/flash opcode.
 * One outstanding request; ack is published last. Successful imports survive
 * reset. Existing slot files cannot be overwritten. ABI2: slot|0x100 creates
 * WALLn.JPG; decode validates into discarded tiles and activation requires
 * reboot, leaving all published pixel pointers immutable.
 * Product ABI3 instead accepts plain slot0..2 and delegates to PhotoStore.
 * Existing CFW slots can be replaced transactionally; no FAT/original-photo
 * write is performed. Ack means verified NOR commit, not GPU upload complete.
 * ABI2 tools must refuse ABI3 rather than reuse a WALLn.JPG write plan. */
typedef struct {uint32_t magic,version,buffer,capacity,slot,length,crc32,arm,sequence,result,ack;} PhotoImport;
extern volatile PhotoImport g_photo_import;
#endif
