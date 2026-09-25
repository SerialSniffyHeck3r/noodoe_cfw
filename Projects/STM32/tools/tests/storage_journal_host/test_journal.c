/* Execute the actual service implementation; only NOR/RTOS/FatFs mount are
 * mocked. Interrupted program/erase changes part of the media before failing. */
#include <stdint.h>
#include <stddef.h>
#include "StorageService.c"
#include "write_range.inc"

volatile StorageBackup_Diagnostics g_storage_backup;
volatile uint32_t g_assertions,g_failure_line,g_mock_error;
static uint8_t media[65536],baseline[65536],payload[2048],received[2048];
static uint32_t unlocked,runtime_permit,formatting,format_calls,reset_in_format,cut_at,operations;
#define CHECK(x) do{++g_assertions;if(!(x)){g_failure_line=__LINE__;return 1;}}while(0)
void *memcpy(void *dest,const void *src,size_t n){uint8_t *d=dest;const uint8_t *s=src;for(size_t i=0;i<n;++i)d[i]=s[i];return dest;}
void *memset(void *dest,int value,size_t n){uint8_t *d=dest;for(size_t i=0;i<n;++i)d[i]=(uint8_t)value;return dest;}
int memcmp(const void *a,const void *b,size_t n){const uint8_t *x=a,*y=b;for(size_t i=0;i<n;++i)if(x[i]!=y[i])return x[i]-y[i];return 0;}
osKernelState_t osKernelGetState(void){return osKernelRunning;}
osMutexId_t osMutexNew(const void *attributes){(void)attributes;return (void*)1;}
osStatus_t osMutexAcquire(osMutexId_t mutex,uint32_t timeout){(void)mutex;(void)timeout;return osOK;}
osStatus_t osMutexRelease(osMutexId_t mutex){(void)mutex;return osOK;}
FRESULT f_mount(FATFS *fs,const TCHAR *path,BYTE opt){(void)path;(void)opt;return fs?FR_NO_FILESYSTEM:FR_OK;}
uint32_t BSP_NOR_IsStorageUnlocked(void){return unlocked;}
uint32_t BSP_NOR_CanWriteStorage(void){return formatting?unlocked:(unlocked || runtime_permit);}
uint32_t StorageDisk_Detect(void){return 1;}
uint32_t StorageDisk_IsStock(void){return 0;}
uint32_t StorageDisk_CanWrite(void){return BSP_NOR_CanWriteStorage();}
BSP_NOR_Status BSP_NOR_BeginFormat(void){if(!unlocked)return BSP_NOR_LOCKED;formatting=1U;return BSP_NOR_OK;}
void BSP_NOR_EndFormat(void){formatting=0U;}
FRESULT f_mkfs(const TCHAR *path,BYTE opt,DWORD au,void *work,UINT len)
{
    (void)path;(void)opt;(void)au;(void)work;(void)len;++format_calls;
    if(!formatting){g_mock_error=5U;return FR_DISK_ERR;}
    if(reset_in_format)unlocked=0U;
    return BSP_NOR_CanWriteStorage()?FR_OK:FR_WRITE_PROTECTED;
}
static uint32_t InRange(uint32_t address,uint32_t length)
{
    if(address<STORAGE_NVM_ADDRESS || address>=STORAGE_NVM_ADDRESS+65536U || length>STORAGE_NVM_ADDRESS+65536U-address){g_mock_error=1;return 0;}
    return 1;
}
BSP_NOR_Status BSP_NOR_Read(uint32_t address,void *data,uint32_t length)
{
    if(!InRange(address,length))return BSP_NOR_ARGUMENT;
    memcpy(data,media+address-STORAGE_NVM_ADDRESS,length);return BSP_NOR_OK;
}
BSP_NOR_Status BSP_NOR_Erase4K(uint32_t address)
{
    if(!InRange(address,4096U) || (address&4095U) || !BSP_NOR_CanWriteStorage()){g_mock_error=2;return BSP_NOR_ARGUMENT;}
    uint32_t cut=++operations==cut_at;
    memset(media+address-STORAGE_NVM_ADDRESS,0xFF,cut?2048U:4096U);
    return cut?BSP_NOR_IO:BSP_NOR_OK;
}
BSP_NOR_Status BSP_NOR_Program(uint32_t address,const void *data,uint32_t length)
{
    if(!InRange(address,length) || !length || length>256U || (address&255U)+length>256U || !BSP_NOR_CanWriteStorage()){g_mock_error=3;return BSP_NOR_ARGUMENT;}
    uint32_t cut=++operations==cut_at,count=cut?length/2U:length;
    uint8_t *dest=media+address-STORAGE_NVM_ADDRESS;const uint8_t *src=data;
    for(uint32_t i=0;i<count;++i){if((dest[i]&src[i])!=src[i]){g_mock_error=4;return BSP_NOR_IO;}dest[i]&=src[i];}
    return cut?BSP_NOR_IO:BSP_NOR_OK;
}
static FRESULT Reboot(void)
{
    memset((void*)&g_storage_service,0,sizeof(g_storage_service));owner=NULL;
    memset(nvm_cache,0,sizeof(nvm_cache));g_storage_backup.transport_verified=1U;
    return StorageService_Init();
}
int StorageJournal_TestMain(void)
{
    /* The exact production address guard is extracted by the runner. Adjacent
     * boundaries, page wrap and UINT32 overflow must never admit BL staging. */
    CHECK(WriteRangeAllowed(0U,0U,1U,0U));
    CHECK(WriteRangeAllowed(0U,0x07F7FFFFU,1U,0U));
    CHECK(!WriteRangeAllowed(0U,0x07F7FFFFU,2U,0U));
    CHECK(WriteRangeAllowed(0U,0x07F7F000U,0U,1U));
    CHECK(!WriteRangeAllowed(0U,0x07F80000U,1U,0U));
    CHECK(!WriteRangeAllowed(0U,0x07F80000U,0U,1U));
    CHECK(!WriteRangeAllowed(0U,0x07F90000U,1U,0U));
    CHECK(!WriteRangeAllowed(1U,0x07F8FFFFU,1U,0U));
    CHECK(!WriteRangeAllowed(1U,0x07F80000U,0U,1U));
    CHECK(WriteRangeAllowed(1U,0x07F90000U,0U,1U));
    CHECK(WriteRangeAllowed(1U,0x07FFFFFFU,1U,0U));
    CHECK(!WriteRangeAllowed(1U,0x07FFFFFFU,2U,0U));
    CHECK(WriteRangeAllowed(1U,0x07FFF000U,0U,1U));
    CHECK(!WriteRangeAllowed(1U,0x08000000U,1U,0U));
    CHECK(!WriteRangeAllowed(0U,0xFFFFFFFFU,0xFFFFFFFFU,0U));
    CHECK(!WriteRangeAllowed(0U,255U,2U,0U));
    CHECK(!WriteRangeAllowed(0U,4095U,0U,1U));
    CHECK(!WriteRangeAllowed(0U,0U,0U,0U));
    memset(media,0xFF,sizeof(media));unlocked=0;CHECK(Reboot()==FR_NO_FILESYSTEM);
    uint32_t size=0;CHECK(StorageService_NVMGet(received,sizeof(received),&size)==FR_NO_FILE);
    payload[0]=0x45;CHECK(StorageService_NVMPut(payload,1)==FR_WRITE_PROTECTED);CHECK(operations==0);
    unlocked=1;CHECK(StorageService_NVMPut(payload,1)==FR_OK);CHECK(g_storage_service.nvm_sequence==1);
    memcpy(baseline,media,sizeof(media));
    for(uint32_t i=0;i<sizeof(payload);++i)payload[i]=(uint8_t)(i*17U+0x21U);
    /* Twelve physical operations: erase, header, nine page fragments, commit.
     * Interrupt each at a partial byte prefix; the previous record must win. */
    for(uint32_t cut=1;cut<=13U;++cut){
        memcpy(media,baseline,sizeof(media));cut_at=0;operations=0;CHECK(Reboot()==FR_NO_FILESYSTEM);
        cut_at=cut;FRESULT result=StorageService_NVMPut(payload,sizeof(payload));
        CHECK(result==(cut<=12U?FR_DISK_ERR:FR_OK));cut_at=0;
        CHECK(Reboot()==FR_NO_FILESYSTEM);size=0;
        CHECK(StorageService_NVMGet(received,sizeof(received),&size)==FR_OK);
        if(cut<=12U){CHECK(size==1);CHECK(received[0]==0x45);CHECK(g_storage_service.nvm_sequence==1);}
        else {CHECK(size==sizeof(payload));CHECK(memcmp(payload,received,sizeof(payload))==0);CHECK(g_storage_service.nvm_sequence==2);}
    }
    /* Corrupted committed payload and sequence are rejected by the CRC, so
     * a valid older sector remains available instead of choosing bad data. */
    media[4096U+20U]^=1U;CHECK(Reboot()==FR_NO_FILESYSTEM);CHECK(g_storage_service.nvm_sequence==1);CHECK(g_storage_service.nvm_invalid==1);
    memcpy(media,baseline,sizeof(media));CHECK(Reboot()==FR_NO_FILESYSTEM);
    for(uint32_t i=0;i<19U;++i){payload[0]=(uint8_t)i;CHECK(StorageService_NVMPut(payload,1U)==FR_OK);}
    CHECK(g_storage_service.nvm_sequence==20U);CHECK(g_storage_service.nvm_slot==3U);
    CHECK(Reboot()==FR_NO_FILESYSTEM);CHECK(g_storage_service.nvm_sequence==20U);
    CHECK(StorageService_NVMGet(received,sizeof(received),&size)==FR_OK);CHECK(size==1U && received[0]==18U);
    media[3U*4096U+4092U]^=1U;CHECK(Reboot()==FR_NO_FILESYSTEM);CHECK(g_storage_service.nvm_sequence==19U);
    CHECK(StorageService_NVMPut(payload,2049U)==FR_INVALID_PARAMETER);
    unlocked=0U;runtime_permit=1U;
    CHECK(StorageService_NVMPut(payload,1U)==FR_OK);
    CHECK(StorageService_Format()==FR_WRITE_PROTECTED);CHECK(format_calls==0U);
    unlocked=1U;reset_in_format=1U;
    CHECK(StorageService_Format()==FR_WRITE_PROTECTED);CHECK(format_calls==1U);
    CHECK(!formatting && BSP_NOR_CanWriteStorage() && !unlocked);
    CHECK(StorageService_NVMPut(payload,1U)==FR_OK);
    CHECK(g_mock_error==0);return 0;
}
