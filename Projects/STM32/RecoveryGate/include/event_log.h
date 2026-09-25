#ifndef NOODOE_EVENT_LOG_H
#define NOODOE_EVENT_LOG_H
#include <stdint.h>
/* Same canonical little-endian journal in standalone Gate and StorageTask.
 * Last sector is immutable UID-bound identity, never a rotating active pointer. */
#define EVENT_LOG_BYTES 0x40000U
#define EVENT_LOG_IDENTITY 0x3F000U
#define EVENT_LOG_SECTORS 63U
#define EVENT_LOG_EVENTS 63U
#define EVENT_LOG_MAGIC 0x31474C4EU
#define EVENT_LOG_COMMIT 0x31544D43U
enum {LOG_BOOT=1,LOG_RESET,LOG_UPDATE,LOG_ROLLBACK,LOG_STORAGE,LOG_BT,LOG_HEALTH,LOG_MEMORY,LOG_FAULT};
typedef struct {
 uint32_t code,detail,boot,transaction,time_ms,count,first_ms,last_ms;
 uint32_t data[8];
} DeviceEvent;
_Static_assert(sizeof(DeviceEvent)==64,"fixed event ABI");
typedef struct {
 void *context;
 uint32_t (*read)(void *,uint32_t,void *,uint32_t);
 uint32_t (*grant)(void *);
 uint32_t (*erase)(void *,uint32_t);
 uint32_t (*program)(void *,uint32_t,const void *,uint32_t);
} EventLogIO;
typedef struct {
 EventLogIO io;uint32_t uid[3],phase,error,scan,found,latest,sequence,target,page,count,written;
 uint8_t block[4096],verify[256];
} EventLog;
enum {LOG_SCAN=1,LOG_READY,LOG_ERASE,LOG_PROGRAM,LOG_VERIFY,LOG_COMMIT,LOG_FINAL,LOG_ERROR};
void EventLog_Identity(uint8_t *,const uint32_t uid[3]);
uint32_t EventLog_CheckIdentity(const uint8_t *,const uint32_t uid[3]);
void EventLog_Init(EventLog *,const EventLogIO *,const uint32_t uid[3]);
/* One physical read/program/erase per invocation, never dynamic allocation.
 * Caller serializes access and must drain before sleep or exclusive backup. */
void EventLog_Process(EventLog *);
uint32_t EventLog_Submit(EventLog *,const DeviceEvent *,uint32_t count);
uint32_t EventLog_CheckRecord(const uint8_t *,const uint32_t uid[3]);
#endif
