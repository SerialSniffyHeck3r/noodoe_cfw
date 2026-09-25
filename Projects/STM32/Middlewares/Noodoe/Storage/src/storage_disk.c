#include <string.h>
#include "ff.h"
#include "diskio.h"
#include "StorageBackup.h"
#include "BSP_NOR.h"
#include "StorageDisk.h"
#include "Cfw_Files.h"

/* The service owns all FatFs calls. A logical sector is exactly one erase
 * sector: no hidden writes outside a requested4KiB filesystem sector occur.
 * NOR media is not a wear-levelled block device; product asset writes should
 * be infrequent and NVM uses the separate rotating journal. */
#define SECTOR_BYTES 4096U
#define SECTOR_COUNT (STORAGE_FS_BYTES/SECTOR_BYTES)
static uint32_t detected,stock,swapped,import_scope,sectors=SECTOR_COUNT;
static uint32_t import_first,import_count;
static uint32_t Le16(const BYTE *p){uint16_t value;memcpy(&value,p,2);return value;}
static uint32_t Le32(const BYTE *p){return Le16(p)|(Le16(p+2)<<16);}
/* Validate geometry before enabling the legacy transform. The stock SPI16
 * layout stores each adjacent byte pair reversed relative to USB FAT data.
 * Detection never writes and a read error cannot grant a write capability. */
uint32_t StorageDisk_Detect(void)
{
    if(detected)return 1;
    BYTE b[512];
    if(!g_bsp_nor.ready||BSP_NOR_Read(0,b,sizeof(b))!=BSP_NOR_OK)return 0;
    for(uint32_t order=0;order<2U;++order){
        if(order)for(uint32_t i=0;i<sizeof(b);i+=2U){BYTE t=b[i];b[i]=b[i+1];b[i+1]=t;}
        uint32_t n=Le16(b+19);if(!n)n=Le32(b+32);
        if(b[510]==0x55&&b[511]==0xAA&&Le16(b+11)==4096U&&b[13]&&
           !(b[13]&(b[13]-1U))&&Le16(b+14)&&b[16]==2&&n&&n<=0x7F80U){
            stock=order||n>SECTOR_COUNT;swapped=order;sectors=n;break;
        }
    }
    detected=1;return 1;
}
uint32_t StorageDisk_IsStock(void){return stock;}
uint32_t StorageDisk_CanWrite(void){return detected&&(!stock||import_scope)&&BSP_NOR_CanWriteStorage();}
void StorageDisk_ImportScope(uint32_t active){import_scope=!!active;import_first=import_count=0;}
void StorageDisk_ImportRange(uint32_t first,uint32_t count){import_scope=2;import_first=first;import_count=count;}
uint32_t StorageDisk_ContainerProgram(uint32_t file,uint32_t address,const void *data,uint32_t bytes)
{
    if(!detected||!data||!bytes||bytes>256||(address|bytes)&1U)return CFW_ARGUMENT;
    uint8_t wire[256];const uint8_t *p=data;
    if(swapped){for(uint32_t i=0;i<bytes;i+=2){wire[i]=p[i+1];wire[i+1]=p[i];}p=wire;}
    return BSP_NOR_ContainerProgram(file,address,p,bytes)==BSP_NOR_OK?CFW_OK:CFW_IO;
}
DSTATUS disk_initialize(BYTE drive){if(drive||!StorageDisk_Detect())return STA_NOINIT;return disk_status(drive);}
DSTATUS disk_status(BYTE drive)
{
    if(drive || !g_bsp_nor.ready)return STA_NOINIT;
    return !detected?STA_NOINIT:StorageDisk_CanWrite()?0U:STA_PROTECT;
}
static uint32_t Range(BYTE drive,DWORD sector,UINT count)
{
    return drive==0U && count && sector<sectors && count<=sectors-sector;
}
DRESULT disk_read(BYTE drive,BYTE *buffer,DWORD sector,UINT count)
{
    if(!buffer || !Range(drive,sector,count))return RES_PARERR;
    if(!detected)return RES_NOTRDY;
    if(BSP_NOR_Read(sector*SECTOR_BYTES,buffer,count*SECTOR_BYTES)!=BSP_NOR_OK)return RES_ERROR;
    if(swapped)for(uint32_t i=0;i<count*SECTOR_BYTES;i+=2U){BYTE t=buffer[i];buffer[i]=buffer[i+1];buffer[i+1]=t;}
    return RES_OK;
}
DRESULT disk_write(BYTE drive,const BYTE *buffer,DWORD sector,UINT count)
{
    if(!buffer || !Range(drive,sector,count))return RES_PARERR;
    if(!StorageDisk_CanWrite())return RES_WRPRT;
    if(import_scope==2&&(sector<import_first||sector-import_first>=import_count||count>import_count-(sector-import_first)))return RES_WRPRT;
    for(UINT i=0;i<count;++i){uint32_t address=(sector+i)*SECTOR_BYTES;
        if(BSP_NOR_Erase4K(address)!=BSP_NOR_OK)return RES_ERROR;
        for(uint32_t offset=0;offset<SECTOR_BYTES;offset+=256U){
            /* Skip erased pages to reduce program time and wear; erase already
             * verified every byte. A later nonempty page still gets readback. */
            const BYTE *page=buffer+i*SECTOR_BYTES+offset;uint32_t nonempty=0U;
            for(uint32_t j=0;j<256U;++j)if(page[j]!=0xFFU){nonempty=1U;break;}
            if(nonempty){
                BYTE wire[256];const BYTE *source=page;
                if(swapped){for(uint32_t j=0;j<256U;j+=2U){wire[j]=page[j+1];wire[j+1]=page[j];}source=wire;}
                if(BSP_NOR_Program(address+offset,source,256U)!=BSP_NOR_OK)return RES_ERROR;
            }
        }
    }
    return RES_OK;
}
DRESULT disk_ioctl(BYTE drive,BYTE command,void *buffer)
{
    if(drive || !g_bsp_nor.ready)return RES_NOTRDY;
    switch(command){
        case CTRL_SYNC:return g_bsp_nor.busy?RES_NOTRDY:RES_OK;
        case GET_SECTOR_COUNT:if(!buffer)return RES_PARERR;*(DWORD*)buffer=sectors;return RES_OK;
        case GET_SECTOR_SIZE:if(!buffer)return RES_PARERR;*(WORD*)buffer=SECTOR_BYTES;return RES_OK;
        case GET_BLOCK_SIZE:if(!buffer)return RES_PARERR;*(DWORD*)buffer=1U;return RES_OK;
        default:return RES_PARERR;
    }
}
