#ifndef CFW_STORE_H
#define CFW_STORE_H
#include "Cfw_Files.h"
/* Read-only maintenance lease for a complete, coherent A/B NOR backup.
 * Host sets request=1 and waits for StorageTask ack=1; request=0 resumes.
 * RAM changes can remain dirty. This is not a durable write completion. */
typedef struct {uint32_t magic,request,ack;} CfwQuiesce;
extern volatile CfwQuiesce g_cfw_quiesce;
#define CFW_RECORD_MAGIC 0x314A4643U
#define CFW_RECORD_COMMIT 0x31544D43U
#define CFW_PAYLOAD_MAX 3968U
enum {CFW_STORE_LOADING,CFW_STORE_SAVED,CFW_STORE_DIRTY,CFW_STORE_SAVING,CFW_STORE_ERROR};
typedef struct {uint32_t status,state,generation,active_sector,request,last_success_ms,
    failures,writes,deduplicated,dirty;} CfwStoreStatus;
typedef struct {uint32_t magic,version,ready,error;CfwStoreStatus files[2];} CfwStoreDiagnostics;
extern volatile CfwStoreDiagnostics g_cfw_store;
/* StorageTask owner. A call issues at most one erase, page program or sector
 * read. A record is visible only after independent readback + commit readback. */
void CfwStore_Init(void);
void CfwStore_Process(uint32_t now);
uint32_t CfwStore_Busy(void);
uint32_t CfwStore_Ready(void);
uint32_t CfwStore_Read(uint32_t file,void *out,uint32_t capacity,uint32_t *bytes);
/* Copies caller data before returning. BUSY means nothing was accepted.
 * Eight terminal request results are retained; older IDs return EXPIRED,
 * never a false success or an indefinitely pending answer. */
uint32_t CfwStore_Request(uint32_t file,const void *data,uint32_t bytes,uint32_t *id);
uint32_t CfwStore_Result(uint32_t id);
void CfwStore_GetStatus(uint32_t file,CfwStoreStatus *out);
/* Shared journal/photo header: purpose, UID, schema, generation, payload CRC
 * are stable LE fields. Unknown newer schema never grants write permission. */
void CfwRecord_Make(uint8_t *sector,uint32_t purpose,uint32_t generation,const void *data,uint32_t bytes);
uint32_t CfwRecord_Check(const uint8_t *sector,uint32_t purpose);
#endif
