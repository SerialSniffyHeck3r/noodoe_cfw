#ifndef NOODOE_GATE_STORE_H
#define NOODOE_GATE_STORE_H
#include "gate_abi.h"
#include "Recovery_Store.h"
enum {GATE_FILE_A,GATE_FILE_B,GATE_FILE_BOOT,GATE_FILE_STOCK,GATE_FILE_RESOURCES,GATE_FILE_LOG,
#if NOODOE_UNINSTALL
 GATE_FILE_CONFIG,GATE_FILE_RIDE,GATE_FILE_PHOTO,GATE_FILE_TEXT,
#endif
 GATE_FILE_COUNT};
enum {GATE_STORE_AUDIT,GATE_STORE_READY,GATE_STORE_ERROR};
typedef struct {
 RecoveryStoreRawRead read_raw;void *io;uint32_t uid[3];
 uint32_t state,error,phase,sector,root,current,head,tail,found,identity;
 uint32_t map[GATE_FILE_COUNT][32];
#if NOODOE_UNINSTALL
 uint32_t root_entry[GATE_FILE_COUNT];
#endif
 uint8_t fat[8192],claimed[510],block[4096];uint16_t directories[4080];
} GateStore;
void GateStore_Init(GateStore *,RecoveryStoreRawRead,void *,const uint32_t uid[3]);
uint32_t GateStore_Process(GateStore *);
uint32_t GateStore_Read(GateStore *,uint32_t file,uint32_t offset,void *,uint32_t);
/* Returns physical location only for a complete audited chain. Caller still
 * needs a valid UID-bound journal before requesting a narrow write grant. */
uint32_t GateStore_Address(const GateStore *,uint32_t file,uint32_t offset,uint32_t *);
#endif
