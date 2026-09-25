#ifndef NOODOE_BOOT_STORE_H
#define NOODOE_BOOT_STORE_H
#include <stdint.h>
#include "gate_abi.h"
typedef struct {uint32_t state,error,sequence,active,candidate,confirmed,writes;} BootStoreDiagnostics;
extern volatile BootStoreDiagnostics g_boot_store;
/* StorageTask only. Audit is shared with CfwFiles; one journal sector is read
 * per Process. No file creation, FAT mutation or implicit repair is allowed. */
void BootStore_Process(void);
void BootStore_RequestConfirm(void);
uint32_t BootStore_RequestResultAck(uint32_t transaction);
uint32_t BootStore_GetBootInfo(GateJournalRecord *);
uint32_t BootStore_MatchTrial(uint32_t sequence,const uint8_t sha[32]);
uint32_t BootStore_FinishSettingsReset(uint32_t epoch);
uint32_t BootStore_Ready(void);
uint32_t BootStore_BeginUpdate(uint32_t transaction);
uint32_t BootStore_Resume(uint32_t version,uint32_t crc,const uint8_t sha[32],uint32_t *verified_bytes);
uint32_t BootStore_Checkpoint(uint32_t verified_bytes);
uint32_t BootStore_Read(uint32_t offset,void *data,uint32_t bytes);
uint32_t BootStore_Erase(uint32_t offset);
uint32_t BootStore_Program(uint32_t offset,const void *data,uint32_t bytes);
uint32_t BootStore_Commit(uint32_t version,const uint8_t sha[32],const uint8_t requirement[44]);
uint32_t BootStore_CommitDiagnostic(uint32_t version,const uint8_t sha[32],const uint8_t requirement[44]);
uint32_t BootStore_Committed(const uint8_t sha[32]);
void BootStore_EndUpdate(void);
#endif
