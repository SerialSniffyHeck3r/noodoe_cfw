#include "Update_Service.h"
#include <string.h>
/* FIPS180-4 SHA256의32-bit round constants다. 외부 framework 없이 hash만
 * 구현하며 caller 소유 state를 사용한다. 인증/서명 검증을 대신하지 않는다. */
static const uint32_t k[64]={
0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
/* SHA rotation은0이아닌 고정 n만 사용한다. */
static uint32_t R(uint32_t v,uint32_t n){return (v>>n)|(v<<(32U-n));}
/* 64-byte block을 big-endian word로 확장하고 현재 chaining state에 더한다. */
static void Block(UpdateSha256 *s)
{
    uint32_t w[64],a,b,c,d,e,f,g,h,i,t1,t2;
    for(i=0;i<16U;++i){const uint8_t *p=s->block+4U*i;w[i]=((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
    for(i=16U;i<64U;++i)w[i]=w[i-16U]+(R(w[i-15U],7)^R(w[i-15U],18)^(w[i-15U]>>3))+w[i-7U]+(R(w[i-2U],17)^R(w[i-2U],19)^(w[i-2U]>>10));
    a=s->words[0];b=s->words[1];c=s->words[2];d=s->words[3];e=s->words[4];f=s->words[5];g=s->words[6];h=s->words[7];
    for(i=0;i<64U;++i){t1=h+(R(e,6)^R(e,11)^R(e,25))+((e&f)^((~e)&g))+k[i]+w[i];t2=(R(a,2)^R(a,13)^R(a,22))+((a&b)^(a&c)^(b&c));h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;}
    s->words[0]+=a;s->words[1]+=b;s->words[2]+=c;s->words[3]+=d;s->words[4]+=e;s->words[5]+=f;s->words[6]+=g;s->words[7]+=h;
}
/* 신규448KiB 이미지의 byte stream을 시작한다. total은이서비스상한내에서32bit다. */
void UpdateSha256_Init(UpdateSha256 *s)
{ static const uint32_t initial[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};if(!s)return;memset(s,0,sizeof(*s));memcpy(s->words,initial,sizeof(initial)); }
/* 입력 조각은 임의 길이이며64-byte block 경계에서만 압축 함수를 실행한다. */
void UpdateSha256_Feed(UpdateSha256 *s,const uint8_t *data,size_t n)
{ size_t i;if(!s||(!data&&n))return;for(i=0;i<n;++i){s->block[s->used++]=data[i];++s->total;if(s->used==64U){Block(s);s->used=0U;}} }
/* 원래 byte길이를64-bit bit길이로append하고digest32byte를내보낸다. Final후
 * 같은context를다시Feed하지않으며다음이미지는Init으로재시작한다. */
void UpdateSha256_Final(UpdateSha256 *s,uint8_t out[32])
{
    uint64_t bits;uint32_t i;
    if(!s||!out)return;
    bits=(uint64_t)s->total*8U;s->block[s->used++]=0x80U;
    if(s->used>56U){while(s->used<64U)s->block[s->used++]=0;Block(s);s->used=0U;}
    while(s->used<56U)s->block[s->used++]=0;
    for(i=0;i<8U;++i)s->block[56U+i]=(uint8_t)(bits>>(56U-i*8U));
    Block(s);
    for(i=0;i<32U;++i)out[i]=(uint8_t)(s->words[i/4U]>>(24U-(i%4U)*8U));
}
