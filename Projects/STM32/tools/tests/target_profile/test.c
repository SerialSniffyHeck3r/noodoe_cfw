#include "Recovery_Core.h"
#include "Recovery_Store.h"
#include <string.h>
volatile uint32_t assertions;
#define CHECK(x) do{assertions++;if(!(x))return __LINE__;}while(0)
static uint8_t lower[65536],header[4096],out[480];
static void W(uint8_t *p,uint32_t n){memcpy(p,&n,4);}
static uint32_t crc(uint8_t *p,uint32_t n){uint32_t c=~0U;while(n--){c^=*p++;for(unsigned i=0;i<8;i++)c=(c>>1)^((0U-(c&1))&0xedb88320U);}return ~c;}
int test_main(void){
 memcpy(lower,(const void*)0x08000000,65536);CHECK(RecoveryTarget_Verify(lower));
 lower[42]^=1;CHECK(!RecoveryTarget_Verify(lower));lower[42]^=1;
 W(lower+0x8000,0xF0000);memcpy(lower+0xc022,"SR0701",6);lower[0xc080]=4;
 CHECK(RecoveryTarget_Verify(lower)); // synthetic0.15 factory profile only
 CHECK(RecoveryTarget_Export(lower,65535,1,out)&&out[0]==lower[65535]);
 CHECK(!RecoveryTarget_Export(lower,0xffffffff,2,out));CHECK(!RecoveryTarget_Export(lower,65535,2,out));
 CHECK(!RecoveryTarget_Export(lower,65536,1,out));CHECK(!RecoveryTarget_Export(lower,0,481,out));CHECK(!RecoveryTarget_Export(lower,0,0,out));
 uint32_t uid[3]={1,2,3};memset(header,255,sizeof(header));
 uint32_t h[10]={0x3152434e,2,4096,0x80000,1,2,3,RECOVERY_STOCK_VERSION,0x70000,0xf0000};
 memcpy(header,h,sizeof(h));memcpy(header+40,recovery_stock_sha256,32);RecoveryTarget_Hash(lower,header+72);
 W(header+4088,crc(header,4088));W(header+4092,0x31544d43);
 CHECK(!RecoveryStore_CheckHeader(header,uid));CHECK(!RecoveryStore_CheckTarget(header,lower));
 lower[43]^=1;CHECK(RecoveryStore_CheckTarget(header,lower));lower[43]^=1;
 lower[0xc080]=3;CHECK(!RecoveryTarget_Verify(lower));lower[0xc080]=4;
 lower[0xc022]='X';CHECK(!RecoveryTarget_Verify(lower));lower[0xc022]='S';
 W(lower+4,0x08010001);CHECK(!RecoveryTarget_Verify(lower));
 W(header+4,1);W(header+4088,crc(header,4088));CHECK(RecoveryStore_CheckHeader(header,uid));
 memcpy(lower,(const void*)0x08000000,65536);W(header+36,0xe0000);memset(header+72,255,32);W(header+4088,crc(header,4088));
 CHECK(!RecoveryStore_CheckHeader(header,uid));CHECK(!RecoveryStore_CheckTarget(header,lower));
 uid[0]=4;CHECK(RecoveryStore_CheckHeader(header,uid));
 return 0;
}
