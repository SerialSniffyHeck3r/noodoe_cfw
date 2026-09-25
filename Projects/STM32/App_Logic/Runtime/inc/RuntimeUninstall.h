#ifndef RUNTIME_UNINSTALL_H
#define RUNTIME_UNINSTALL_H
#include <stdint.h>
/* Dedicated original-BL staging adapter. StorageTask is the only caller.
 * The common UpdateService owns transport, authentication, CRC/SHA and state. */
uint32_t RuntimeUninstall_Enable(uint32_t tx);
uint32_t RuntimeUninstall_Read(uint32_t a,void *,uint32_t n);
uint32_t RuntimeUninstall_Program(uint32_t a,const void *,uint32_t n);
uint32_t RuntimeUninstall_Commit(uint32_t version,uint32_t crc);
uint32_t RuntimeUninstall_Untouched(void);
#endif
