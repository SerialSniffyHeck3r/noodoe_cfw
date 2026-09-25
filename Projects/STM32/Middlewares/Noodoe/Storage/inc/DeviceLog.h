#ifndef NOODOE_DEVICE_LOG_H
#define NOODOE_DEVICE_LOG_H
#include "event_log.h"
typedef struct {uint32_t state,error,queued,dropped,coalesced,sequence,written,boot_id;} DeviceLogStatus;
extern volatile DeviceLogStatus g_device_log;
/* Normal task callers only; short critical section, no storage in caller.
 * Fault ISR uses the retained mailbox instead. Zero = queue full/unavailable. */
uint32_t DeviceLog_Post(const DeviceEvent *);
void DeviceLog_Process(uint32_t now,uint32_t drain);
uint32_t DeviceLog_Busy(void);
/* Nonblocking read mailbox: latest journal sequence fences each 256-byte
 * chunk. A changed ring returns STALE; callers restart rather than mix boots. */
uint32_t DeviceLog_RequestRead(uint32_t token,uint32_t expected_sequence,uint32_t offset);
uint32_t DeviceLog_ReadResult(uint32_t token,uint8_t out[256]);
#endif
