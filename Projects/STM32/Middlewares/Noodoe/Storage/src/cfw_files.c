#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "RAM codec requires little-endian target"
#endif
#include "Noodoe_Crc32.h"
#include "Cfw_Files.h"
#include "StorageDisk.h"
#include "BSP_NOR.h"
#include "BSP_RAM.h"
#include "diskio.h"
#include <string.h>
/* This deliberately supports only the measured stock FAT12 geometry. All
 * directory/file chains (not only the three target chains) are traversed.
 * A FAT reference to FREE, a loop, duplicate owner or orphan fails closed. */
typedef struct  {
    uint8_t fat[8192],block[4096];
    uint16_t owner[4080],dirs[4080];
    uint32_t map[CFW_FILE_COUNT][32],present[CFW_FILE_COUNT],head,tail,current,sector,root,phase,error;
} FileAudit;
static FileAudit *a;
static const char names[CFW_FILE_COUNT][12]= {
    "CFWCFG  DAT","CFWRIDE DAT","CFWPIC  DAT","CFWA    DAT","CFWB    DAT","CFWBOOT DAT","CFWLOG  DAT","CFWTEXT DAT"
};
static const uint32_t sizes[CFW_FILE_COUNT]= {
    131072,262144,1048576,524288,524288,65536,262144,131072
};
static uint32_t U16(const uint8_t *p) {
    uint16_t value;memcpy(&value,p,2);return value;
}
uint32_t Cfw_Get32(const uint8_t *p) { uint32_t v;memcpy(&v,p,4);return v;}
void Cfw_Put32(uint8_t *p,uint32_t v) {
#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "CFW storage codec requires little-endian STM32"
#endif
    memcpy(p,&v,4);
}
uint32_t Cfw_Crc(const void *data,uint32_t bytes)
{return Noodoe_Crc32(data,bytes);}
static uint32_t Fat(uint32_t c) {
    uint32_t n=U16(a->fat+c+c/2);
    return c&1?n>>4:n&4095;
}
static uint32_t Address(uint32_t c) {
    return 0x9000U+(c-2)*32768U;
}
uint32_t CfwFiles_Size(uint32_t f) {
    return f<CFW_FILE_COUNT?sizes[f]:0;
}
uint32_t CfwFiles_Status(void) {
    return !a?CFW_MEMORY:a->error?a->error:a->phase==4?CFW_OK:CFW_PENDING;
}
uint32_t CfwFiles_Present(uint32_t f) {
    return a&&a->phase==4&&!a->error&&f<CFW_FILE_COUNT&&a->present[f];
}
void CfwFiles_Begin(void)
{
    BSP_NOR_ClearContainers();
    if(!a)a=BSP_RAM_AllocateNamed(BSP_RAM_CFW_AUDIT,sizeof(*a));
    if(a) {
        memset(a,0,sizeof(*a));
        a->phase=1;
    }
}
static uint32_t Fail(uint32_t e) {
    a->error=e;
    BSP_NOR_ClearContainers();
    return e;
}
/* Claims each cluster once. Directory chains own their own clusters; dot
 * references never claim again. Nonempty files must have exact chain length. */
static uint32_t Entry(const uint8_t *e)
{
    if(e[0]==0xE5||e[11]==15||e[11]&8||e[0]=='.')return 1;
    if(U16(e+20)||e[11]&0xC0)return 0;
    uint32_t first=U16(e+26),c=first,count=0,n=Cfw_Get32(e+28),f=CFW_FILE_COUNT;
    if(a->root)for(uint32_t i=0;i<CFW_FILE_COUNT;++i)if(!memcmp(e,names[i],11)) {
        if(a->present[i]||e[11]&16||n!=sizes[i])return 0;
        f=i;
        a->present[i]=1;
    }
    if(!first)return !n&&!(e[11]&16)&&f==CFW_FILE_COUNT;
    while(c<0xFF8) {
        if(c<2||c>=4080||a->owner[c])return 0;
        a->owner[c]=(uint16_t)first;
        if(f<CFW_FILE_COUNT) {
            if(count>=sizes[f]/32768||Address(c)>CFW_SAFE_END-32768)return 0;
            a->map[f][count]=Address(c);
        }
        ++count;
        c=Fat(c);
    }
    if(!(e[11]&16)&&count!=(n+32767U)/32768U)return 0;
    if(e[11]&16) {
        if(a->tail>=4080)return 0;
        a->dirs[a->tail++]=(uint16_t)first;
    }
    return 1;
}
static void NextDir(void)
{
    a->root=0;
    a->sector=0;
    if(a->head<a->tail)a->current=a->dirs[a->head++];
    else a->phase=3;
}
uint32_t CfwFiles_Process(void)
{
    if(!a)return CFW_MEMORY;
    if(a->error||a->phase==4)return CfwFiles_Status();
    if(a->phase==1) {
        if(disk_read(0,a->block,0,1)!=RES_OK)return Fail(CFW_IO);
        uint8_t *b=a->block;
        if(!StorageDisk_IsStock()||U16(b+11)!=4096||b[13]!=8||U16(b+14)!=1||b[16]!=2||
        U16(b+17)!=512||U16(b+22)!=2||Cfw_Get32(b+32)!=0x7F80||b[510]!=0x55||b[511]!=0xAA)return Fail(CFW_CORRUPT);
        if(disk_read(0,a->fat,1,2)!=RES_OK)return Fail(CFW_IO);
        for(uint32_t i=0;i<2;++i) {
            if(disk_read(0,a->block,3+i,1)!=RES_OK)return Fail(CFW_IO);
            if(memcmp(a->fat+i*4096,a->block,4096))return Fail(CFW_CORRUPT);
        }
        if(Fat(0)<0xFF0||Fat(1)<0xFF8)return Fail(CFW_CORRUPT);
        a->phase=2;
        a->root=1;
        a->sector=0;
        return CFW_PENDING;
    }
    if(a->phase==2) {
        uint32_t sector=a->root?5+a->sector:Address(a->current)/4096+a->sector;
        if(disk_read(0,a->block,sector,1)!=RES_OK)return Fail(CFW_IO);
        for(uint32_t i=0;i<4096;i+=32) {
            if(!a->block[i]) {
                NextDir();
                return CFW_PENDING;
            }
            if(!Entry(a->block+i))return Fail(CFW_CORRUPT);
        }
        if(++a->sector==(a->root?4U:8U)) {
            a->sector=0;
            if(a->root)NextDir();
            else {
                uint32_t next=Fat(a->current);
                if(next>=0xFF8)NextDir();
                else a->current=next;
            }
        }
        return CFW_PENDING;
    }
    for(uint32_t c=2;c<4080;++c) {
        uint32_t v=Fat(c);
        if(v&&v!=0xFF7&&!a->owner[c])return Fail(CFW_CORRUPT);
    }
    a->phase=4;
    return CFW_OK;
}
uint32_t CfwFiles_Grant(uint32_t f)
{
    return CfwFiles_Present(f)&&BSP_NOR_GrantContainer(f,a->map[f],sizes[f]/32768)==BSP_NOR_OK?CFW_OK:CFW_CORRUPT;
}
static uint32_t Range(uint32_t f,uint32_t off,uint32_t n)
{
    return CfwFiles_Present(f)&&n&&off<sizes[f]&&n<=sizes[f]-off;
}
uint32_t CfwFiles_Read(uint32_t f,uint32_t off,void *data,uint32_t n)
{
    if(!data||!Range(f,off,n))return CFW_ARGUMENT;
    uint8_t *p=data;
    /* Logical reads share the disk adapter, hence exactly one byte-pair swap.
         * Sector bounce permits unaligned payloads without touching FatFs state. */
    while(n) {
        uint32_t s=off&4095U,k=4096-s;
        if(k>n)k=n;
        uint32_t address=a->map[f][off/32768]+(off%32768&~4095U);
        if(disk_read(0,a->block,address/4096,1)!=RES_OK)return CFW_IO;
        memcpy(p,a->block+s,k);
        p+=k;
        off+=k;
        n-=k;
    }
    return CFW_OK;
}
uint32_t CfwFiles_Program(uint32_t f,uint32_t off,const void *data,uint32_t n)
{
    if(!data||!Range(f,off,n)||(off|n)&1U||n>256||(off&255U)+n>256)return CFW_ARGUMENT;
    return StorageDisk_ContainerProgram(f,a->map[f][off/32768]+off%32768,data,n);
}
uint32_t CfwFiles_Erase(uint32_t f,uint32_t off)
{
    return Range(f,off,4096)&&!(off&4095U)&&BSP_NOR_ContainerErase(f,a->map[f][off/32768]+off%32768)==BSP_NOR_OK?CFW_OK:CFW_IO;
}
