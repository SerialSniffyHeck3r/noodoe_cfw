#include "Cfw_Store.h"
#include "BSP_RAM.h"
#include "stm32f4xx_hal.h"
#include <string.h>
typedef struct  {
    uint8_t saved[CFW_PAYLOAD_MAX],pending[CFW_PAYLOAD_MAX];
    uint32_t length,pending_length,pending_id,valid,active,generation,unsupported;
} Journal;
typedef struct  {
    Journal j[2];
    uint8_t write[4096],read[4096];
    uint32_t scan_file,scan_sector,phase,file,target,page,next_id;
    struct  {
        uint32_t id,result;
    } results[8];
    uint32_t result_head;
} Store;
static Store *s;
volatile CfwStoreDiagnostics g_cfw_store;
volatile CfwQuiesce g_cfw_quiesce= {
    0x53554150U,0,0
};
static uint32_t Lock(void) {
    uint32_t m=__get_PRIMASK();
    __disable_irq();
    return m;
}
static void Unlock(uint32_t m) {
    __DMB();
    __set_PRIMASK(m);
}
static uint32_t UID(uint32_t i) {
    return i==0?HAL_GetUIDw0():i==1?HAL_GetUIDw1():HAL_GetUIDw2();
}
void CfwRecord_Make(uint8_t *p,uint32_t purpose,uint32_t gen,const void *data,uint32_t n)
{
    memset(p,0xFF,4096);
    Cfw_Put32(p,CFW_RECORD_MAGIC);
    Cfw_Put32(p+4,1);
    Cfw_Put32(p+8,purpose);
    Cfw_Put32(p+12,gen);
    Cfw_Put32(p+16,n);
    for(uint32_t i=0;i<3;++i)Cfw_Put32(p+20+i*4,UID(i));
    if(n)memcpy(p+64,data,n);
    Cfw_Put32(p+4088,Cfw_Crc(p,4088));
}
uint32_t CfwRecord_Check(const uint8_t *p,uint32_t purpose)
{
    if(Cfw_Get32(p)!=CFW_RECORD_MAGIC||Cfw_Get32(p+8)!=purpose||Cfw_Get32(p+4092)!=CFW_RECORD_COMMIT||
    Cfw_Get32(p+4088)!=Cfw_Crc(p,4088))return CFW_CORRUPT;
    for(uint32_t i=0;i<3;++i)if(Cfw_Get32(p+20+i*4)!=UID(i))return CFW_CORRUPT;
    if(Cfw_Get32(p+4)!=1)return CFW_VERSION;
    return Cfw_Get32(p+16)<=CFW_PAYLOAD_MAX?CFW_OK:CFW_CORRUPT;
}
void CfwStore_Init(void)
{
    if(s)return;
    s=BSP_RAM_AllocateNamed(BSP_RAM_CFW_JOURNAL,sizeof(*s));
    memset((void*)&g_cfw_store,0,sizeof(g_cfw_store));
    g_cfw_store.magic=0x31534643;
    g_cfw_store.version=1;
    if(!s) {
        g_cfw_store.error=CFW_MEMORY;
        g_cfw_store.ready=1;
        return;
    }
    memset(s,0,sizeof(*s));
    CfwFiles_Begin();
}
uint32_t CfwStore_Ready(void) {
    return g_cfw_store.ready;
}
uint32_t CfwStore_Busy(void)
{
    return s&&(!g_cfw_store.ready||s->phase||s->j[0].pending_id||s->j[1].pending_id);
}
void CfwStore_GetStatus(uint32_t f,CfwStoreStatus *out)
{
    if(f>=2||!out)return;
    uint32_t m=Lock();
    memcpy(out,(const void*)&g_cfw_store.files[f],sizeof(*out));
    Unlock(m);
}
uint32_t CfwStore_Read(uint32_t f,void *out,uint32_t capacity,uint32_t *bytes)
{
    if(f>=2||!out||!bytes)return CFW_ARGUMENT;
    if(g_cfw_store.error)return g_cfw_store.error;
    if(!s||!g_cfw_store.ready)return CFW_PENDING;
    uint32_t m=Lock(),e=g_cfw_store.error?g_cfw_store.error:g_cfw_store.files[f].status;
    if(!e) {
        if(capacity<s->j[f].length)e=CFW_ARGUMENT;
        else {
            *bytes=s->j[f].length;
            memcpy(out,s->j[f].saved,*bytes);
        }
    }
    Unlock(m);
    return e;
}
static void Complete(uint32_t f,uint32_t e,uint32_t now)
{
    Journal *j=&s->j[f];
    volatile CfwStoreStatus *d=&g_cfw_store.files[f];
    uint32_t m=Lock();
    s->results[s->result_head%8].id=j->pending_id;
    s->results[s->result_head++%8].result=e;
    if(!e) {
        j->length=j->pending_length;
        memcpy(j->saved,j->pending,j->length);
        d->last_success_ms=now;
        d->state=CFW_STORE_SAVED;
        d->dirty=0;
    }
    else {
        ++d->failures;
        d->state=CFW_STORE_ERROR;
        d->dirty=1;
    }
    d->status=e;
    j->pending_id=0;
    s->phase=0;
    Unlock(m);
}
uint32_t CfwStore_Request(uint32_t f,const void *data,uint32_t n,uint32_t *id)
{
    if(f>=2||!data||!id||n>CFW_PAYLOAD_MAX||__get_IPSR())return CFW_ARGUMENT;
    if(g_cfw_quiesce.request)return CFW_BUSY;
    if(!s||!g_cfw_store.ready)return CFW_PENDING;
    uint32_t m=Lock();
    Journal *j=&s->j[f];
    uint32_t e=g_cfw_store.error?g_cfw_store.error:!j->valid?g_cfw_store.files[f].status:j->unsupported?CFW_VERSION:0;
    if(!e&&j->pending_id)e=CFW_BUSY;
    if(!e) {
        if(!++s->next_id)++s->next_id;
        *id=j->pending_id=s->next_id;
        j->pending_length=n;
        memcpy(j->pending,data,n);
        g_cfw_store.files[f].request=*id;
        g_cfw_store.files[f].state=CFW_STORE_DIRTY;
        g_cfw_store.files[f].dirty=1;
    }
    Unlock(m);
    return e;
}
uint32_t CfwStore_Result(uint32_t id)
{
    if(!id||!s)return CFW_ARGUMENT;
    uint32_t m=Lock(),e=CFW_EXPIRED;
    if(s->j[0].pending_id==id||s->j[1].pending_id==id)e=CFW_PENDING;
    else for(uint32_t i=0;i<8;++i)if(s->results[i].id==id) {
        e=s->results[i].result;
        break;
    }
    Unlock(m);
    return e;
}
static void Scan(void)
{
    uint32_t f=s->scan_file;
    Journal *j=&s->j[f];
    uint32_t n=CfwFiles_Size(f)/4096;
    if(!CfwFiles_Present(f)) {
        g_cfw_store.files[f].status=CFW_MISSING;
        s->scan_sector=n;
    }
    if(s->scan_sector<n) {
        uint32_t e=CfwFiles_Read(f,s->scan_sector*4096,s->read,4096);
        if(e) {
            g_cfw_store.error=e;
            g_cfw_store.ready=1;
            return;
        }
        e=CfwRecord_Check(s->read,f+1);
        if(e==CFW_VERSION)j->unsupported=1;
        if(e==CFW_OK) {
            uint32_t gen=Cfw_Get32(s->read+12);
            if(!j->valid||(int32_t)(gen-j->generation)>0) {
                j->valid=1;
                j->active=s->scan_sector;
                j->generation=gen;
                j->length=Cfw_Get32(s->read+16);
                memcpy(j->saved,s->read+64,j->length);
            }
        }
        ++s->scan_sector;
        return;
    }
    uint32_t e=j->unsupported?CFW_VERSION:!CfwFiles_Present(f)?CFW_MISSING:!j->valid?CFW_CORRUPT:CfwFiles_Grant(f);
    volatile CfwStoreStatus *d=&g_cfw_store.files[f];
    d->status=e;
    d->state=e?CFW_STORE_ERROR:CFW_STORE_SAVED;
    d->generation=j->generation;
    d->active_sector=j->active;
    s->scan_sector=0;
    if(++s->scan_file==2)g_cfw_store.ready=1;
}
void CfwStore_Process(uint32_t now)
{
    if(!s||g_cfw_store.error)return;
    if(!g_cfw_store.ready) {
        uint32_t e=CfwFiles_Process();
        if(e==CFW_PENDING)return;
        if(e) {
            g_cfw_store.error=e;
            g_cfw_store.ready=1;
            return;
        }
        Scan();
        return;
    }
    if(!s->phase) {
        /* Ride checkpoints take priority; no photo task can own this arena. */
        uint32_t f=s->j[1].pending_id?1:0;
        if(!s->j[f].pending_id)return;
        s->file=f;
        Journal *j=&s->j[f];
        if(j->length==j->pending_length&&!memcmp(j->saved,j->pending,j->length)) {
            ++g_cfw_store.files[f].deduplicated;
            Complete(f,0,now);
            return;
        }
        s->target=(j->active+1)%(CfwFiles_Size(f)/4096);
        s->page=0;
        CfwRecord_Make(s->write,f+1,j->generation+1,j->pending,j->pending_length);
        g_cfw_store.files[f].state=CFW_STORE_SAVING;
        s->phase=1;
        return;
    }
    uint32_t f=s->file,off=s->target*4096,e=0;
    if(s->phase==1) {
        e=CfwFiles_Erase(f,off);
        if(!e)s->phase=2;
    }
    else if(s->phase==2) {
        e=CfwFiles_Program(f,off+s->page*256,s->write+s->page*256,256);
        if(!e&&++s->page==16)s->phase=3;
    }
    else if(s->phase==3) {
        e=CfwFiles_Read(f,off,s->read,4096);
        if(!e&&memcmp(s->read,s->write,4096))e=CFW_CORRUPT;
        if(!e)s->phase=4;
    }
    else if(s->phase==4) {
        uint8_t c[4];
        Cfw_Put32(c,CFW_RECORD_COMMIT);
        e=CfwFiles_Program(f,off+4092,c,4);
        if(!e)s->phase=5;
    }
    else if(s->phase==5) {
        e=CfwFiles_Read(f,off,s->read,4096);
        Cfw_Put32(s->write+4092,CFW_RECORD_COMMIT);
        if(!e&&(memcmp(s->read,s->write,4096)||CfwRecord_Check(s->read,f+1)))e=CFW_CORRUPT;
        if(!e) {
            Journal *j=&s->j[f];
            j->active=s->target;
            ++j->generation;
            g_cfw_store.files[f].active_sector=j->active;
            g_cfw_store.files[f].generation=j->generation;
            ++g_cfw_store.files[f].writes;
            Complete(f,0,now);
            return;
        }
    }
    if(e)Complete(f,e,now);
}
