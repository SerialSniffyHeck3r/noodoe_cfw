#include "RadioSelfTest.h"
#include "Noodoe_Crc32.h"
#include <string.h>
RadioSelfTestState g_radio_self_test;
static uint32_t U(const uint8_t *p){uint32_t v;memcpy(&v,p,4);return v;}
uint32_t RadioSelfTest_Handle(const uint8_t *p,uint32_t n,uint32_t epoch,uint32_t now,uint8_t *out,uint32_t *bytes)
{
    *bytes=0;if(n<8||!epoch)return 1;uint32_t op=U(p),nonce=U(p+4);
    RadioSelfTestState *s=&g_radio_self_test;
    if(op==1){if(n!=8||!nonce)return 1;*s=(RadioSelfTestState){.epoch=epoch,.nonce=nonce,.started=now,.crc=0xffffffffU};}
    else {
        if(s->epoch!=epoch||s->nonce!=nonce||now-s->started>30000U){++s->errors;return 7;}
        if(op==2){
            if(n<13||n>524||U(p+8)!=s->bytes||n-12>8192-s->bytes||s->complete){++s->errors;return 1;}
            for(uint32_t i=0;i<n-12;i++)if(p[12+i]!=(uint8_t)((s->bytes+i)*73U+nonce)){++s->errors;return 6;}
            s->crc=Noodoe_Crc32Feed(s->crc,p+12,n-12);s->bytes+=n-12;
            memcpy(out,p+12,n-12);*bytes=n-12;return 0;
        }
        if(op!=3||n!=8||s->bytes!=8192){++s->errors;return 1;}
        s->complete=1;s->elapsed=now-s->started;
    }
    uint32_t result[]={1,s->bytes,s->crc^0xffffffffU,s->elapsed,s->complete,s->errors};
    memcpy(out,result,sizeof(result));*bytes=sizeof(result);return 0;
}
