#include "PhotoService.h"
#include "StorageService.h"
#include "StorageDisk.h"
#include "StorageBackup.h"
#include "BSP_NOR.h"
#include "BSP_RAM.h"
#include "diskio.h"
#include <string.h>
volatile BSP_NOR_Diagnostics g_bsp_nor={.ready=1};
volatile StorageBackup_Diagnostics g_storage_backup={.transport_verified=1};
volatile uint32_t assertions,writes,input_lengths[2];
uint32_t allocated;
PhotoImage result_images[3] __attribute__((used,externally_visible));
#define CHECK(x) do{++assertions;if(!(x))return __LINE__;}while(0)
BSP_NOR_Status BSP_NOR_Read(uint32_t a,void *d,uint32_t n){if(a>0x8000000||n>0x8000000-a)return BSP_NOR_ARGUMENT;memcpy(d,(void*)(0x90000000U+a),n);return BSP_NOR_OK;}
uint32_t BSP_NOR_CanWriteStorage(void){return 1;} /* Even an unlocked host cannot alter stock FAT/NVM. */
BSP_NOR_Status BSP_NOR_Program(uint32_t a,const void *d,uint32_t n){
 if(a+n>0x7F80000U)return BSP_NOR_ARGUMENT;
 uint8_t *p=(uint8_t*)(0x90000000U+a);const uint8_t *v=d;
 for(uint32_t i=0;i<n;++i){if((p[i]&v[i])!=v[i])return BSP_NOR_VERIFY;p[i]=v[i];}
 ++writes;return BSP_NOR_OK;}
BSP_NOR_Status BSP_NOR_UnlockStorage(uint32_t t){return t==0x42414B32U?BSP_NOR_OK:BSP_NOR_ARGUMENT;}
void BSP_NOR_LockStorage(void){}
BSP_NOR_Status BSP_NOR_Erase4K(uint32_t a){if(a+4096>0x7F80000U)return BSP_NOR_ARGUMENT;memset((void*)(0x90000000U+a),255,4096);++writes;return BSP_NOR_OK;}
BSP_NOR_Status BSP_NOR_BeginFormat(void){++writes;return BSP_NOR_OK;}
void BSP_NOR_EndFormat(void){}
void *BSP_RAM_Allocate(size_t n){n=(n+31U)&~31U;if(n>0x4000000U-allocated)return 0;void *p=(void*)(0xC0000000U+allocated);allocated+=n;return p;}
static uint32_t Crc(const uint8_t *p,uint32_t n){uint32_t c=0xFFFFFFFFU;for(uint32_t i=0;i<n;++i){c^=p[i];for(uint32_t j=0;j<8;++j)c=(c>>1)^((c&1)?0xEDB88320U:0);}return c^0xFFFFFFFFU;}
uint32_t TestMain(void)
{
    CHECK(StorageService_Init()==FR_OK);CHECK(StorageDisk_IsStock());
    CHECK(g_storage_service.mounted&&!g_storage_service.nvm_valid);
    uint8_t byte=0;CHECK(StorageService_WriteFile("0:/BAD.BIN",&byte,1)==FR_WRITE_PROTECTED);
    CHECK(StorageService_NVMPut(&byte,1)==FR_WRITE_PROTECTED);
    CHECK(StorageService_Format()==FR_WRITE_PROTECTED);CHECK(!writes);
    CHECK(!PhotoService_RequestLoad(3));
    for(uint32_t i=0;i<3;++i)CHECK(PhotoService_RequestLoad(i));
    for(uint32_t i=0;i<1000U;++i)PhotoService_Process();
    CHECK(g_photos.failed_mask==3U);CHECK(g_photos.ready_mask==4U);CHECK(!writes);
    /* A bad CRC cannot cause a single physical program/erase. */
    CHECK(g_photo_import.buffer&&g_photo_import.capacity>=input_lengths[0]);
    memcpy((void*)g_photo_import.buffer,(void*)0xA0000000U,input_lengths[0]);
    g_photo_import.slot=0;g_photo_import.length=input_lengths[0];g_photo_import.crc32=0;
    g_photo_import.arm=0x42414B32U;g_photo_import.sequence=1;PhotoService_Process();
    CHECK(g_photo_import.ack==1&&g_photo_import.result==0x601U&&!writes);
    for(uint32_t i=0;i<2;++i){
        PhotoService_Process();CHECK(g_photo_import.buffer);
        const uint8_t *source=(void*)(0xA0000000U+i*0x20000U);
        memcpy((void*)g_photo_import.buffer,source,input_lengths[i]);
        g_photo_import.slot=i;g_photo_import.length=input_lengths[i];g_photo_import.crc32=Crc(source,input_lengths[i]);
        g_photo_import.arm=0x42414B32U;g_photo_import.sequence=i+2U;
        for(uint32_t n=0;n<1000U&&g_photo_import.ack!=i+2U;++n)PhotoService_Process();
        CHECK(g_photo_import.ack==i+2U&&!g_photo_import.result);
    }
    CHECK(!g_photos.failed_mask&&g_photos.ready_mask==7U&&writes);
    CHECK(StorageService_NVMPut(&byte,1)==FR_WRITE_PROTECTED);
    for(uint32_t i=0;i<3;++i){CHECK(PhotoService_Get(i,&result_images[i]));CHECK(result_images[i].width==480&&result_images[i].height==480);}
    uint32_t before=writes;
    CHECK(StorageService_CreateAlbumPhoto(0,(void*)0xA0000000U,input_lengths[0],0x42414B32U)==FR_EXIST);
    CHECK(before==writes);return 0;
}
uint32_t TestReload(void)
{
    CHECK(StorageService_Init()==FR_OK);CHECK(StorageDisk_IsStock());
    for(uint32_t i=0;i<3;++i)CHECK(PhotoService_RequestLoad(i));
    for(uint32_t i=0;i<1000U;++i)PhotoService_Process();
    CHECK(g_photos.ready_mask==7U&&!g_photos.failed_mask&&!writes);
    for(uint32_t i=0;i<3;++i)CHECK(PhotoService_Get(i,&result_images[i]));
    return 0;
}
