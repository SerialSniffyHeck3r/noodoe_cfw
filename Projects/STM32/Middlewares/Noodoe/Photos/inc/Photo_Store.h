#ifndef PHOTO_STORE_H
#define PHOTO_STORE_H
#include "Cfw_Store.h"
#define PHOTO_STORE_BANK_BYTES 163840U
#define PHOTO_STORE_MAX_BYTES 131072U
typedef struct {uint32_t ready,error,busy,request,completed,result,last_success_ms;
    uint32_t generation[3],length[3],active[3],failed_mask,unsupported;} PhotoStoreStatus;
extern volatile PhotoStoreStatus g_photo_store;
void PhotoStore_Process(uint32_t now);
uint32_t PhotoStore_Busy(void);
/* Request copies at most128KiB into its fixed SDRAM input. Caller may release
 * input on return. Completion means NOR readback + FULL JPEG decode + commit.
 * Upper layers never perform file or NOR I/O. */
uint32_t PhotoStore_RequestReplace(uint32_t slot,const void *jpeg,uint32_t bytes,uint32_t *id);
uint32_t PhotoStore_GetResult(uint32_t id);
/* StorageTask loader only; selected complete generation, never a partial bank. */
uint32_t PhotoStore_Read(uint32_t slot,void *out,uint32_t capacity,uint32_t *bytes);
/* Storage owner only, under durable boot RESET_PENDING. Erase/readback both
 * headers per slot; repeat after power loss until all slots are empty. */
uint32_t PhotoStore_ResetSlots(void);
#endif
