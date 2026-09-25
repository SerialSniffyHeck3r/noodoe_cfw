#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "RAM codec requires little-endian target"
#endif
#include "Noodoe_Crc32.h"
#include "StorageService.h"
#include "StorageBackup.h"
#include "StorageDisk.h"
#include <stdio.h>
#include "BSP_NOR.h"
#include "cmsis_os2.h"
#include <string.h>

volatile StorageService_Diagnostics g_storage_service;
static FATFS filesystem;
static osMutexId_t owner;
/* Formatting is a provisioning tool in Integrated, not a Product operation.
 * Product still mounts/reads/imports stock FAT and performs normal allowed
 * file writes; it neither links the formatter nor reserves its4KiB scratch. */
#if !NOODOE_PRODUCT
static uint8_t format_work[4096] __attribute__((aligned(4)));
#endif
/* NVM scan/program staging never consumes a2KiB task stack allocation. */
static uint8_t nvm_cache[STORAGE_NVM_MAX_PAYLOAD];
static uint8_t nvm_work[STORAGE_NVM_MAX_PAYLOAD];
#define JOURNAL_MAGIC 0x4E564D31U
#define JOURNAL_COMMIT 0x434D5431U
#define JOURNAL_SECTORS 16U
#define JOURNAL_BYTES 4096U

/* Serialize filesystem objects and journal buffers at service level. USB IRQs
 * never call this API. Mutex creation occurs once before worker tasks use it. */
static FRESULT Lock(void)
{
    if(__get_IPSR() || __get_PRIMASK() || __get_BASEPRI())return FR_INVALID_OBJECT;
    if(!owner)return FR_NOT_READY;
    return osMutexAcquire(owner,5000U)==osOK?FR_OK:FR_TIMEOUT;
}
static FRESULT Done(FRESULT result)
{
    g_storage_service.last_result=(uint32_t)result;(void)osMutexRelease(owner);return result;
}
static uint32_t Get32(const uint8_t *p){ uint32_t v;memcpy(&v,p,4);return v;}
/* Preserve the explicit LE32 wire bytes without a byte-store loop at every
 * call site. memcpy also preserves the contract for unaligned RAM buffers. */
#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "This scalar codec requires a little-endian target"
#endif
static void Put32(uint8_t *p,uint32_t value){memcpy(p,&value,4);}
uint32_t StorageService_Crc32(const uint8_t *data,uint32_t length,uint32_t seed)
{return Noodoe_Crc32Feed(seed,data,length);}
/* CRC covers sequence+length and payload. The final commit word is programmed
 * only after all header/payload page programs were independently verified. */
static FRESULT ScanJournal(void)
{
    g_storage_service.nvm_valid=0U;g_storage_service.nvm_slot=0xFFFFFFFFU;
    g_storage_service.nvm_length=g_storage_service.nvm_sequence=0U;
    for(uint32_t slot=0;slot<JOURNAL_SECTORS;++slot){uint8_t header[20],commit[4];uint32_t address=STORAGE_NVM_ADDRESS+slot*JOURNAL_BYTES;
        if(BSP_NOR_Read(address,header,sizeof(header))!=BSP_NOR_OK || BSP_NOR_Read(address+JOURNAL_BYTES-4U,commit,4U)!=BSP_NOR_OK)return FR_DISK_ERR;
        if(Get32(commit)!=JOURNAL_COMMIT || Get32(header)!=JOURNAL_MAGIC)continue;
        uint32_t sequence=Get32(header+4),length=Get32(header+8);
        if(length>STORAGE_NVM_MAX_PAYLOAD || Get32(header+16)!=~length){++g_storage_service.nvm_invalid;continue;}
        if(length && BSP_NOR_Read(address+20U,nvm_work,length)!=BSP_NOR_OK)return FR_DISK_ERR;
        uint32_t crc=StorageService_Crc32(nvm_work,length,StorageService_Crc32(header+4,8U,0xFFFFFFFFU))^0xFFFFFFFFU;
        if(crc!=Get32(header+12)){++g_storage_service.nvm_invalid;continue;}
        if(!g_storage_service.nvm_valid || (int32_t)(sequence-g_storage_service.nvm_sequence)>0){
            memcpy(nvm_cache,nvm_work,length);g_storage_service.nvm_valid=1U;
            g_storage_service.nvm_sequence=sequence;g_storage_service.nvm_length=length;g_storage_service.nvm_slot=slot;
        }
    }
    return FR_OK;
}
FRESULT StorageService_Init(void)
{
    if(__get_IPSR() || osKernelGetState()!=osKernelRunning || !g_storage_backup.transport_verified)return FR_NOT_READY;
    if(!owner)owner=osMutexNew(NULL);
    FRESULT result=Lock();if(result!=FR_OK)return result;
    if(g_storage_service.initialized)return Done(FR_OK);
    g_storage_service.magic=0x53544F31U;g_storage_service.version=1U;
    if(!StorageDisk_Detect())return Done(FR_DISK_ERR);
    /* Stock FAT extends across the proposed CFW journal. Do not interpret
     * those file bytes as settings, and never permit journal writes there. */
    result=StorageDisk_IsStock()?FR_OK:ScanJournal();
    if(result==FR_OK){g_storage_service.initialized=1U;result=f_mount(&filesystem,"0:",1U);g_storage_service.mounted=result==FR_OK;}
    return Done(result);
}
FRESULT StorageService_Mount(void)
{
    FRESULT result=Lock();if(result!=FR_OK)return result;
    result=f_mount(&filesystem,"0:",1U);g_storage_service.mounted=result==FR_OK;return Done(result);
}
FRESULT StorageService_Format(void)
{
#if NOODOE_PRODUCT
    return FR_WRITE_PROTECTED;
#else
    FRESULT result=Lock();if(result!=FR_OK)return result;
    /* A persistent runtime permit must never satisfy the destructive format
     * gate. BeginFormat also makes every underlying page/erase host-only until
     * EndFormat, so a USB reset during f_mkfs revokes subsequent writes. */
    if(StorageDisk_IsStock())return Done(FR_WRITE_PROTECTED);
    BSP_NOR_Status gate=BSP_NOR_BeginFormat();
    if(gate!=BSP_NOR_OK)return Done(gate==BSP_NOR_BUSY?FR_TIMEOUT:FR_WRITE_PROTECTED);
    result=f_mount(NULL,"0:",0U);g_storage_service.mounted=0U;
    if(result==FR_OK)result=f_mkfs("0:",FM_FAT|FM_SFD,4096U,format_work,sizeof(format_work));
    if(result==FR_OK){++g_storage_service.formats;result=f_mount(&filesystem,"0:",1U);g_storage_service.mounted=result==FR_OK;}
    BSP_NOR_EndFormat();return Done(result);
#endif
}
FRESULT StorageService_ReadFile(const char *path,uint32_t offset,void *destination,uint32_t capacity,uint32_t *received)
{
    if(!path || !destination || !received)return FR_INVALID_PARAMETER;
    *received=0U;FRESULT result=Lock();if(result!=FR_OK)return result;
    FIL file;result=f_open(&file,path,FA_READ);
    if(result==FR_OK){result=f_lseek(&file,offset);UINT count=0U;
        if(result==FR_OK)result=f_read(&file,destination,capacity,&count);
        *received=count;FRESULT close=f_close(&file);if(result==FR_OK)result=close;
        if(result==FR_OK)++g_storage_service.file_reads;
    }
    return Done(result);
}
FRESULT StorageService_WriteFile(const char *path,const void *source,uint32_t length)
{
    if(!path || (!source && length))return FR_INVALID_PARAMETER;
    FRESULT result=Lock();if(result!=FR_OK)return result;
    if(!StorageDisk_CanWrite())return Done(FR_WRITE_PROTECTED);
    FIL file;result=f_open(&file,path,FA_CREATE_ALWAYS|FA_WRITE);
    if(result==FR_OK){UINT count=0U;result=f_write(&file,source,length,&count);
        if(result==FR_OK && count!=length)result=FR_DISK_ERR;
        if(result==FR_OK)result=f_sync(&file);
        FRESULT close=f_close(&file);if(result==FR_OK)result=close;
        if(result==FR_OK)++g_storage_service.file_writes;
    }
    return Done(result);
}
FRESULT StorageService_Stat(const char *path,uint32_t *length)
{
    if(!path || !length)return FR_INVALID_PARAMETER;
    FRESULT result=Lock();if(result!=FR_OK)return result;
    FILINFO info;result=f_stat(path,&info);if(result==FR_OK)*length=(uint32_t)info.fsize;return Done(result);
}
FRESULT StorageService_Mkdir(const char *path)
{
    if(!path)return FR_INVALID_PARAMETER;
    FRESULT result=Lock();if(result!=FR_OK)return result;
    return Done(StorageDisk_CanWrite()?f_mkdir(path):FR_WRITE_PROTECTED);
}
FRESULT StorageService_Remove(const char *path)
{
    if(!path)return FR_INVALID_PARAMETER;
    FRESULT result=Lock();if(result!=FR_OK)return result;
    return Done(StorageDisk_CanWrite()?f_unlink(path):FR_WRITE_PROTECTED);
}
FRESULT StorageService_NVMGet(void *destination,uint32_t capacity,uint32_t *received)
{
    if(!destination || !received)return FR_INVALID_PARAMETER;
    *received=0U;
    FRESULT result=Lock();if(result!=FR_OK)return result;
    if(!g_storage_service.nvm_valid)return Done(FR_NO_FILE);
    if(capacity<g_storage_service.nvm_length)return Done(FR_INVALID_PARAMETER);
    *received=g_storage_service.nvm_length;memcpy(destination,nvm_cache,*received);return Done(FR_OK);
}
FRESULT StorageService_NVMPut(const void *source,uint32_t length)
{
    if((!source && length) || length>STORAGE_NVM_MAX_PAYLOAD)return FR_INVALID_PARAMETER;
    FRESULT result=Lock();if(result!=FR_OK)return result;
    if(!StorageDisk_CanWrite())return Done(FR_WRITE_PROTECTED);
    uint32_t slot=g_storage_service.nvm_valid?(g_storage_service.nvm_slot+1U)%JOURNAL_SECTORS:0U;
    uint32_t address=STORAGE_NVM_ADDRESS+slot*JOURNAL_BYTES,sequence=g_storage_service.nvm_valid?g_storage_service.nvm_sequence+1U:1U;
    uint8_t header[20];Put32(header,JOURNAL_MAGIC);Put32(header+4,sequence);Put32(header+8,length);Put32(header+16,~length);
    uint32_t crc=StorageService_Crc32(source,length,StorageService_Crc32(header+4,8U,0xFFFFFFFFU))^0xFFFFFFFFU;Put32(header+12,crc);
    if(BSP_NOR_Erase4K(address)!=BSP_NOR_OK || BSP_NOR_Program(address,header,sizeof(header))!=BSP_NOR_OK)return Done(FR_DISK_ERR);
    /* The first payload starts at20, so split at the physical256-byte page
     * boundary rather than blindly using256-byte chunks from payload offset0. */
    const uint8_t *data=source;uint32_t offset=0U;
    while(offset<length){uint32_t at=address+20U+offset,count=256U-(at&255U);if(count>length-offset)count=length-offset;
        if(BSP_NOR_Program(at,data+offset,count)!=BSP_NOR_OK)return Done(FR_DISK_ERR);
        offset+=count;
    }
    uint8_t commit[4];Put32(commit,JOURNAL_COMMIT);
    if(BSP_NOR_Program(address+JOURNAL_BYTES-4U,commit,4U)!=BSP_NOR_OK)return Done(FR_DISK_ERR);
    if(length)memcpy(nvm_cache,source,length);
    g_storage_service.nvm_valid=1U;g_storage_service.nvm_slot=slot;
    g_storage_service.nvm_sequence=sequence;g_storage_service.nvm_length=length;++g_storage_service.nvm_commits;
    return Done(FR_OK);
}

FRESULT StorageService_FindAlbumPhoto(uint32_t slot,char *path,uint32_t capacity,uint32_t *length)
{
    if(slot>=3U||!path||capacity<32U)return FR_INVALID_PARAMETER;
    path[0]=0;FRESULT r=Lock();if(r!=FR_OK)return r;
    char folder[]="0:/album/0";folder[9]=(char)('0'+slot);
    DIR dir;FILINFO info;r=f_opendir(&dir,folder);uint32_t found=0;
    if(r==FR_OK){
        while((r=f_readdir(&dir,&info))==FR_OK&&info.fname[0]){
            const char *ext=strrchr(info.fname,'.');
            if(!(info.fattrib&AM_DIR)&&ext&&(!strcmp(ext,".JPG")||!strcmp(ext,".jpg"))){
                ++found;if(found==1U){memcpy(path,folder,10);path[10]='/';memcpy(path+11,info.fname,sizeof(info.fname));if(length)*length=(uint32_t)info.fsize;}
            }
        }
        FRESULT close=f_closedir(&dir);if(r==FR_OK)r=close;
        if(r==FR_OK&&found!=1U)r=found?FR_INVALID_NAME:FR_NO_FILE;
    }
    if(r!=FR_OK)path[0]=0;
    return Done(r);
}

static FRESULT CreatePhoto(const char *path,const uint8_t *jpeg,uint32_t length,uint32_t token)
{
    if(!path||!jpeg||length<4U||length>128U*1024U||token!=0x42414B32U)return FR_INVALID_PARAMETER;
    FRESULT r=Lock();if(r!=FR_OK)return r;
    if(!StorageDisk_IsStock()||!g_storage_backup.transport_verified)return Done(FR_WRITE_PROTECTED);
    if(BSP_NOR_UnlockStorage(token)!=BSP_NOR_OK)return Done(FR_WRITE_PROTECTED);
    StorageDisk_ImportScope(1);
    FIL f;r=f_open(&f,path,FA_CREATE_NEW|FA_WRITE|FA_READ);
    if(r==FR_OK){
        UINT n=0;r=f_write(&f,jpeg,length,&n);if(r==FR_OK&&n!=length)r=FR_DISK_ERR;
        if(r==FR_OK)r=f_sync(&f);
        if(r==FR_OK)r=f_lseek(&f,0);
        uint8_t verify[256];
        for(uint32_t at=0;r==FR_OK&&at<length;){
            UINT count=length-at;if(count>sizeof(verify))count=sizeof(verify);
            r=f_read(&f,verify,count,&n);
            if(r==FR_OK&&(n!=count||memcmp(verify,jpeg+at,count)))r=FR_DISK_ERR;
            at+=count;
        }
        FRESULT close=f_close(&f);if(r==FR_OK)r=close;
        if(r==FR_OK)++g_storage_service.file_writes;
    }
    StorageDisk_ImportScope(0);BSP_NOR_LockStorage();return Done(r);
}

/* Separate immutable overrides preserve donor photos. Invalid existing files
 * are errors, not silently treated as missing. Only an absent override falls
 * back to the original album. Storage owner serializes every FatFs operation. */
FRESULT StorageService_FindWallpaper(uint32_t slot,char *path,uint32_t capacity,uint32_t *length)
{
    if(slot>=3U||!path||capacity<32U)return FR_INVALID_PARAMETER;
    strcpy(path,"0:/WALL0.JPG");path[7]=(char)('0'+slot);
    FRESULT r=Lock();if(r!=FR_OK)return r;
    FILINFO info;r=f_stat(path,&info);
    if(r==FR_OK){if(info.fattrib&AM_DIR)r=FR_INVALID_NAME;else if(length)*length=info.fsize;}
    Done(r);
    return r==FR_NO_FILE?StorageService_FindAlbumPhoto(slot,path,capacity,length):r;
}
FRESULT StorageService_CreateAlbumPhoto(uint32_t slot,const uint8_t *jpeg,uint32_t length,uint32_t token)
{
    if(slot>=3U)return FR_INVALID_PARAMETER;
    char path[]="0:/album/0/CFW.JPG";path[9]=(char)('0'+slot);
    return CreatePhoto(path,jpeg,length,token);
}
/* Provision once, then reboot to activate. Never truncate/unlink/replace a
 * file, so an installation cannot overwrite a currently displayed photo. */
FRESULT StorageService_CreateWallpaper(uint32_t slot,const uint8_t *jpeg,uint32_t length,uint32_t token)
{
    if(slot>=3U)return FR_INVALID_PARAMETER;
    char path[]="0:/WALL0.JPG";path[7]=(char)('0'+slot);
    return CreatePhoto(path,jpeg,length,token);
}
