#include "Noodoe_Crc32.h"
#include "Photo_Store.h"
#include "Photo_Jpeg.h"
#include "BSP_RAM.h"
#include "stm32f4xx_hal.h"
#include <string.h>
#define FILE_MAGIC 0x31465043U
#define JPEG_MAGIC 0x314A5043U
typedef struct  {
    uint8_t source[PHOTO_STORE_MAX_BYTES],verify[PHOTO_STORE_MAX_BYTES],header[4096],check[4096],work[4096];
    JDEC decoder;
    uint32_t phase,scan,bank,slot,length,crc,generation,pos,input_pos,x,y,rst,rsc,next_id,loading;
    uint32_t filling;
    struct {
        uint32_t id,result;
    } results[8];
    uint32_t results_head;
} PhotoStore;
static PhotoStore *w;
volatile PhotoStoreStatus g_photo_store;
static uint32_t Lock(void) {
    uint32_t m=__get_PRIMASK();
    __disable_irq();
    return m;
}
static void Unlock(uint32_t m) {
    __DMB();
    __set_PRIMASK(m);
}
static uint32_t Bank(void) {
    return 65536U+w->bank*PHOTO_STORE_BANK_BYTES;
}
uint32_t PhotoStore_Busy(void) {
    return w&&(!g_photo_store.ready||w->phase||w->filling);
}
static size_t Input(JDEC *d,uint8_t *dst,size_t n)
{
    (void)d;
    uint32_t left=w->length-w->input_pos;
    if(n>left)n=left;
    if(dst)memcpy(dst,w->verify+w->input_pos,n);
    w->input_pos+=n;
    return n;
}
static int Output(JDEC *d,void *tile,JRECT *r)
{
    (void)tile;
    return r->right<d->width&&r->bottom<d->height&&r->left<=r->right&&r->top<=r->bottom;
}
static uint32_t Prepare(void)
{
    w->input_pos=w->x=w->y=w->rst=w->rsc=0;
    if(jd_prepare(&w->decoder,Input,w->work,sizeof(w->work),w)!=JDR_OK)return CFW_CORRUPT;
    JDEC *d=&w->decoder;
    if(!d->width||!d->height||d->width>480||d->height>480)return CFW_ARGUMENT;
    d->scale=0;
    d->dcv[0]=d->dcv[1]=d->dcv[2]=0;
    return 0;
}
/* Same complete MCU/restart walk as the existing decoder, eight MCUs/call. */
static uint32_t Decode(void)
{
    JDEC *d=&w->decoder;
    for(uint32_t i=0;i<8;++i) {
        JRESULT r=JDR_OK;
        if(d->nrst&&w->rst++==d->nrst) {
            r=jd_restart(d,(uint16_t)w->rsc++);
            w->rst=1;
        }
        if(r==JDR_OK)r=jd_mcu_load(d);
        if(r==JDR_OK)r=jd_mcu_output(d,Output,w->x,w->y);
        if(r!=JDR_OK)return CFW_CORRUPT;
        w->x+=d->msx*8U;
        if(w->x>=d->width) {
            w->x=0;
            w->y+=d->msy*8U;
        }
        if(w->y>=d->height)return 0;
    }
    return CFW_PENDING;
}
static void NextScan(uint32_t error)
{
    if(error!=CFW_MISSING&&error)g_photo_store.failed_mask|=1U<<w->slot;
    if(++w->scan==6) {
        g_photo_store.ready=1;
        w->phase=0;
        g_photo_store.busy=0;
    }
    else w->phase=1;
}
static void Publish(void)
{
    uint32_t s=w->slot;
    if(!g_photo_store.length[s]||(int32_t)(w->generation-g_photo_store.generation[s])>0) {
        g_photo_store.generation[s]=w->generation;
        g_photo_store.length[s]=w->length;
        __DMB();
        g_photo_store.active[s]=w->bank;
        g_photo_store.failed_mask&=~(1U<<s);
    }
}
static void Done(uint32_t error,uint32_t now)
{
    if(!error) {
        Publish();
        g_photo_store.last_success_ms=now;
    }
    uint32_t m=Lock();
    g_photo_store.result=error;
    g_photo_store.completed=g_photo_store.request;
    w->results[w->results_head%8].id=g_photo_store.request;
    w->results[w->results_head++%8].result=error;
    w->phase=0;
    g_photo_store.busy=0;
    Unlock(m);
}
uint32_t PhotoStore_RequestReplace(uint32_t slot,const void *data,uint32_t n,uint32_t *id)
{
    if(slot>=3||!data||!id||!n||n>PHOTO_STORE_MAX_BYTES||__get_IPSR())return CFW_ARGUMENT;
    if(g_cfw_quiesce.request)return CFW_BUSY;
    if(!w||!g_photo_store.ready)return CFW_PENDING;
    if(g_photo_store.error)return g_photo_store.error;
    if(g_photo_store.unsupported&(1U<<slot))return CFW_VERSION;
    uint32_t m=Lock();
    if(w->phase||w->filling) {
        Unlock(m);
        return CFW_BUSY;
    }
    w->filling=1;
    Unlock(m);
    memcpy(w->source,data,n);
    w->length=n;
    w->slot=slot;
    w->bank=g_photo_store.length[slot]?g_photo_store.active[slot]^1U:slot*2;
    w->generation=g_photo_store.generation[slot]+1U;
    w->crc=0xFFFFFFFFU;
    w->pos=0;
    m=Lock();
    if(!++w->next_id)++w->next_id;
    *id=g_photo_store.request=w->next_id;
    g_photo_store.busy=1;
    w->phase=9;
    w->filling=0;
    Unlock(m);
    return 0;
}
uint32_t PhotoStore_GetResult(uint32_t id)
{
    if(!w||!id)return CFW_ARGUMENT;
    uint32_t m=Lock(),e=CFW_EXPIRED;
    if(g_photo_store.busy&&g_photo_store.request==id)e=CFW_PENDING;
    else for(uint32_t i=0;i<8;++i)if(w->results[i].id==id) {
        e=w->results[i].result;
        break;
    }
    Unlock(m);
    return e;
}
uint32_t PhotoStore_Read(uint32_t slot,void *out,uint32_t capacity,uint32_t *bytes)
{
    if(slot>=3||!out||!bytes)return CFW_ARGUMENT;
    if(!g_photo_store.ready)return CFW_PENDING;
    if(g_photo_store.error)return g_photo_store.error;
    uint32_t n=g_photo_store.length[slot];
    if(!n)return g_photo_store.failed_mask&(1U<<slot)?CFW_CORRUPT:CFW_MISSING;
    if(n>capacity)return CFW_ARGUMENT;
    uint32_t e=CfwFiles_Read(CFW_PHOTOS,65536+g_photo_store.active[slot]*PHOTO_STORE_BANK_BYTES+4096,out,n);
    if(!e)*bytes=n;
    return e;
}
uint32_t PhotoStore_ResetSlots(void)
{
    if(!w||!g_photo_store.ready||PhotoStore_Busy())return CFW_PENDING;
    if(g_photo_store.error)return g_photo_store.error;
    for(uint32_t bank=0;bank<6;bank++){
        uint32_t at=65536+bank*PHOTO_STORE_BANK_BYTES;
        uint32_t e=CfwFiles_Read(CFW_PHOTOS,at,w->check,4096);if(e)return e;
        for(uint32_t i=0;i<4096;i++)if(w->check[i]!=255){
            e=CfwFiles_Erase(CFW_PHOTOS,at);return e?e:CFW_PENDING;
        }
    }
    memset((void*)g_photo_store.length,0,sizeof(g_photo_store.length));
    memset((void*)g_photo_store.generation,0,sizeof(g_photo_store.generation));
    g_photo_store.failed_mask=g_photo_store.unsupported=0;
    return CFW_OK;
}
void PhotoStore_Process(uint32_t now)
{
    if(!w) {
        if(CfwFiles_Status()==CFW_PENDING)return;
        w=BSP_RAM_AllocateNamed(BSP_RAM_CFW_PHOTOS,sizeof(*w));
        if(!w) {
            g_photo_store.error=CFW_MEMORY;
            g_photo_store.ready=1;
            return;
        }
        memset(w,0,sizeof(*w));
        uint32_t e=!CfwFiles_Present(CFW_PHOTOS)?CFW_MISSING:CfwFiles_Read(CFW_PHOTOS,0,w->header,4096);
        if(!e)e=CfwRecord_Check(w->header,3);
        if(!e&&(Cfw_Get32(w->header+16)!=8||Cfw_Get32(w->header+64)!=FILE_MAGIC||Cfw_Get32(w->header+68)!=1))e=CFW_CORRUPT;
        if(!e)e=CfwFiles_Grant(CFW_PHOTOS);
        if(e) {
            g_photo_store.error=e;
            g_photo_store.ready=1;
            return;
        }
        w->phase=1;
    }
    if(g_photo_store.error||w->filling||!w->phase)return;
    uint32_t e=0;
    /* API callers only copy their bounded input. Hashing and the second
     * decoder copy run in 4KiB StorageTask slices, never in a UI/BT callback. */
    if(w->phase==9){
        uint32_t n=w->length-w->pos;if(n>4096)n=4096;
        memcpy(w->verify+w->pos,w->source+w->pos,n);
        w->crc=Noodoe_Crc32Feed(w->crc,w->source+w->pos,n);
        w->pos+=n;
        if(w->pos==w->length){w->crc^=0xFFFFFFFFU;w->phase=10;}
        return;
    }
    if(w->phase==1) {
        w->bank=w->scan;
        w->slot=w->bank/2;
        e=CfwFiles_Read(CFW_PHOTOS,Bank(),w->header,4096);
        if(e) {
            g_photo_store.error=e;
            g_photo_store.ready=1;
            return;
        }
        uint32_t blank=1;
        for(uint32_t i=0;i<4096;++i)if(w->header[i]!=255) {
            blank=0;
            break;
        }
        if(blank) {
            NextScan(CFW_MISSING);
            return;
        }
        e=CfwRecord_Check(w->header,16+w->slot);
        if(e==CFW_VERSION)g_photo_store.unsupported|=1U<<w->slot;
        if(e) {
            NextScan(e);
            return;
        }
        w->generation=Cfw_Get32(w->header+12);
        w->length=Cfw_Get32(w->header+68);
        w->crc=Cfw_Get32(w->header+72);
        if(Cfw_Get32(w->header+16)!=12||Cfw_Get32(w->header+64)!=JPEG_MAGIC||!w->length||w->length>PHOTO_STORE_MAX_BYTES) {
            NextScan(CFW_CORRUPT);
            return;
        }
        w->pos=0;
        w->phase=2;
        return;
    }
    if(w->phase==2||w->phase==15) {
        uint32_t n=w->length-w->pos;
        if(n>4096)n=4096;
        e=CfwFiles_Read(CFW_PHOTOS,Bank()+4096+w->pos,w->verify+w->pos,n);
        if(!e&&w->phase==15&&memcmp(w->verify+w->pos,w->source+w->pos,n))e=CFW_CORRUPT;
        w->pos+=n;
        if(!e&&w->pos==w->length) {
            if(Cfw_Crc(w->verify,w->length)!=w->crc)e=CFW_CORRUPT;
            if(!e)e=Prepare();
            if(!e)w->phase=w->phase==2?3:16;
        }
    }
    else if(w->phase==3) {
        e=Decode();
        if(e==CFW_PENDING)return;
        if(!e)Publish();
        NextScan(e);
        return;
    }
    else if(w->phase==10) {
        e=Prepare();
        if(!e)w->phase=11;
    }
    else if(w->phase==11) {
        e=Decode();
        if(e==CFW_PENDING)return;
        if(!e) {
            uint8_t b[12];
            Cfw_Put32(b,JPEG_MAGIC);
            Cfw_Put32(b+4,w->length);
            Cfw_Put32(b+8,w->crc);
            CfwRecord_Make(w->header,16+w->slot,w->generation,b,12);
            w->pos=0;
            w->phase=12;
        }
    }
    else if(w->phase==12) {
        e=CfwFiles_Erase(CFW_PHOTOS,Bank()+w->pos);
        if(!e) {
            w->pos+=4096;
            if(w->pos==PHOTO_STORE_BANK_BYTES) {
                w->pos=0;
                w->phase=13;
            }
        }
    }
    else if(w->phase==13) {
        e=CfwFiles_Program(CFW_PHOTOS,Bank()+w->pos,w->header+w->pos,256);
        if(!e) {
            w->pos+=256;
            if(w->pos==4096) {
                w->pos=0;
                w->phase=14;
            }
        }
    }
    else if(w->phase==14) {
        uint8_t page[256];
        memset(page,255,256);
        uint32_t n=w->length-w->pos;
        if(n>256)n=256;
        memcpy(page,w->source+w->pos,n);
        e=CfwFiles_Program(CFW_PHOTOS,Bank()+4096+w->pos,page,256);
        if(!e) {
            w->pos+=n;
            if(w->pos==w->length) {
                w->pos=0;
                w->phase=15;
            }
        }
    }
    else if(w->phase==16) {
        e=Decode();
        if(e==CFW_PENDING)return;
        if(!e)w->phase=17;
    }
    else if(w->phase==17) {
        e=CfwFiles_Read(CFW_PHOTOS,Bank(),w->check,4096);
        if(!e&&memcmp(w->header,w->check,4096))e=CFW_CORRUPT;
        if(!e)w->phase=18;
    }
    else if(w->phase==18) {
        uint8_t c[4];
        Cfw_Put32(c,CFW_RECORD_COMMIT);
        e=CfwFiles_Program(CFW_PHOTOS,Bank()+4092,c,4);
        if(!e)w->phase=19;
    }
    else if(w->phase==19) {
        e=CfwFiles_Read(CFW_PHOTOS,Bank(),w->check,4096);
        Cfw_Put32(w->header+4092,CFW_RECORD_COMMIT);
        if(!e&&(memcmp(w->header,w->check,4096)||CfwRecord_Check(w->check,16+w->slot)))e=CFW_CORRUPT;
        Done(e,now);
        return;
    }
    if(e) {
        if(!g_photo_store.ready)NextScan(e);
        else Done(e,now);
    }
}
