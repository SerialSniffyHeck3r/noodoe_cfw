#ifndef NOODOE_GATE_ENGINE_H
#define NOODOE_GATE_ENGINE_H
#include "gate_abi.h"
#include "Update_Service.h"
enum {GATE_ENGINE_IDLE,GATE_ENGINE_SOURCE,GATE_ENGINE_INTENT,GATE_ENGINE_ERASE,
 GATE_ENGINE_COPY,GATE_ENGINE_VERIFY,GATE_ENGINE_FINISH,GATE_ENGINE_READY,GATE_ENGINE_FAILED};
enum {GATE_E_ARGUMENT=1,GATE_E_SOURCE,GATE_E_HASH,GATE_E_VECTOR,GATE_E_JOURNAL,GATE_E_FLASH};
typedef struct {
 void *context;
 uint32_t (*source)(void *,uint32_t slot,uint32_t offset,void *,uint32_t);
 uint32_t (*read_flash)(void *,uint32_t offset,void *,uint32_t);
 uint32_t (*erase)(void *,uint32_t sector);
 uint32_t (*program)(void *,uint32_t address,const void *,uint32_t);
 uint32_t (*journal)(void *,const GateJournalRecord *);
} GateEngineIO;
typedef struct {
 GateEngineIO io;GateJournalRecord record;GateImageInfo image;
 uint32_t state,error,slot,install,offset,sector;UpdateSha256 sha;
 uint8_t buffer[4096];
} GateEngine;
/* The caller verifies the UID-bound image header, audited FAT chain and full
 * required resource package first. No write is possible before source SHA. */
uint32_t GateEngine_Begin(GateEngine *,const GateEngineIO *,const GateJournalRecord *,
 const GateImageInfo *,uint32_t slot,uint32_t install);
void GateEngine_Process(GateEngine *);
#endif
