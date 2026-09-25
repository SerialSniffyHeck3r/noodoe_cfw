#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "RAM codec requires little-endian target"
#endif
#include "ResourceStore.h"
#include "ResourceRepair.h"
#include "Resources.h"
#include "StorageService.h"
#include "StorageDisk.h"
#include "StorageBackup.h"
#include "BSP_NOR.h"
#include "BSP_RAM.h"
#include "BSP_Power.h"
#include "diskio.h"
#include "Update_Service.h"
#if NOODOE_PRODUCT
#include "BootStore.h"
#endif
#include <string.h>
/* Provisioning is deliberately limited to the observed stock FAT12 geometry.
 * FAT edits preserve packed neighbour entries. A host traversal additionally
 * rejects cross-links/free clusters referenced by directory entries before
 * it grants BAK2. Unknown geometries fail closed, never auto-format. */
#define SLOT 524288U
#define SECTOR 4096U
#define ROOT_BYTES 16384U
#define COMMIT_OFFSET 4092U
volatile ResourceStoreMailbox g_resource_install;
static uint8_t *stage,*fat,*root,*work;
static uint32_t phase,seq,command,slot,first,entry,position,start_sector;
static UpdateSha256 sha;
static uint8_t target_hash[32];
static uint8_t compatibility_hash[32];
static uint32_t compatibility,compat_slot,compat_position,compat_total;
static UpdateSha256 compat_sha;
static uint32_t U16(const uint8_t *p){uint16_t value;memcpy(&value,p,2);return value;}
static uint32_t U32(const uint8_t *p){ uint32_t v;memcpy(&v,p,4);return v;}
static void P16(uint8_t *p,uint32_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static void P32(uint8_t *p,uint32_t v){P16(p,v);P16(p+2,v>>16);}
static uint32_t FAT(uint32_t c){uint32_t n=U16(fat+c+c/2);return (c&1)?n>>4:n&0xFFF;}
static void SetFAT(uint32_t c,uint32_t next)
{uint8_t *p=fat+c+c/2;uint32_t old=U16(p);P16(p,c&1?(old&15)|(next<<4):(old&0xF000)|next);}
uint32_t ResourceStore_Busy(void){return phase!=0||compatibility==1||compatibility==2;}
uint32_t ResourceStore_Compatible(const uint8_t required[32])
{
    if(!required||phase||ResourceStore_TransferActive())return 0;
    if(Resources_GetStatus()==RESOURCES_READY&&!memcmp(required,g_resource_requirement.sha256,32))return 1;
    if(memcmp(required,compatibility_hash,32)||!compatibility){
        if(compatibility==1||compatibility==2||g_resource_install.sequence!=g_resource_install.ack)return 0;
        memcpy(compatibility_hash,required,32);compat_slot=0;compatibility=1;
    }
    return compatibility==3;
}
/* Uses the idle import arena, never the immutable live resource arena. Normal
 * APP verification polls this before accepting COMMIT; no FAT/NOR writes. */
static void CompatibilityStep(void)
{
    uint32_t got=0;
    if(compatibility==1){
        uint32_t length=0;
        if(StorageService_Stat("0:/NOODOE.RSC",&length)!=FR_OK||length!=2*SLOT||
           StorageService_ReadFile("0:/NOODOE.RSC",compat_slot*SLOT,stage,4096,&got)!=FR_OK||got!=4096||
           Resources_CheckHeader(stage,compatibility_hash))goto bad;
        compat_total=U32(stage+8);compat_position=0;UpdateSha256_Init(&compat_sha);
        UpdateSha256_Feed(&compat_sha,stage+48,U32(stage+12)*16);compatibility=2;return;
    }
    if(compatibility==2){
        uint32_t n=compat_total-compat_position;if(n>4096)n=4096;
        if(StorageService_ReadFile("0:/NOODOE.RSC",compat_slot*SLOT+4096+compat_position,work,n,&got)!=FR_OK||got!=n)goto bad;
        UpdateSha256_Feed(&compat_sha,work,n);compat_position+=n;if(compat_position<compat_total)return;
        uint8_t hash[32];UpdateSha256_Final(&compat_sha,hash);if(memcmp(hash,compatibility_hash,32))goto bad;
        compatibility=3;return;
    }
    return;
bad:
    if(!compat_slot){compat_slot=1;compatibility=1;}else compatibility=4;
}
static void Done(uint32_t error)
{
    StorageDisk_ImportScope(0);BSP_NOR_LockStorage();
    if(!error)(void)StorageService_Mount();
    g_resource_install.error=error;g_resource_install.state=error?3:2;
    phase=0;__DMB();g_resource_install.ack=seq;
}
/* Each invocation writes only one4KiB sector, with an independent logical
 * readback. The disk adapter performs physical pair swapping exactly once. */
static uint32_t Write(uint32_t sector,const uint8_t *data)
{
    if(sector>=STORAGE_FS_BYTES/SECTOR)return 0;
    if(disk_write(0,data,sector,1)!=RES_OK||disk_read(0,work,sector,1)!=RES_OK)return 0;
    return !memcmp(data,work,SECTOR);
}
/* Read-only preflight before the first erase. Existing-container updates must
 * match its original32-cluster chain and size; they cannot touch FAT/root. */
static uint32_t Layout(void)
{
    if(disk_read(0,work,0,1)!=RES_OK)return 1;
    if(U16(work+11)!=4096||work[13]!=8||U16(work+14)!=1||work[16]!=2||U16(work+17)!=512||U16(work+22)!=2||U32(work+32)!=0x7F80)return 2;
    if(disk_read(0,fat,1,2)!=RES_OK||disk_read(0,root,3,2)!=RES_OK||memcmp(fat,root,8192))return 3;
    if(disk_read(0,root,5,4)!=RES_OK)return 4;
    uint32_t found=UINT32_MAX,end_entry=512;
    for(uint32_t i=0;i<512;++i){uint8_t *r=root+i*32;
        if(!r[0]){end_entry=i;break;}
        if(r[0]!=0xE5&&r[11]!=0x0F&&!memcmp(r,"NOODOE  RSC",11)){if(found!=UINT32_MAX)return 5;found=i;}
    }
    if(command==1){
        if(found!=UINT32_MAX||entry>=512||entry>end_entry||(root[entry*32]!=0&&root[entry*32]!=0xE5))return 6;
        if(slot)return 7;
    }else{
        if(found==UINT32_MAX)return 8;
        entry=found;
        uint8_t *r=root+entry*32;
        if(r[11]&0x18||U32(r+28)!=2*SLOT||U16(r+20))return 9;
        first=U16(r+26);
        if(Resources_GetStatus()==RESOURCES_READY&&slot==g_resources.slot)return 10;
    }
    if(first<2||first>4078-32||slot>1)return 11;
    start_sector=9+(first-2)*8;
    if(start_sector+256>STORAGE_FS_BYTES/SECTOR)return 12;
    for(uint32_t i=0;i<32;++i){uint32_t n=FAT(first+i);
        if(command==1?n!=0:(i==31?n<0xFF8:n!=first+i+1))return 13;
    }
    if(command==1){
        for(uint32_t i=0;i<32;++i)SetFAT(first+i,i==31?0xFFF:first+i+1);
        uint8_t *r=root+entry*32;memset(r,0,32);memcpy(r,"NOODOE  RSC",11);r[11]=0x20;
        P16(r+26,first);P32(r+28,2*SLOT);
    }
    g_resource_install.container_sector=start_sector;return 0;
}
#include "resource_transfer.inc"
void ResourceStore_Process(void)
{
    if(!stage){stage=BSP_RAM_AllocateNamed(BSP_RAM_RESOURCE_IMPORT,SLOT+8192+ROOT_BYTES+SECTOR);
        if(!stage)return;
        fat=stage+SLOT;root=fat+8192;work=root+ROOT_BYTES;
        g_resource_install.magic=0x52534931;g_resource_install.version=1;g_resource_install.buffer=(uint32_t)stage;g_resource_install.capacity=SLOT;}
    if(TransferStep())return;
    if(compatibility==1||compatibility==2){g_resource_install.state=4;CompatibilityStep();return;}
    if(g_resource_install.state==4)g_resource_install.state=0;
    if(!phase){
        uint32_t request=g_resource_install.sequence;if(!request||request==g_resource_install.ack)return;
        __DMB();command=g_resource_install.command;slot=g_resource_install.slot;first=g_resource_install.first_cluster;entry=g_resource_install.root_index;
        seq=request;__DMB();if(request!=g_resource_install.sequence)return;
        g_resource_install.state=1;g_resource_install.error=0;g_resource_install.progress=0;
        compatibility=0;
#if NOODOE_PRODUCT
        /* Both resource generations remain pinned throughout trial/reset.
         * No third package may erase the previous confirmed APP's assets. */
        GateJournalRecord boot;
        if(!BootStore_GetBootInfo(&boot)||!g_boot_store.confirmed||
           (boot.flags&(GATE_F_TRIAL|GATE_F_RESET_PENDING))){Done(23);return;}
#endif
        if((command!=1&&command!=2&&command!=3)||g_resource_install.token!=0x42414B32||
           (!(rx_state==RX_WRITE&&command==2)&&(!g_bsp_power.ign_valid||!g_bsp_power.ign_on))||!StorageDisk_IsStock()){Done(1);return;}
        if(command==3){uint32_t error=ResourceRepair_Begin(stage,SLOT,work);if(error)Done(error);else phase=8;return;}
        if(Resources_CheckHeader(stage,stage+16)){Done(2);return;}
        memcpy(target_hash,stage+16,32);UpdateSha256_Init(&sha);UpdateSha256_Feed(&sha,stage+48,U32(stage+12)*16);
        position=0;phase=1;return;
    }
    /* The prepared input must remain immutable through ack. Verify before
     * unlock, and re-verify the NOR stream before publishing its commit. */
    if(phase==8){uint32_t result=ResourceRepair_Process();if(result)Done(result==1?0:result);return;}
    if(phase==1){
        uint32_t total=U32(stage+8),n=total-position;if(n>4096)n=4096;
        UpdateSha256_Feed(&sha,stage+4096+position,n);position+=n;
        if(position<total)return;
        uint8_t hash[32];UpdateSha256_Final(&sha,hash);if(memcmp(hash,target_hash,32)){Done(3);return;}
        uint32_t result=Layout();if(result){Done(100+result);return;}
        /* An update never destroys the only matching known-good slot. Validate
         * the retained slot independently, before any inactive-slot erase. */
        if(command==2){position=0;phase=6;return;}
        phase=2;position=0;return;
    }
    if(phase==6){
        if(disk_read(0,work,start_sector+(1-slot)*128,1)!=RES_OK||Resources_CheckHeader(work,g_resource_requirement.sha256)){Done(20);return;}
        memcpy(fat,work,4096);UpdateSha256_Init(&sha);UpdateSha256_Feed(&sha,fat+48,U32(fat+12)*16);position=0;phase=7;return;
    }
    if(phase==7){
        uint32_t total=U32(fat+8),n=total-position;if(n>4096)n=4096;
        if(disk_read(0,work,start_sector+(1-slot)*128+1+position/4096,1)!=RES_OK){Done(21);return;}
        UpdateSha256_Feed(&sha,work,n);position+=n;if(position<total)return;
        uint8_t hash[32];UpdateSha256_Final(&sha,hash);if(memcmp(hash,fat+16,32)){Done(22);return;}
        phase=2;position=0;return;
    }
    if(phase==2){
        if(BSP_NOR_UnlockStorage(0x42414B32)!=BSP_NOR_OK){Done(4);return;}
        if(command==2)StorageDisk_ImportRange(start_sector+slot*128,128);
        else StorageDisk_ImportScope(1);
        uint32_t sector=start_sector+slot*128+position/4096;
        if(position==0){P32(stage+4092,UINT32_MAX);}
        const uint8_t *data=stage+(position<SLOT?position:0);
        if(command==1&&position>=SLOT){memset(work,0xFF,4096);data=work;sector=start_sector+position/4096;}
        /* Blank second slot needs only erase+readback; avoid a scratch alias in Write. */
        uint32_t ok;
        if(data==work){ok=disk_write(0,work,sector,1)==RES_OK&&disk_read(0,work,sector,1)==RES_OK;
            for(uint32_t i=0;ok&&i<4096;++i)if(work[i]!=0xFF)ok=0;}
        else ok=Write(sector,data);
        if(!ok){Done(5);return;}
        position+=4096;g_resource_install.progress=position;
        if(position<(command==1?2*SLOT:SLOT))return;
        position=0;phase=3;UpdateSha256_Init(&sha);UpdateSha256_Feed(&sha,stage+48,U32(stage+12)*16);return;
    }
    if(phase==3){
        uint32_t total=U32(stage+8),n=total-position;if(n>4096)n=4096;
        if(disk_read(0,work,start_sector+slot*128+1+position/4096,1)!=RES_OK){Done(6);return;}
        UpdateSha256_Feed(&sha,work,n);position+=n;if(position<total)return;
        uint8_t hash[32];UpdateSha256_Final(&sha,hash);if(memcmp(hash,target_hash,32)){Done(7);return;}
        position=0;phase=command==1?4:5;return;
    }
    if(phase==4){
        if(position<4){uint32_t fat_offset=(position%2)*4096;
            if(!Write(1+position,fat+fat_offset)){Done(8);return;}++position;return;}
        if(!Write(5+entry*32/4096,root+(entry*32/4096)*4096)){Done(9);return;}
        phase=5;return;
    }
    if(phase==5){
        /* NOR only1->0 programming: commit last without another sector erase. */
        uint8_t commit[4]={0x54,0x31,0x43,0x4D}; /* pair-swapped CMT1 */
        uint32_t address=(start_sector+slot*128)*4096+4092;
        if(BSP_NOR_Program(address,commit,4)!=BSP_NOR_OK||disk_read(0,work,start_sector+slot*128,1)!=RES_OK||Resources_CheckHeader(work,target_hash)){Done(10);return;}
        P32(stage+4092,RESOURCES_COMMIT);Done(0);
    }
}
