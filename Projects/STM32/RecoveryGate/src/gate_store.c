#include "gate_store.h"
#include <string.h>
static const char names[GATE_FILE_COUNT][12]={"CFWA    DAT","CFWB    DAT","CFWBOOT DAT","CFWREC  DAT","NOODOE  RSC","CFWLOG  DAT"
#if NOODOE_UNINSTALL
 ,"CFWCFG  DAT","CFWRIDE DAT","CFWPIC  DAT","CFWTEXT DAT"
#endif
};
static const uint32_t sizes[GATE_FILE_COUNT]={0x80000,0x80000,0x10000,0x80000,0x100000,0x40000
#if NOODOE_UNINSTALL
 ,0x20000,0x40000,0x100000,0x20000
#endif
};
static uint32_t U16(const uint8_t *p){return p[0]|((uint32_t)p[1]<<8);}
static uint32_t U32(const uint8_t *p){return U16(p)|(U16(p+2)<<16);}
static uint32_t Fail(GateStore *s,uint32_t e){s->state=GATE_STORE_ERROR;s->error=e;return s->state;}
static uint32_t Fat(GateStore *s,uint32_t c){uint32_t n=U16(s->fat+c+c/2);return c&1?n>>4:n&4095;}
static uint32_t Address(uint32_t c){return 0x9000U+(c-2)*32768U;}
static uint32_t Logical(GateStore *s,uint32_t address,void *out,uint32_t n)
{
    if(!s->read_raw||!out||!n||(address|n)&1U||address>=0x8000000U||n>0x8000000U-address)return RECOVERY_STORE_BOUNDS;
    uint8_t *p=out;if(s->read_raw(s->io,address,out,n))return RECOVERY_STORE_IO;
    for(uint32_t i=0;i<n;i+=2){uint8_t a=p[i];p[i]=p[i+1];p[i+1]=a;}return 0;
}
void GateStore_Init(GateStore *s,RecoveryStoreRawRead read,void *io,const uint32_t uid[3])
{
    if(!s)return;
    memset(s,0,sizeof(*s));s->read_raw=read;s->io=io;
    if(!read||!uid){Fail(s,RECOVERY_STORE_BOUNDS);return;}
    memcpy(s->uid,uid,12);s->state=GATE_STORE_AUDIT;
}
/* Every file and directory claims its complete chain. Duplicate ownership,
 * loops, free-in-chain and orphan allocation fail closed before image use. */
static uint32_t Entry(GateStore *s,const uint8_t *p)
{
    if(p[0]==0xe5||p[11]==15||(p[11]&8)||p[0]=='.')return 1;
    if(U16(p+20)||(p[11]&0xc0))return 0;
    uint32_t first=U16(p+26),c=first,n=U32(p+28),count=0,rec=0;
    if(s->root)for(uint32_t f=0;f<GATE_FILE_COUNT;f++)if(!memcmp(p,names[f],11)){
        if((s->found&(1U<<f))||(p[11]&16)||n!=sizes[f])return 0;
        s->found|=1U<<f;rec=f+1;
#if NOODOE_UNINSTALL
        s->root_entry[f]=s->sector*128+(uint32_t)(p-s->block)/32;
#endif
    }
    if(!first)return !n&&!(p[11]&16)&&!rec;
    while(c<0xff8){
        if(c<2||c>=4080||(s->claimed[c/8]&(1U<<(c&7))))return 0;
        s->claimed[c/8]|=(uint8_t)(1U<<(c&7));
        if(rec){if(count>=sizes[rec-1]/32768||Address(c)>RECOVERY_STORE_SAFE_END-32768)return 0;s->map[rec-1][count]=Address(c);}
        ++count;c=Fat(s,c);
    }
    if(!(p[11]&16)&&count!=(n+32767U)/32768U)return 0;
    if(p[11]&16){if(s->tail>=4080)return 0;s->directories[s->tail++]=(uint16_t)first;}
    return 1;
}
static void Next(GateStore *s){s->root=0;s->sector=0;if(s->head<s->tail)s->current=s->directories[s->head++];else s->phase=4;}
uint32_t GateStore_Process(GateStore *s)
{
    if(!s)return GATE_STORE_ERROR;
    if(s->state!=GATE_STORE_AUDIT)return s->state;
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
        s->phase=5;s->sector=0;return s->state;
    }
    if(s->phase==5){uint32_t slot=s->sector++;
        if((s->found&(1U<<slot))&&!Logical(s,s->map[slot][15]+0x7000,s->block,4096)&&GateIdentity_Decode(s->block,s->uid,slot))s->identity|=1U<<slot;
        if(s->sector==2){s->state=GATE_STORE_READY;}return s->state;}
    return Fail(s,RECOVERY_STORE_FORMAT);
}
uint32_t GateStore_Address(const GateStore *s,uint32_t file,uint32_t off,uint32_t *address)
{if(!s||s->state!=GATE_STORE_READY||file>=GATE_FILE_COUNT||!(s->found&(1U<<file))||(file<2&&!(s->identity&(1U<<file)))||off>=sizes[file]||!address)return 1;
 *address=s->map[file][off/32768]+off%32768;return 0;}
uint32_t GateStore_Read(GateStore *s,uint32_t file,uint32_t off,void *destination,uint32_t n)
{uint8_t *out=destination;if(!out||!n||file>=GATE_FILE_COUNT||off>=sizes[file]||n>sizes[file]-off)return 1;
 while(n){uint32_t address,k=32768-off%32768;if(GateStore_Address(s,file,off,&address))return 1;if(k>n)k=n;
  if(address&1){uint8_t pair[2];if(Logical(s,address-1,pair,2))return 1;*out++=pair[1];++off;--n;}
  else if(k>=2){k&=~1U;if(Logical(s,address,out,k))return 1;off+=k;out+=k;n-=k;}
  else{uint8_t pair[2];if(Logical(s,address,pair,2))return 1;*out++=pair[0];++off;--n;}
 }return 0;}
