#ifndef STORAGE_SWD_TEST_PORT_H
#define STORAGE_SWD_TEST_PORT_H
#include <stddef.h>
#include <stdint.h>
uint32_t StorageSWDTest_RamReady(void);
uint32_t StorageSWDTest_RamCapacity(void);
void *StorageSWDTest_Allocate(size_t bytes);
uint32_t StorageSWDTest_NorReady(void);
uint32_t StorageSWDTest_UID(uint32_t index);
uint32_t StorageSWDTest_Read(uint32_t address,void *destination,uint32_t length);
void StorageSWDTest_Barrier(void);
void StorageSWDTest_Sync(void);
uint32_t StorageSWDTest_RequestSeq(void);
#define SWD_PORT_RAM_READY() StorageSWDTest_RamReady()
#define SWD_PORT_RAM_CAPACITY() StorageSWDTest_RamCapacity()
#define SWD_PORT_RAM_ALLOCATE(bytes) StorageSWDTest_Allocate(bytes)
#define SWD_PORT_NOR_READY() StorageSWDTest_NorReady()
#define SWD_PORT_JEDEC() 0x00C2201BUL
#define SWD_PORT_UID(index) StorageSWDTest_UID(index)
#define SWD_PORT_READ(address,destination,length) StorageSWDTest_Read(address,destination,length)
#define SWD_PORT_BARRIER() StorageSWDTest_Barrier()
#define SWD_PORT_SYNC() StorageSWDTest_Sync()
#define SWD_PORT_REQUEST_SEQ() StorageSWDTest_RequestSeq()
#endif
