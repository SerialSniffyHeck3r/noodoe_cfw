#include "bluetooth_keys.h"
#include "NoodoeBluetooth.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
static Bluetooth_KeyStore store={.version=1U};
static uint32_t replacement;
/* Link key access occurs in the BT task; import/export are protected because
 * the independent storage worker must never observe a half-written key. */
static void Open(void) {}
static void Close(void) {}
static void SetLocal(bd_addr_t addr) { (void)addr; }
static int Get(bd_addr_t addr,link_key_t key,link_key_type_t *type)
{
    for (unsigned i=0;i<6U;i++) if (store.keys[i].valid && !memcmp(addr,store.keys[i].address,6U)) {
        memcpy(key,store.keys[i].key,16U); *type=(link_key_type_t)store.keys[i].type; return 1;
    }
    return 0;
}
static void Put(bd_addr_t addr,link_key_t key,link_key_type_t type)
{
    unsigned slot=6U;
    taskENTER_CRITICAL();
    for (unsigned i=0;i<6U;i++) if (store.keys[i].valid && !memcmp(addr,store.keys[i].address,6U)) { slot=i; break; }
    if (slot==6U) for (unsigned i=0;i<6U;i++) if (!store.keys[i].valid) { slot=i; break; }
    if (slot==6U) { slot=replacement++%6U; }
    memcpy(store.keys[slot].address,addr,6U); memcpy(store.keys[slot].key,key,16U);
    store.keys[slot].type=(uint8_t)type; store.keys[slot].valid=1U;
    store.generation++; g_bluetooth.key_generation=store.generation; g_bluetooth.pairings++;
    taskEXIT_CRITICAL();
}
static void Delete(bd_addr_t addr)
{
    taskENTER_CRITICAL();
    for (unsigned i=0;i<6U;i++) if (store.keys[i].valid && !memcmp(addr,store.keys[i].address,6U)) {
        memset(&store.keys[i],0,sizeof(store.keys[i])); store.generation++;
    }
    g_bluetooth.key_generation=store.generation;
    taskEXIT_CRITICAL();
}
static int IteratorInit(btstack_link_key_iterator_t *it) { it->context=NULL; return 1; }
static int IteratorNext(btstack_link_key_iterator_t *it,bd_addr_t addr,link_key_t key,link_key_type_t *type)
{
    uintptr_t i=(uintptr_t)it->context;
    for (;i<6U;i++) if (store.keys[i].valid) {
        memcpy(addr,store.keys[i].address,6U); memcpy(key,store.keys[i].key,16U);
        *type=(link_key_type_t)store.keys[i].type; it->context=(void *)(i+1U); return 1;
    }
    return 0;
}
static void IteratorDone(btstack_link_key_iterator_t *it) { it->context=NULL; }
static const btstack_link_key_db_t db={Open,SetLocal,Close,Get,Put,Delete,IteratorInit,IteratorNext,IteratorDone};
const btstack_link_key_db_t *Bluetooth_KeyDB(void) { return &db; }
void Bluetooth_ClearKeys(void)
{
    /* No live ACL may republish a deleted key. StorageTask observes this new
     * generation and journals it; persistence is not claimed here. */
    memset(store.keys,0,sizeof(store.keys));replacement=0;
    ++store.generation;g_bluetooth.key_generation=store.generation;
}
uint32_t Bluetooth_BondCount(void)
{
    uint32_t count=0;taskENTER_CRITICAL();
    for(unsigned i=0;i<6U;++i)count+=store.keys[i].valid!=0;
    taskEXIT_CRITICAL();return count;
}
int Bluetooth_ImportKeys(const Bluetooth_KeyStore *keys)
{
    if (!keys || keys->version!=1U || g_bluetooth.state!=BLUETOOTH_STATE_OFF || g_bluetooth.heartbeat) return BLUETOOTH_INVALID;
    for (unsigned i=0;i<6U;i++) if (keys->keys[i].valid>1U || keys->keys[i].type>8U) return BLUETOOTH_INVALID;
    taskENTER_CRITICAL(); store=*keys; g_bluetooth.key_generation=keys->generation;
    g_bluetooth.key_persisted_generation=keys->generation; taskEXIT_CRITICAL(); return 0;
}
int Bluetooth_ExportKeys(Bluetooth_KeyStore *keys)
{
    if (!keys) return BLUETOOTH_INVALID;
    taskENTER_CRITICAL(); *keys=store; taskEXIT_CRITICAL(); return 0;
}
#if NOODOE_BOOTSTRAP
int Bluetooth_RestoreKeys(const Bluetooth_KeyStore *keys)
{
    if(!keys||keys->version!=1)return BLUETOOTH_INVALID;
    for(unsigned i=0;i<6;i++)if(keys->keys[i].valid>1||keys->keys[i].type>8)return BLUETOOTH_INVALID;
    taskENTER_CRITICAL();
    if((int32_t)(keys->generation-store.generation)>0)store.generation=keys->generation;
    for(unsigned i=0;i<6;i++)if(keys->keys[i].valid){
        unsigned slot=6,j;
        for(j=0;j<6;j++){
            if(store.keys[j].valid&&!memcmp(store.keys[j].address,keys->keys[i].address,6))break;
            if(!store.keys[j].valid&&slot==6)slot=j;
        }
        if(j==6&&slot<6){store.keys[slot]=keys->keys[i];++store.generation;}
    }
    g_bluetooth.key_generation=store.generation;taskEXIT_CRITICAL();return 0;
}
#endif
void Bluetooth_MarkKeysPersisted(uint32_t generation)
{
    taskENTER_CRITICAL();
    /* An older save must not falsely clear a key update which arrived later. */
    if (store.generation==generation) g_bluetooth.key_persisted_generation=generation;
    taskEXIT_CRITICAL();
}
