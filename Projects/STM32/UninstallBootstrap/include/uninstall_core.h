#ifndef NOODOE_UNINSTALL_CORE_H
#define NOODOE_UNINSTALL_CORE_H
#include "gate_store.h"
#define UNINSTALL_JOURNAL_BASE 0x08011000U
#define UNINSTALL_JOURNAL_BYTES 0xF000U
#define UNINSTALL_METADATA_BYTES 0x9000U
#define UNINSTALL_CONFIRM 0x45524153U
enum {UNINSTALL_NEW,UNINSTALL_READY,UNINSTALL_ERASING,UNINSTALL_METADATA,
      UNINSTALL_CLEAN,UNINSTALL_ERROR};
enum {UNINSTALL_IO=1,UNINSTALL_FAT,UNINSTALL_OWNER,UNINSTALL_JOURNAL,
      UNINSTALL_IDENTITY,UNINSTALL_APPROVAL,UNINSTALL_CHANGED};
/* NOR callbacks use physical bytes. The only pair-swap lives in this adapter.
 * Journal callbacks use offsets inside the dedicated 60KiB internal region.
 * There is deliberately no internal erase callback: S4 cannot erase itself. */
typedef struct {
 void *context;
 uint32_t (*read)(void *,uint32_t,void *,uint32_t);
 uint32_t (*erase)(void *,uint32_t);
 uint32_t (*program)(void *,uint32_t,const void *,uint32_t);
 uint32_t (*journal_read)(void *,uint32_t,void *,uint32_t);
 uint32_t (*journal_program)(void *,uint32_t,const void *,uint32_t);
 void (*progress)(void *,uint32_t,uint32_t,uint32_t);
} UninstallIO;
typedef struct {
 UninstallIO io;GateStore audit;
 uint32_t uid[3],state,error,resuming,approved,found,bytes,cluster,sector,metadata_sector;
 uint8_t identity[32],header[4096],original[UNINSTALL_METADATA_BYTES];
 uint8_t block[4096],verify[4096];
} UninstallCore;
/* Identity is the SHA of immutable BL code + factory sector, excluding the
 * resident install metadata which is expected to change at final handoff. */
uint32_t Uninstall_Init(UninstallCore *,const UninstallIO *,const uint32_t uid[3],const uint8_t identity[32]);
uint32_t Uninstall_Audit(UninstallCore *);
/* Called only after complete embedded-stock verification and fresh O2s. */
uint32_t Uninstall_Approve(UninstallCore *,uint32_t token);
/* One content sector / metadata sector per call. Reset repeats physical
 * verification; cached progress is never evidence that bytes were erased. */
uint32_t Uninstall_Process(UninstallCore *);
#endif
