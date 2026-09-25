#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "RAM codec requires little-endian target"
#endif
#include "Noodoe_Crc32.h"
#include "settings_record.h"
#include <string.h>
#define MAGIC 0x31544553U
/* Exact approved layout, including duplicated adjacent boundaries. This makes
 * a future repartition/schema change opt-in instead of silently accepting old
 * settings and granting writes to a differently interpreted medium. */
static const uint32_t layout[7]={0x07F70000U,0x07F70000U,0x07F80000U,0x07F80000U,0x07F90000U,0x07F90000U,0x08000000U};
static uint32_t Get(const uint8_t *p){uint32_t v;memcpy(&v,p,4);return v;}
/* Preserve the explicit LE32 wire bytes without a byte-store loop at every
 * call site. memcpy also preserves the contract for unaligned RAM buffers. */
#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "This scalar codec requires a little-endian target"
#endif
static void Put(uint8_t *p,uint32_t v){memcpy(p,&v,4);}
static uint32_t Crc(const uint8_t *p,uint32_t length)
{return Noodoe_Crc32(p,length);}
uint32_t SettingsRecord_ValuesValid(const Settings_Values *values)
{
    if(!values || values->backlight_percent>100U || values->units>1U || values->oil_usage_valid>1U || (!values->oil_usage_valid&&values->oil_on_ms))return 0U;
    uint32_t name_bytes=0;while(name_bytes<RIDER_NAME_CAPACITY&&values->rider_name[name_bytes])++name_bytes;
    if(!RiderName_Validate(values->rider_name,name_bytes))return 0;
    for(uint32_t role=0;role<2U;++role){const Settings_RoleBinding *r=&values->roles[role];
        if(r->enabled>1U || r->channel>30U)return 0U;
        uint32_t nonzero=0U,not_ff=0U;for(uint32_t i=0;i<6U;++i){nonzero|=r->address[i];not_ff|=r->address[i]^0xFFU;}
        if(r->enabled && (!nonzero || !not_ff))return 0U;
    }
    return 1U;
}
static uint32_t KeysValid(const Bluetooth_KeyStore *keys)
{
    if(keys->version!=1U)return 0U;
    for(uint32_t i=0;i<6U;++i)if(keys->keys[i].valid>1U || keys->keys[i].type>8U)return 0U;
    return 1U;
}
uint32_t SettingsRecord_Encode(uint8_t out[SETTINGS_RECORD_BYTES],const Settings_Record *r,const uint32_t uid[3])
{
    if(!out || !r || !uid || !SettingsRecord_ValuesValid(&r->values) || !KeysValid(&r->keys))return 0U;
    memset(out,0,SETTINGS_RECORD_BYTES);Put(out,MAGIC);Put(out+4U,3U);Put(out+8U,SETTINGS_RECORD_BYTES);Put(out+12U,1U);
    for(uint32_t i=0;i<3U;++i)Put(out+16U+4U*i,uid[i]);
    for(uint32_t i=0;i<7U;++i)Put(out+28U+4U*i,layout[i]);
    Put(out+56U,1U);Put(out+60U,r->generation);Put(out+64U,r->values.backlight_percent);Put(out+68U,r->values.units);
    for(uint32_t i=0;i<2U;++i){memcpy(out+72U+i*8U,r->values.roles[i].address,6U);out[78U+i*8U]=r->values.roles[i].channel;out[79U+i*8U]=r->values.roles[i].enabled;}
    Put(out+88U,r->keys.version);Put(out+92U,r->keys.generation);
    for(uint32_t i=0;i<6U;++i){const Bluetooth_LinkKey *k=&r->keys.keys[i];uint8_t *p=out+96U+i*24U;
        memcpy(p,k->address,6U);memcpy(p+6U,k->key,16U);p[22U]=k->type;p[23U]=k->valid;
    }
    Put(out+240U,(uint32_t)r->values.oil_on_ms);Put(out+244U,(uint32_t)(r->values.oil_on_ms>>32));
    Put(out+248U,r->values.oil_usage_valid);
    /* Keep the old reserved word252 zero. Name256..304 is NUL terminated;
     * padding305..307 is zero and CRC308 covers all bytes, including padding. */
    uint32_t n=0;while(r->values.rider_name[n])++n;
    memcpy(out+256U,r->values.rider_name,n);Put(out+308U,Crc(out,308U));return 1U;
}
uint32_t SettingsRecord_Decode(Settings_Record *out,const uint8_t *data,uint32_t length,const uint32_t uid[3])
{
    if(!out || !data || !uid || (length!=SETTINGS_RECORD_BYTES&&length!=SETTINGS_RECORD_V2_BYTES&&length!=SETTINGS_RECORD_LEGACY_BYTES))return 0U;
    uint32_t legacy=length==SETTINGS_RECORD_LEGACY_BYTES,crc_offset=length-4U;
    uint32_t version=legacy?1U:length==SETTINGS_RECORD_V2_BYTES?2U:3U;
    if(Get(data)!=MAGIC || Get(data+4U)!=version || Get(data+8U)!=length || Get(data+12U)!=1U || Get(data+56U)!=1U || Get(data+crc_offset)!=Crc(data,crc_offset))return 0U;
    for(uint32_t i=0;i<3U;++i)if(Get(data+16U+i*4U)!=uid[i])return 0U;
    for(uint32_t i=0;i<7U;++i)if(Get(data+28U+i*4U)!=layout[i])return 0U;
    /* Local candidate avoids partially publishing keys/values when any later
     * field is invalid. This bounded parsing frame is task-only. */
    Settings_Record r;memset(&r,0,sizeof(r));r.generation=Get(data+60U);
    r.values.backlight_percent=Get(data+64U);r.values.units=Get(data+68U);
    for(uint32_t i=0;i<2U;++i){memcpy(r.values.roles[i].address,data+72U+i*8U,6U);r.values.roles[i].channel=data[78U+i*8U];r.values.roles[i].enabled=data[79U+i*8U];}
    r.keys.version=Get(data+88U);r.keys.generation=Get(data+92U);
    for(uint32_t i=0;i<6U;++i){Bluetooth_LinkKey *k=&r.keys.keys[i];const uint8_t *p=data+96U+i*24U;
        memcpy(k->address,p,6U);memcpy(k->key,p+6U,16U);k->type=p[22U];k->valid=p[23U];
    }
    /* Older records restore preferences/keys unchanged, but invent no hours.
     * They remain read-only until a normal authorized checkpoint writes v2. */
    if(!legacy){
        if(Get(data+252U))return 0U;
        r.values.oil_on_ms=(uint64_t)Get(data+240U)|((uint64_t)Get(data+244U)<<32);
        r.values.oil_usage_valid=Get(data+248U);
    }
    if(version==3U){
        memcpy(r.values.rider_name,data+256U,RIDER_NAME_CAPACITY);
        if(data[305U]||data[306U]||data[307U])return 0;
        /* Reject noncanonical tail bytes after NUL, even with a valid CRC. */
        uint32_t ended=0;
        for(uint32_t i=0;i<RIDER_NAME_CAPACITY;++i){if(ended&&r.values.rider_name[i])return 0;if(!r.values.rider_name[i])ended=1;}
    }
    if(!SettingsRecord_ValuesValid(&r.values) || !KeysValid(&r.keys))return 0U;
    *out=r;return 1U;
}
