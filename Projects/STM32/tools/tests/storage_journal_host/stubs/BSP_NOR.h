#ifndef BSP_NOR_H
#define BSP_NOR_H
#include <stdint.h>
#define __get_IPSR() 0U
#define __get_PRIMASK() 0U
#define __get_BASEPRI() 0U
typedef enum {BSP_NOR_OK=0,BSP_NOR_ARGUMENT=1,BSP_NOR_BUSY=4,BSP_NOR_IO=5,BSP_NOR_LOCKED=8} BSP_NOR_Status;
BSP_NOR_Status BSP_NOR_Read(uint32_t address,void *data,uint32_t length);
BSP_NOR_Status BSP_NOR_Erase4K(uint32_t address);
BSP_NOR_Status BSP_NOR_Program(uint32_t address,const void *data,uint32_t length);
uint32_t BSP_NOR_IsStorageUnlocked(void);
uint32_t BSP_NOR_CanWriteStorage(void);
BSP_NOR_Status BSP_NOR_BeginFormat(void);
void BSP_NOR_EndFormat(void);
void BSP_NOR_LockStorage(void);
BSP_NOR_Status BSP_NOR_UnlockStorage(uint32_t token);
#endif
