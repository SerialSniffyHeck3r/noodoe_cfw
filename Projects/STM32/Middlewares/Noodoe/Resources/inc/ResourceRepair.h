#ifndef RESOURCE_REPAIR_H
#define RESOURCE_REPAIR_H
#include <stdint.h>
/* Explicit SWD recovery only, called by the sole StorageTask owner while
 * Resources is not READY. Input/work remain owned until completion. Returns
 *0 for accepted/busy,1 for complete, or an error >=200. No allocation/format. */
uint32_t ResourceRepair_Begin(uint8_t *input,uint32_t capacity,uint8_t *work);
uint32_t ResourceRepair_Process(void);
#endif
