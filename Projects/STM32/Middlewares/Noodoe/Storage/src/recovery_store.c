#include "Noodoe_Crc32.h"
#include "Recovery_Store.h"
#include "Recovery_Core.h"
#include <string.h>
static uint32_t U16(const uint8_t *p){uint16_t value;memcpy(&value,p,2);return value;}
static uint32_t U32(const uint8_t *p){return U16(p)|(U16(p+2)<<16);}
static uint32_t CRC(const uint8_t *p,uint32_t n){return Noodoe_Crc32(p,n);}
/* These shared audit boundaries must stay out of the multiple caller state
 * machines under LTO; inlining duplicates the complete format/hash checks. */
__attribute__((noinline)) uint32_t RecoveryStore_CheckHeader(const uint8_t *p,const uint32_t uid[3])
{
    if(!p||!uid||U32(p)!=0x3152434eU||(U32(p+4)!=1&&U32(p+4)!=2)||U32(p+8)!=4096||
       U32(p+12)!=RECOVERY_STORE_BYTES||U32(p+28)!=0x00100005U||
       U32(p+32)!=RECOVERY_IMAGE_BYTES||!RECOVERY_VERSION_SUPPORTED(U32(p+36))||
       U32(p+4092)!=0x31544d43U||CRC(p,4088)!=U32(p+4088)||
       memcmp(p+40,recovery_stock_sha256,32))return RECOVERY_STORE_FORMAT;
    for(uint32_t i=0;i<3;++i)if(U32(p+16+4*i)!=uid[i])return RECOVERY_STORE_UID;
    if(U32(p+4)==1&&U32(p+36)!=0x000e0000U)return RECOVERY_STORE_FORMAT;
    for(uint32_t i=U32(p+4)==1?72U:104U;i<4088;++i)if(p[i]!=255)return RECOVERY_STORE_FORMAT;
    return 0;
}
/* Compare a valid header with physical resident bytes, never a host assertion. */
__attribute__((noinline)) uint32_t RecoveryStore_CheckTarget(const uint8_t *p,const uint8_t lower[65536])
{
 if(!p||!lower||U32(p+36)!=U32(lower+0x8000)||!RecoveryTarget_Verify(lower))return RECOVERY_STORE_HASH;
 if(U32(p+4)==1)return U32(p+36)==0x000e0000U?0:RECOVERY_STORE_FORMAT;
 if(U32(p+4)!=2)return RECOVERY_STORE_FORMAT;
 uint8_t sha[32];RecoveryTarget_Hash(lower,sha);return memcmp(sha,p+72,32)?RECOVERY_STORE_HASH:0;
}
static uint32_t Fail(RecoveryStore *s,uint32_t e){s->state=RECOVERY_STORE_FAILED;s->error=e;return s->state;}
static uint32_t Fat(RecoveryStore *s,uint32_t c){uint32_t n=U16(s->fat+c+c/2);return c&1?n>>4:n&4095;}
static uint32_t Address(uint32_t c){return 0x9000U+(c-2)*32768U;}
static uint32_t Logical(RecoveryStore *s,uint32_t address,void *out,uint32_t n)
{
    if(!s->read_raw||!out||!n||(address|n)&1U||address>=0x8000000U||n>0x8000000U-address)return RECOVERY_STORE_BOUNDS;
    uint8_t *p=out;if(s->read_raw(s->io,address,out,n))return RECOVERY_STORE_IO;
    for(uint32_t i=0;i<n;i+=2){uint8_t a=p[i];p[i]=p[i+1];p[i+1]=a;}return 0;
}
void RecoveryStore_Init(RecoveryStore *s,RecoveryStoreRawRead read,void *io,const uint32_t uid[3])
{
    if(!s)return;
    memset(s,0,sizeof(*s));s->read_raw=read;s->io=io;
    if(!read||!uid){Fail(s,RECOVERY_STORE_BOUNDS);return;}
    memcpy(s->uid,uid,12);s->state=RECOVERY_STORE_AUDITING;
}
/* Every file and directory claims its complete chain. Duplicate ownership,
 * loops, free-in-chain and orphan allocation fail closed before image use. */
static uint32_t Entry(RecoveryStore *s,const uint8_t *p)
{
    if(p[0]==0xe5||p[11]==15||(p[11]&8)||p[0]=='.')return 1;
    if(U16(p+20)||(p[11]&0xc0))return 0;
    uint32_t first=U16(p+26),c=first,n=U32(p+28),count=0,rec=0;
    if(s->root&&!memcmp(p,"CFWREC  DAT",11)){
        if(s->found||(p[11]&16)||n!=RECOVERY_STORE_BYTES)return 0;
        s->found=rec=1;
    }
    if(!first)return !n&&!(p[11]&16)&&!rec;
    while(c<0xff8){
        if(c<2||c>=4080||(s->claimed[c/8]&(1U<<(c&7))))return 0;
        s->claimed[c/8]|=(uint8_t)(1U<<(c&7));
        if(rec){if(count>=16||Address(c)>RECOVERY_STORE_SAFE_END-32768)return 0;s->image_map[count]=Address(c);}
        ++count;c=Fat(s,c);
    }
    if(!(p[11]&16)&&count!=(n+32767U)/32768U)return 0;
    if(p[11]&16){if(s->tail>=4080)return 0;s->directories[s->tail++]=(uint16_t)first;}
    return 1;
}
static void Next(RecoveryStore *s){s->root=0;s->sector=0;if(s->head<s->tail)s->current=s->directories[s->head++];else s->phase=4;}
uint32_t RecoveryStore_Process(RecoveryStore *s)
{
    if(!s)return RECOVERY_STORE_FAILED;
    if(s->state!=RECOVERY_STORE_AUDITING&&s->state!=RECOVERY_STORE_HASHING)return s->state;
    if(s->phase==0){
        if(Logical(s,0,s->block,4096))return Fail(s,RECOVERY_STORE_IO);
        uint8_t *b=s->block;
        if(U16(b+11)!=4096||b[13]!=8||U16(b+14)!=1||b[16]!=2||U16(b+17)!=512||U16(b+22)!=2||U32(b+32)!=0x7f80||b[510]!=0x55||b[511]!=0xaa)return Fail(s,RECOVERY_STORE_FORMAT);
        s->phase=1;s->sector=0;return s->state;
    }
    if(s->phase==1){
        if(Logical(s,(1+s->sector)*4096,s->fat+s->sector*4096,4096))return Fail(s,RECOVERY_STORE_IO);
        if(++s->sector==2){s->phase=2;s->sector=0;}return s->state;
    }
    if(s->phase==2){
        if(Logical(s,(3+s->sector)*4096,s->block,4096))return Fail(s,RECOVERY_STORE_IO);
        if(memcmp(s->block,s->fat+s->sector*4096,4096))return Fail(s,RECOVERY_STORE_FORMAT);
        if(++s->sector==2){if(Fat(s,0)<0xff0||Fat(s,1)<0xff8)return Fail(s,RECOVERY_STORE_FORMAT);s->phase=3;s->sector=0;s->root=1;}return s->state;
    }
    if(s->phase==3){
        uint32_t address=s->root?0x5000+s->sector*4096:Address(s->current)+s->sector*4096;
        if(Logical(s,address,s->block,4096))return Fail(s,RECOVERY_STORE_IO);
        for(uint32_t i=0;i<4096;i+=32){if(!s->block[i]){Next(s);return s->state;}if(!Entry(s,s->block+i))return Fail(s,RECOVERY_STORE_FORMAT);}
        if(++s->sector==(s->root?4U:8U)){s->sector=0;if(s->root)Next(s);else{uint32_t c=Fat(s,s->current);if(c>=0xff8)Next(s);else s->current=c;}}return s->state;
    }
    if(s->phase==4){
        for(uint32_t c=2;c<4080;++c)if(Fat(s,c)&&Fat(s,c)!=0xff7&&!(s->claimed[c/8]&(1U<<(c&7))))return Fail(s,RECOVERY_STORE_FORMAT);
        if(!s->found){s->state=RECOVERY_STORE_MISSING;return s->state;}
        if(Logical(s,s->image_map[0],s->block,4096))return Fail(s,RECOVERY_STORE_IO);
        uint32_t e=RecoveryStore_CheckHeader(s->block,s->uid);if(e)return Fail(s,e);
        if(s->resident)e=RecoveryStore_CheckTarget(s->block,s->resident);
        else if(U32(s->block+4)==2)e=RECOVERY_STORE_HASH;
        if(e)return Fail(s,e);
        s->phase=5;s->state=RECOVERY_STORE_HASHING;s->position=0;UpdateSha256_Init(&s->sha);return s->state;
    }
    uint32_t off=4096+s->position;
    if(Logical(s,s->image_map[off/32768]+off%32768,s->block,4096))return Fail(s,RECOVERY_STORE_IO);
    UpdateSha256_Feed(&s->sha,s->block,4096);s->position+=4096;
    if(s->position==RECOVERY_IMAGE_BYTES){uint8_t digest[32];UpdateSha256_Final(&s->sha,digest);if(memcmp(digest,recovery_stock_sha256,32))return Fail(s,RECOVERY_STORE_HASH);s->state=RECOVERY_STORE_READY;}
    return s->state;
}
/* Arbitrary byte slices use a tiny pair bounce, keeping the public source API
 * canonical without SDRAM or a shared 4KiB scratch buffer. */
uint32_t RecoveryStore_Read(void *context,uint32_t off,void *destination,uint32_t n)
{
    RecoveryStore *s=context;uint8_t *out=destination;
    if(!s||s->state!=RECOVERY_STORE_READY||!out||!n||off>=RECOVERY_IMAGE_BYTES||n>RECOVERY_IMAGE_BYTES-off)return RECOVERY_STORE_BOUNDS;
    off+=4096;
    while(n){uint32_t address=s->image_map[off/32768]+off%32768,k=32768-off%32768;
        if(k>n)k=n;
        if(address&1){uint8_t pair[2];if(Logical(s,address-1,pair,2))return RECOVERY_STORE_IO;*out++=pair[1];++off;--n;continue;}
        if(k>=2){k&=~1U;if(Logical(s,address,out,k))return RECOVERY_STORE_IO;off+=k;out+=k;n-=k;}
        else{uint8_t pair[2];if(Logical(s,address,pair,2))return RECOVERY_STORE_IO;*out++=pair[0];++off;--n;}
    }return 0;
}
