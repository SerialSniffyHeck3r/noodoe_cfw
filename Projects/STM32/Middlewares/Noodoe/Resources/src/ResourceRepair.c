#include "ResourceRepair.h"
#include "ResourceStore.h"
#include "Resources.h"
#include "BSP_NOR.h"
#include "BSP_Power.h"
#include "Update_Service.h"
#include <string.h>
/* Repair is a reviewed physical-sector transaction, not a filesystem bypass.
 * SHA of the entire request, every preimage and every replacement is checked
 * before the first erase. Metadata publication order is supplied by the host's
 * fully validated recovery graph. The complete A/B backup remains mandatory. */
#define RECORD_BYTES 4164U
static uint8_t *input,*scratch,skip[120];
static uint32_t phase,count,position,total;
static UpdateSha256 hash;
static uint32_t U32(const uint8_t *p){uint32_t v;memcpy(&v,p,4);return v;}
static void Digest(const void *data,uint32_t n,uint8_t result[32])
{UpdateSha256 h;UpdateSha256_Init(&h);UpdateSha256_Feed(&h,data,n);UpdateSha256_Final(&h,result);}
uint32_t ResourceRepair_Begin(uint8_t *data,uint32_t capacity,uint8_t *work)
{
    if(!data||!work||Resources_GetStatus()==RESOURCES_READY||U32(data)!=0x31505253||U32(data+4)!=1)return 200;
    count=U32(data+8);total=U32(data+12);
    if(!count||count>120||total!=count*RECORD_BYTES||total>capacity-64)return 201;
    input=data;scratch=work;position=0;phase=1;memset(skip,0,sizeof(skip));UpdateSha256_Init(&hash);return 0;
}
uint32_t ResourceRepair_Process(void)
{
    if(!g_bsp_power.ign_valid||!g_bsp_power.ign_on)return 202;
    if(phase==1){
        uint32_t n=total-position;if(n>4096)n=4096;
        UpdateSha256_Feed(&hash,input+64+position,n);position+=n;if(position<total)return 0;
        uint8_t result[32];UpdateSha256_Final(&hash,result);if(memcmp(result,input+16,32))return 203;
        phase=2;position=0;return 0;
    }
    if(phase==2){
        uint8_t *r=input+64+position*RECORD_BYTES;uint32_t a=U32(r);uint8_t actual[32];
        if(a<4096||(a&4095)||a>0x07F70000U-4096)return 204;
        for(uint32_t i=0;i<position;++i)if(U32(input+64+i*RECORD_BYTES)==a)return 205;
        Digest(r+68,4096,actual);if(memcmp(actual,r+36,32))return 206;
        if(BSP_NOR_Read(a,scratch,4096)!=BSP_NOR_OK)return 207;
        Digest(scratch,4096,actual);
        if(!memcmp(actual,r+36,32))skip[position]=1; /* Exact completed replay. */
        else if(memcmp(actual,r+4,32))return 208;
        else if(a>=0x9000){
            /* Reconstruction may publish new data only in proven blank space;
             * no recovery request can replace an existing photo/file body. */
            uint8_t fill=scratch[0];if(fill!=0&&fill!=255)return 217;
            for(uint32_t i=1;i<4096;++i)if(scratch[i]!=fill)return 217;
        }
        if(++position<count)return 0;
        if(BSP_NOR_UnlockStorage(0x42414B32)!=BSP_NOR_OK)return 209;
        phase=3;position=0;return 0;
    }
    if(phase==3){
        uint8_t *r=input+64+position*RECORD_BYTES;uint32_t a=U32(r);uint8_t actual[32];
        if(!skip[position]){
            /* Recheck immediately before erase; this also detects an unexpected
             * writer between transaction preflight and sector publication. */
            if(BSP_NOR_Read(a,scratch,4096)!=BSP_NOR_OK)return 210;
            Digest(scratch,4096,actual);if(memcmp(actual,r+4,32))return 211;
            Digest(r+68,4096,actual);if(memcmp(actual,r+36,32))return 212;
            if(BSP_NOR_Erase4K(a)!=BSP_NOR_OK)return 213;
            for(uint32_t p=0;p<4096;p+=256)if(BSP_NOR_Program(a+p,r+68+p,256)!=BSP_NOR_OK)return 214;
            if(BSP_NOR_Read(a,scratch,4096)!=BSP_NOR_OK||memcmp(scratch,r+68,4096))return 215;
        }
        g_resource_install.progress=(position+1)*4096;
        return ++position<count?0:1;
    }
    return 216;
}
