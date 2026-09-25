#ifndef RESOURCE_STORE_H
#define RESOURCE_STORE_H
#include <stdint.h>
/* Host owns command..sequence and uploads exactly one512KiB prepared slot.
 * Provision also requires an independently verified full NOR backup and FAT
 * reachability plan. Host commits sequence last; single outstanding request.
 * command1=create-only,2=replace inactive slot,3=explicit offline-reviewed FAT
 * recovery sector batch. Command3 is unavailable once assets are READY and
 * requires full-request/preimage/replacement SHA checks. Never auto-repair. */
typedef struct {
    uint32_t magic,version,buffer,capacity;
    uint32_t command,first_cluster,root_index,slot,token,sequence;
    uint32_t ack,state,error,progress,container_sector;
} ResourceStoreMailbox;
extern volatile ResourceStoreMailbox g_resource_install;
void ResourceStore_Process(void);
uint32_t ResourceStore_Busy(void);
/* Nonblocking compatibility check for a staged APP.1=verified available,
 *0=pending/missing. StorageTask processes SHA in bounded chunks. */
uint32_t ResourceStore_Compatible(const uint8_t required[32]);
/* 0x8D status / 8E begin / 8F RAM data / 90 publish / 91 cancel.
 * Fixed 92-byte status, no physical address supplied by the peer. */
uint32_t ResourceStore_Transfer(uint32_t op,const uint8_t *data,uint32_t bytes,uint8_t status[92]);
uint32_t ResourceStore_TransferActive(void);
uint32_t ResourceStore_TransferCancel(void);
#endif
