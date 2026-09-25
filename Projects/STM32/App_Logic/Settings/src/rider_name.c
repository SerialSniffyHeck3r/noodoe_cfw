#include "Rider_Name.h"
#include "SettingsService.h"
#include "stm32f4xx_hal.h"
#include <string.h>

static char session_name[RIDER_NAME_CAPACITY];
static uint32_t session_override;

/* Reject truncation, overlong encodings, surrogate code points and controls
 * before publishing anything. The source is borrowed only for this call. */
uint32_t RiderName_Validate(const char *text,uint32_t bytes)
{
    if(bytes>RIDER_NAME_MAX_BYTES||(!text&&bytes))return 0;
    const uint8_t *p=(const uint8_t*)text;
    for(uint32_t i=0;i<bytes;){
        uint32_t c=p[i++],extra=0,min=0;
        if(c<0x80U){}
        else if(c>=0xC2U&&c<=0xDFU){extra=1;min=0x80U;c&=0x1FU;}
        else if(c>=0xE0U&&c<=0xEFU){extra=2;min=0x800U;c&=0x0FU;}
        else if(c>=0xF0U&&c<=0xF4U){extra=3;min=0x10000U;c&=7U;}
        else return 0;
        if(extra>bytes-i)return 0;
        while(extra--){uint32_t b=p[i++];if((b&0xC0U)!=0x80U)return 0;c=(c<<6)|(b&0x3FU);}
        if(c<min||c>0x10FFFFU||(c>=0xD800U&&c<=0xDFFFU)||
           c<0x20U||(c>=0x7FU&&c<=0x9FU)||c==0x2028U||c==0x2029U)return 0;
    }
    return 1;
}

/* Validate/copy before masking IRQs; the critical section contains only a
 * fixed49-byte publication. The foreground renderer cannot see a torn name. */
uint32_t RiderName_Set(const char *text,uint32_t bytes)
{
    if(__get_IPSR()||__get_PRIMASK()||__get_BASEPRI()||!RiderName_Validate(text,bytes))return 0;
    char next[RIDER_NAME_CAPACITY]={0};if(bytes)memcpy(next,text,bytes);
    uint32_t mask=__get_PRIMASK();__disable_irq();
    memcpy(session_name,next,sizeof(next));session_override=1;
    __DMB();__set_PRIMASK(mask);return 1;
}

/* Startup does not manufacture a name or depend on a Bluetooth connection.
 * An explicitly set session name wins over a later storage initialization. */
uint32_t RiderName_Get(char *out,uint32_t capacity)
{
    if(!out||capacity<RIDER_NAME_CAPACITY)return 0;
    uint32_t mask=__get_PRIMASK();__disable_irq();
    if(session_override){memcpy(out,session_name,sizeof(session_name));__DMB();__set_PRIMASK(mask);return 1;}
    __set_PRIMASK(mask);
    (void)SettingsService_GetRiderName(out,capacity);return 1;
}
