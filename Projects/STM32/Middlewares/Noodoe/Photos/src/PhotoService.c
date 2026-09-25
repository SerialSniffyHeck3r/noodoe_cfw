#include "PhotoService.h"
#include "StorageService.h"
#include "BSP_RAM.h"
#include "Photo_Jpeg.h"
#include "stm32f4xx_hal.h"
#if NOODOE_PRODUCT
#include "Photo_Store.h"
#endif
#include <string.h>
#define INPUT_BYTES (128U*1024U)
#define WORK_BYTES 4096U
/* Large immutable pixels and decoder scratch use external SDRAM, never the
 * tiny internal SRAM remainder or the graphics heap. Allocate each only once. */
typedef struct {JDEC decoder;uint8_t input[INPUT_BYTES],work[WORK_BYTES];PhotoImage images[3];
    uint8_t *pixels[3],*spare[3];const void *retiring[3];uint32_t revision,refresh,swapped;
    uint32_t offset,length,x,y,rst,rsc;} PhotoWorker;
static PhotoWorker *worker;
static uint32_t import_sequence,import_wallpaper;
#if NOODOE_PRODUCT
static uint32_t import_store_request;
#endif
volatile PhotoImport g_photo_import;
static void ImportDone(uint32_t result)
{
    if(!import_sequence)return;
    g_photo_import.result=result;__atomic_store_n(&g_photo_import.ack,import_sequence,__ATOMIC_RELEASE);import_sequence=0;
}
volatile PhotoDiagnostics g_photos;
uint32_t PhotoService_RequestLoad(uint32_t slot)
{
    if(slot>=PHOTO_SLOTS)return 0;
    if(__atomic_load_n(&g_photos.ready_mask,__ATOMIC_ACQUIRE)&(1U<<slot))return 1;
    __atomic_fetch_or(&g_photos.pending,1U<<slot,__ATOMIC_RELEASE);return 1;
}
uint32_t PhotoService_Get(uint32_t slot,PhotoImage *out)
{
#if NOODOE_PRODUCT
    if(slot>=3||!g_photo_store.length[slot])return 0;
#endif
    if(!out||slot>=PHOTO_SLOTS||!(__atomic_load_n(&g_photos.ready_mask,__ATOMIC_ACQUIRE)&(1U<<slot)))return 0;
    uint32_t m=__get_PRIMASK();__disable_irq();*out=worker->images[slot];__set_PRIMASK(m);return 1;
}
const void *PhotoService_Retiring(uint32_t slot){return worker&&slot<3?worker->retiring[slot]:NULL;}
void PhotoService_Release(uint32_t slot,const void *pixels)
{if(worker&&slot<3&&worker->retiring[slot]==pixels){__DMB();worker->retiring[slot]=NULL;}}
/* Decoder input permits bounded skips for EXIF; malformed lengths cannot walk
 * outside the validated file. No filesystem lock is held while decompressing. */
static size_t Input(JDEC *jd,uint8_t *dst,size_t count)
{
    PhotoWorker *w=jd->device;uint32_t left=w->length-w->offset;
    if(count>left)count=left;
    if(dst)memcpy(dst,w->input+w->offset,count);
    w->offset+=count;return count;
}
/* Configured native RGB565 tiles become tightly packed little-endian RGB565.
 * Validate every rectangle before publication; the renderer only borrows completed pixels. */
static int Output(JDEC *jd,void *tile,JRECT *r)
{
    PhotoWorker *w=jd->device;
    if(r->right>=jd->width||r->bottom>=jd->height||r->left>r->right||r->top>r->bottom)return 0;
    if(import_wallpaper)return 1; /* Validation only: never overwrite resident pixels. */
    const uint8_t *p=tile;uint8_t *dest=w->pixels[g_photos.slot];
    uint32_t row_bytes=(r->right-r->left+1U)*2U;
    /* Cortex-M4 and EVE both use little-endian RGB565. Copy only visible tile
     * rows, including partial right/bottom MCUs; no per-pixel conversion. */
    for(uint32_t y=r->top;y<=r->bottom;++y){
        PhotoJpeg_CopyRGB565(dest+(y*jd->width+r->left)*2U,p,row_bytes/2U);p+=row_bytes;
    }
    return 1;
}
static void Fail(uint32_t result)
{
    if(worker&&worker->swapped){uint32_t s=g_photos.slot;uint8_t *bad=worker->pixels[s];
        worker->pixels[s]=worker->spare[s];worker->spare[s]=bad;worker->swapped=0;}
    g_photos.result=result;g_photos.failed_mask|=1U<<g_photos.slot;g_photos.active=0;ImportDone(result);
}
void PhotoService_Process(void)
{
    /* Keep zero-filled diagnostics in BSS rather than storing112 zero bytes
     * in flash. Publish each magic after its ABI/capacity have been initialized. */
    if(!g_photos.magic){g_photos.version=1;g_photos.magic=0x50484F31U;
#if NOODOE_PRODUCT
        g_photo_import.version=3;
#else
        g_photo_import.version=2;
#endif
        g_photo_import.capacity=INPUT_BYTES;g_photo_import.magic=0x50494D31U;}
    if(!g_storage_service.mounted)return;
#if NOODOE_PRODUCT
    if(!g_photo_store.ready)return;
    /* A confirmed-install reset can restart durable photo generations at one.
     * Invalidate cached revisions while empty, keeping borrowed pixels intact.
     * Otherwise a new generation-one photo could reuse the old decoded image. */
    if(worker)for(uint32_t s=0;s<3;s++)if(!g_photo_store.length[s])worker->images[s].revision=0;
    if(g_photos.active&&!g_photo_store.length[g_photos.slot]){Fail(0x800U+CFW_MISSING);return;}
    if(import_store_request){
        uint32_t e=PhotoStore_GetResult(import_store_request);
        if(e!=CFW_PENDING){import_store_request=0;ImportDone(e?0x800U+e:0);}
        return;
    }
#endif
    if(!g_photos.active){
        uint32_t pending=__atomic_load_n(&g_photos.pending,__ATOMIC_ACQUIRE);
        uint32_t sequence=__atomic_load_n(&g_photo_import.sequence,__ATOMIC_ACQUIRE);
        uint32_t importing=sequence&&sequence!=g_photo_import.ack;
#if NOODOE_PRODUCT
        if(worker&&!importing)for(uint32_t s=0;s<3;++s){
            if(g_photo_store.length[s]&&worker->images[s].revision!=g_photo_store.generation[s]+0x10000U&&!worker->retiring[s])
                pending|=1U<<s;
        }
#endif
        if(!pending&&!importing){g_photo_import.buffer=worker?(uint32_t)worker->input:0;return;}
        uint32_t slot=0,length=0;
        if(importing){
#if NOODOE_PRODUCT
            /* Product ABI3 writes only the audited CFWPIC A/B bank. Original
             * WALL/album files are read-only migration inputs forever. */
            import_sequence=sequence;slot=g_photo_import.slot;length=g_photo_import.length;
            uint32_t arm=g_photo_import.arm;g_photo_import.arm=0;
            if(pending||slot>=3U||!worker||!g_photo_import.buffer||arm!=0x42414B32U||
                !length||length>INPUT_BYTES){ImportDone(0x600U);return;}
            g_photo_import.buffer=0;
            if((StorageService_Crc32(worker->input,length,0xFFFFFFFFU)^0xFFFFFFFFU)!=g_photo_import.crc32){ImportDone(0x601U);return;}
            uint32_t e=PhotoStore_RequestReplace(slot,worker->input,length,&import_store_request);
            if(e)ImportDone(0x800U+e);
            return;
#else
            import_sequence=sequence;slot=g_photo_import.slot;length=g_photo_import.length;
            import_wallpaper=!!(slot&PHOTO_IMPORT_WALLPAPER);slot&=~PHOTO_IMPORT_WALLPAPER;
            uint32_t arm=g_photo_import.arm;g_photo_import.arm=0;
            if(pending||g_photos.active||slot>=3U||!worker||!g_photo_import.buffer||arm!=0x42414B32U||
               !length||length>INPUT_BYTES||(!import_wallpaper&&(g_photos.ready_mask&(1U<<slot)))){ImportDone(0x600U);return;}
            if((StorageService_Crc32(worker->input,length,0xFFFFFFFFU)^0xFFFFFFFFU)!=g_photo_import.crc32){ImportDone(0x601U);return;}
#endif
        }else{
            import_wallpaper=0;
            while(!(pending&(1U<<slot)))++slot;
            __atomic_fetch_and(&g_photos.pending,~(1U<<slot),__ATOMIC_ACQ_REL);
            if(!import_wallpaper&&(g_photos.ready_mask&(1U<<slot))){
#if NOODOE_PRODUCT
                if(!g_photo_store.length[slot]||worker->images[slot].revision==g_photo_store.generation[slot]+0x10000U)return;
#else
                return;
#endif
            }
        }
        g_photo_import.buffer=0;
        g_photos.slot=slot;g_photos.result=0;g_photos.decoded_mcus=0;
        if(!worker){worker=BSP_RAM_Allocate(sizeof(*worker));if(worker)memset(worker,0,sizeof(*worker));}
        if(!worker){Fail(0x100U);return;}
        worker->refresh=!!(g_photos.ready_mask&(1U<<slot));worker->revision=slot+1U;
        uint32_t received=0;
#if !NOODOE_PRODUCT
        char path[40];
        FRESULT r=import_wallpaper?FR_NO_FILE:
            importing?StorageService_FindAlbumPhoto(slot,path,sizeof(path),NULL):StorageService_FindWallpaper(slot,path,sizeof(path),&length);
        if(importing){if(r!=FR_NO_FILE){Fail(0x602U);return;}}
        else{
#else
        {
            uint32_t stored=PhotoStore_Read(slot,worker->input,INPUT_BYTES,&received);
            if(!stored){length=received;worker->revision=g_photo_store.generation[slot]+0x10000U;}
            else if(stored==CFW_MISSING){g_photos.result=0;g_photos.active=0;return;}
            else{Fail(0x800U+stored);return;}
        }
#endif
#if !NOODOE_PRODUCT
            if(r!=FR_OK){Fail(0x200U+r);return;}
            if(!length||length>INPUT_BYTES){Fail(0x101U);return;}
            r=StorageService_ReadFile(path,0,worker->input,length,&received);
            if(r!=FR_OK||received!=length){Fail(0x300U+r);return;}
        }
#endif
        worker->length=length;worker->offset=0;
        JRESULT j=jd_prepare(&worker->decoder,Input,worker->work,sizeof(worker->work),worker);
        if(j!=JDR_OK){Fail(0x400U+j);return;}
        JDEC *d=&worker->decoder;
        if(!d->width||!d->height||d->width>480U||d->height>480U){Fail(0x102U);return;}
        if(!import_wallpaper){
            if(worker->refresh){
                if(!worker->spare[slot])worker->spare[slot]=BSP_RAM_Allocate(480U*480U*2U);
                if(!worker->spare[slot]){Fail(0x100U);return;}
                uint8_t *old=worker->pixels[slot];worker->pixels[slot]=worker->spare[slot];worker->spare[slot]=old;
                worker->swapped=1;
            }
            if(!worker->pixels[slot])worker->pixels[slot]=BSP_RAM_Allocate(480U*480U*2U);
            if(!worker->pixels[slot]){Fail(0x100U);return;}
        }
        worker->x=worker->y=worker->rst=worker->rsc=0;d->scale=0;
        d->dcv[0]=d->dcv[1]=d->dcv[2]=0;
        if(!import_wallpaper){g_photos.file_bytes[slot]=length;g_photos.width[slot]=d->width;g_photos.height[slot]=d->height;}
        g_photos.active=1;g_photos.failed_mask&=~(1U<<slot);return;
    }
    /* Same MCU/restart order as pinned ChaN jd_decomp, split across calls so
     * USB/control/clock work can run between small decode batches. */
    JDEC *d=&worker->decoder;
    for(uint32_t i=0;i<8U;++i){
        JRESULT r=JDR_OK;
        if(d->nrst&&worker->rst++==d->nrst){r=jd_restart(d,(uint16_t)worker->rsc++);worker->rst=1;}
        if(r==JDR_OK)r=jd_mcu_load(d);
        if(r==JDR_OK)r=jd_mcu_output(d,Output,worker->x,worker->y);
        if(r!=JDR_OK){Fail(0x500U+r);return;}
        ++g_photos.decoded_mcus;worker->x+=d->msx*8U;
        if(worker->x>=d->width){worker->x=0;worker->y+=d->msy*8U;}
        if(worker->y>=d->height){
            uint32_t slot=g_photos.slot;
#if !NOODOE_PRODUCT
            if(import_sequence){
                FRESULT stored=import_wallpaper?StorageService_CreateWallpaper(slot,worker->input,worker->length,0x42414B32U):
                    StorageService_CreateAlbumPhoto(slot,worker->input,worker->length,0x42414B32U);
                if(stored!=FR_OK){Fail(0x700U+stored);return;}
            }
#endif
            if(import_wallpaper){g_photos.active=0;ImportDone(0);return;}
            uint32_t m=__get_PRIMASK();__disable_irq();
            if(worker->refresh)worker->retiring[slot]=worker->images[slot].pixels;
            worker->images[slot]=(PhotoImage){worker->pixels[slot],d->width,d->height,d->width*d->height*2U,worker->revision};
            worker->swapped=0;
            __DMB();__set_PRIMASK(m);
            __atomic_fetch_or(&g_photos.ready_mask,1U<<slot,__ATOMIC_RELEASE);g_photos.active=0;ImportDone(0);return;
        }
    }
}
