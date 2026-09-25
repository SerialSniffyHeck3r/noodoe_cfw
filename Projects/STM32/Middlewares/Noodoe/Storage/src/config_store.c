#include "Config_Store.h"
#include "BSP_RAM.h"
#include "stm32f4xx_hal.h"
#include <string.h>
#define MAGIC 0x31474643U
typedef struct  {
    uint8_t data[CFW_PAYLOAD_MAX];
    uint32_t bytes,request,attempt_ms;
} ConfigCache;
static ConfigCache *c;
static uint32_t trial_view;
static uint32_t Preference(uint32_t field){return field==CONFIG_FIELD_RIDER||(field>=0x1000U&&field<0x2000U);}
void ConfigStore_SetTrialView(uint32_t enabled){trial_view=enabled;}
volatile ConfigStoreStatus g_config_store;
static uint32_t Lock(void) {
    uint32_t m=__get_PRIMASK();
    __disable_irq();
    return m;
}
static void Unlock(uint32_t m) {
    __DMB();
    __set_PRIMASK(m);
}
static uint32_t U16(const uint8_t *p) {
    uint16_t value;memcpy(&value,p,2);return value;
}
static void P16(uint8_t *p,uint32_t n) {
    p[0]=n;
    p[1]=n>>8;
}
static uint32_t Valid(void)
{
    if(c->bytes<8||Cfw_Get32(c->data)!=MAGIC)return CFW_CORRUPT;
    if(Cfw_Get32(c->data+4)!=1)return CFW_VERSION;
    uint32_t off=8,last=0;
    while(off<c->bytes) {
        if(c->bytes-off<4)return CFW_CORRUPT;
        uint32_t id=U16(c->data+off),n=U16(c->data+off+2);
        if(id<=last||n>c->bytes-off-4)return CFW_CORRUPT;
        last=id;
        off+=4+n;
    }
    return CFW_OK;
}
uint32_t ConfigStore_Get(uint32_t field,void *out,uint32_t capacity,uint32_t *bytes)
{
    if(!out||!bytes||!field||field>65535)return CFW_ARGUMENT;
    if(!g_config_store.ready)return CFW_PENDING;
    if(g_config_store.error)return g_config_store.error;
    if(trial_view==1&&Preference(field)){*bytes=0;return CFW_MISSING;}
    uint32_t m=Lock(),e=CFW_MISSING;
    for(uint32_t o=8;o<c->bytes;) {
        uint32_t id=U16(c->data+o),n=U16(c->data+o+2);
        if(id==field) {
            if(n>capacity)e=CFW_ARGUMENT;
            else {
                memcpy(out,c->data+o+4,n);
                *bytes=n;
                e=0;
            }
            break;
        }
        o+=4+n;
    }
    Unlock(m);
    return e;
}
uint32_t ConfigStore_Set(uint32_t field,const void *data,uint32_t n,uint32_t *revision)
{
    if(!data||!revision||!field||field>65535||n>CFW_PAYLOAD_MAX-12||__get_IPSR())return CFW_ARGUMENT;
    if(!g_config_store.ready)return CFW_PENDING;
    if(g_config_store.error)return g_config_store.error;
    if(trial_view&&Preference(field))return CFW_BUSY;
    uint32_t m=Lock(),o=8,old=0;
    while(o<c->bytes) {
        uint32_t id=U16(c->data+o),len=U16(c->data+o+2);
        if(id>=field) {
            if(id==field)old=len+4;
            break;
        }
        o+=4+len;
    }
    if(old==n+4&&!memcmp(c->data+o+4,data,n)) {
        *revision=g_config_store.revision;
        Unlock(m);
        return 0;
    }
    if(c->bytes-old+4+n>CFW_PAYLOAD_MAX) {
        Unlock(m);
        return CFW_MEMORY;
    }
    memmove(c->data+o+4+n,c->data+o+old,c->bytes-o-old);
    P16(c->data+o,field);
    P16(c->data+o+2,n);
    memcpy(c->data+o+4,data,n);
    c->bytes=c->bytes-old+4+n;
    uint32_t now=HAL_GetTick();
    if(g_config_store.revision==g_config_store.saved_revision)g_config_store.dirty_since=now;
    g_config_store.changed_ms=now;
    if(!++g_config_store.revision)++g_config_store.revision;
    *revision=g_config_store.revision;
    Unlock(m);
    return 0;
}
uint32_t ConfigStore_Result(uint32_t r)
{
    if(!g_config_store.ready)return CFW_PENDING;
    if(g_config_store.error)return g_config_store.error;
    if((int32_t)(g_config_store.saved_revision-r)>=0)return CFW_OK;
    if(g_config_store.last_result)return g_config_store.last_result;
    return CFW_PENDING;
}
uint32_t ConfigStore_SetWords(const uint16_t *fields,const uint32_t *values,uint32_t count,uint32_t *revision)
{
    if(!fields||!values||!revision||!count||count>32||__get_IPSR())return CFW_ARGUMENT;
    if(!g_config_store.ready)return CFW_PENDING;
    if(g_config_store.error)return g_config_store.error;
    uint32_t m=Lock(),needed=c->bytes;
    for(uint32_t i=0;i<count;++i){
        if(trial_view&&Preference(fields[i])){Unlock(m);return CFW_BUSY;}
        if(!fields[i]){Unlock(m);return CFW_ARGUMENT;}
        for(uint32_t j=0;j<i;++j)if(fields[i]==fields[j]){Unlock(m);return CFW_ARGUMENT;}
        uint32_t old=0;
        for(uint32_t o=8;o<c->bytes;){uint32_t n=U16(c->data+o+2);
            if(U16(c->data+o)==fields[i]){
                if(n!=4){Unlock(m);return CFW_CORRUPT;}
                old=n+4;break;}o+=n+4;}
        needed=needed-old+8;
    }
    if(needed>CFW_PAYLOAD_MAX){Unlock(m);return CFW_MEMORY;}
    /* Every possible failure was preflighted while IRQ scheduling is locked. */
    for(uint32_t i=0;i<count;++i){uint8_t b[4];Cfw_Put32(b,values[i]);
        (void)ConfigStore_Set(fields[i],b,4,revision);}
    Unlock(m);return CFW_OK;
}
void ConfigStore_Reject(uint32_t error)
{
    if(error==CFW_CORRUPT||error==CFW_VERSION)g_config_store.error=error;
}
/* Defaults and reset epoch occupy one committed configuration record. Keys,
 * maintenance baselines (separate ride file), and unknown fields survive. */
uint32_t ConfigStore_ResetPreferences(uint32_t epoch,uint32_t *revision)
{
    if(!epoch||!revision)return CFW_ARGUMENT;
    if(!c||!g_config_store.ready)return CFW_PENDING;
    if(g_config_store.error)return g_config_store.error;
    if(c->request)return CFW_BUSY;
    uint32_t m=Lock(),found=0,needed=16;
    for(uint32_t o=8;o<c->bytes;){uint32_t id=U16(c->data+o),n=U16(c->data+o+2);
        if(id==CONFIG_FIELD_RESET_EPOCH&&n==4&&Cfw_Get32(c->data+o+4)==epoch)found=1;
        if(!Preference(id)&&id!=CONFIG_FIELD_RESET_EPOCH)needed+=n+4;
        o+=n+4;
    }
    if(found){*revision=g_config_store.revision;trial_view=0;Unlock(m);return 0;}
    if(needed>CFW_PAYLOAD_MAX){Unlock(m);return CFW_MEMORY;}
    uint32_t out=8;
    for(uint32_t o=8;o<c->bytes;){uint32_t id=U16(c->data+o),n=U16(c->data+o+2)+4;
        if(!Preference(id)&&id!=CONFIG_FIELD_RESET_EPOCH){memmove(c->data+out,c->data+o,n);out+=n;}o+=n;
    }
    c->bytes=out;trial_view=0;uint8_t b[4];Cfw_Put32(b,epoch);
    uint32_t result=ConfigStore_Set(CONFIG_FIELD_RESET_EPOCH,b,4,revision);Unlock(m);return result;
}
uint32_t ConfigStore_Busy(void)
{
    return !g_config_store.ready||(c&&(c->request||(!g_config_store.error&&g_config_store.revision!=g_config_store.saved_revision)));
}
void ConfigStore_Process(uint32_t now)
{
    if(!g_config_store.ready) {
        if(!CfwStore_Ready())return;
        if(!c) {
            c=BSP_RAM_AllocateNamed(BSP_RAM_CFW_CONFIG,sizeof(*c));
            if(c)memset(c,0,sizeof(*c));
        }
        uint32_t e=c?CfwStore_Read(0,c->data,sizeof(c->data),&c->bytes):CFW_MEMORY;
        if(!e&&!c->bytes) {
            Cfw_Put32(c->data,MAGIC);
            Cfw_Put32(c->data+4,1);
            c->bytes=8;
        }
        if(!e)e=Valid();
        g_config_store.error=e;
        __DMB();
        g_config_store.ready=1;
        return;
    }
    if(!c||g_config_store.error)return;
    if(c->request) {
        uint32_t e=CfwStore_Result(c->request);
        if(e==CFW_PENDING)return;
        c->request=0;
        g_config_store.last_result=e;
        if(!e) {
            g_config_store.saved_revision=g_config_store.saving_revision;
            g_config_store.last_success_ms=now;
            if(g_config_store.revision!=g_config_store.saved_revision)g_config_store.dirty_since=now;
        }
        else {
            ++g_config_store.failures;
            /* A failed physical transaction is visible and retryable. */
        }
        return;
    }
    if(g_config_store.revision==g_config_store.saved_revision||now-c->attempt_ms<1000U)return;
    if(now-g_config_store.changed_ms<1000U&&now-g_config_store.dirty_since<2000U)return;
    c->attempt_ms=now;
    uint32_t m=Lock(),id;
    uint32_t e=CfwStore_Request(0,c->data,c->bytes,&id);
    if(!e) {
        c->request=id;
        g_config_store.saving_revision=g_config_store.revision;
        g_config_store.last_result=0;
    }
    else if(e!=CFW_BUSY&&e!=CFW_PENDING) {
        /* Admission backpressure has not started a physical transaction.
         * Keep the revision dirty and retry; only real failures are errors. */
        g_config_store.last_result=e;
        ++g_config_store.failures;
    }
    Unlock(m);
}
