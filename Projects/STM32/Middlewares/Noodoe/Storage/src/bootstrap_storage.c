#include "Noodoe_Crc32.h"
#include "Bootstrap_Storage.h"
#include "Bootstrap_Resources.h"
#include "Resources.h"
#include "gate_abi.h"
#include "event_log.h"
#include <string.h>
static const char names[BS_FILE_COUNT][12]={"NOODOE  RSC","CFWCFG  DAT","CFWRIDE DAT","CFWPIC  DAT","CFWREC  DAT","CFWA    DAT","CFWB    DAT","CFWBOOT DAT","CFWLOG  DAT"};
static const uint32_t sizes[BS_FILE_COUNT]={1048576,131072,262144,1048576,524288,524288,524288,65536,262144};
volatile BootstrapStorageBenchMailbox g_bootstrap_storage_bench;
static uint32_t U16(const uint8_t *p){return p[0]|((uint32_t)p[1]<<8);}
static uint32_t U32(const uint8_t *p){return U16(p)|(U16(p+2)<<16);}
static void P16(uint8_t *p,uint32_t n){p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);}
static void P32(uint8_t *p,uint32_t n){P16(p,n);P16(p+2,n>>16);}
static uint32_t CRC(const uint8_t *p,uint32_t n){return Noodoe_Crc32(p,n);}
static void Hash(const void *p,uint32_t n,uint8_t out[32]){UpdateSha256 h;UpdateSha256_Init(&h);UpdateSha256_Feed(&h,p,n);UpdateSha256_Final(&h,out);}
static void Fail(BootstrapStorage *s,uint32_t e){s->write_lease=0;s->draining=0;s->io.lock(s->io.context);s->scope_verified=0;if(s->state==BS_WRITING)s->verified_files=0;s->state=BS_FAILED;s->error=e;}
static uint32_t Allowed(BootstrapStorage *s){return !s->draining&&s->authenticated&&s->epoch&&s->io.ign_on(s->io.context);}
void BootstrapStorage_Timeout(BootstrapStorage *s){if(s){s->backup_valid=0;Fail(s,BS_TIMEOUT);}}
static uint32_t Logical(BootstrapStorage *s,uint32_t address,void *out,uint32_t bytes)
{
    if(s->io.read_raw(s->io.context,address,out,bytes))return BS_IO;
    uint8_t *p=out;for(uint32_t i=0;i<bytes;i+=2){uint8_t a=p[i];p[i]=p[i+1];p[i+1]=a;}return 0;
}
void BootstrapStorage_Init(BootstrapStorage *s,const BootstrapStoragePlatform *io,void *arena,uint32_t capacity,const uint32_t uid[3])
{
    memset(s,0,sizeof(*s));s->io=*io;s->arena=arena;s->capacity=capacity;memcpy(s->uid,uid,12);s->io.lock(s->io.context);
}
void BootstrapStorage_SetSession(BootstrapStorage *s,uint32_t epoch,uint32_t auth)
{
    if(!s)return;
    if(s->epoch!=epoch||s->authenticated!=!!auth){
        s->backup_valid=0;s->backup_pass=0;s->request_pending=s->reply_pending=0;
        s->scoped=s->scope_verified=s->scope_reuse=s->scope_v2=0;
        if(s->state==BS_WRITING&&s->write_lease){s->draining=1;}
        else {s->io.lock(s->io.context);s->write_lease=s->draining=0;
            if(s->state!=BS_INSPECTING&&s->state!=BS_FAILED){s->state=BS_IDLE;s->error=0;}}
    }
    s->epoch=epoch;s->authenticated=!!auth;
}
uint32_t BootstrapStorage_Cancel(BootstrapStorage *s)
{if(!s)return 1;if(s->state==BS_WRITING&&s->write_lease){s->draining=1;return 0;}
 BootstrapStorage_SetSession(s,0,0);s->write_lease=s->draining=0;return 1;}
/* Only a fully verified, FAT-owned Product body is available as a local source.
 * The caller cannot choose a NOR address or reuse another file as firmware. */
uint32_t BootstrapStorage_ReadProduct(BootstrapStorage *s,uint32_t offset,void *out,uint32_t n)
{if(!s||!s->scoped||!(s->verified_files&(1U<<BS_GATE_A))||!n||n>4096||
 offset>GATE_PRODUCT_BYTES||n>GATE_PRODUCT_BYTES-offset||(offset|n)&1U)return BS_DENIED;
 return Logical(s,s->scope_first[BS_GATE_A]+4096+offset,out,n);}
uint32_t BootstrapStorage_Request(BootstrapStorage *s,uint32_t op,const void *data,uint32_t n)
{
    if(!s||!((op>=0x50&&op<=0x59&&op!=0x58)||(op>=0x80&&op<=0x83)||op==0x87||op==0x88||op==0x89||op==0x8a)||n>sizeof(s->request)||(!data&&n))return BS_ARGUMENT;
    /* A physical SWD operation owns the maintenance epoch exclusively. A
     * later radio request must not inherit its authorization or SHA state. */
    if(s->bench_active||s->request_pending||s->reply_pending||(s->draining&&op!=0x56))return BS_BUSY;
    if(n)memcpy(s->request,data,n);
    s->request_op=op;s->request_size=n;s->request_pending=1;return 0;
}
uint32_t BootstrapStorage_TakeReply(BootstrapStorage *s,void *out,uint32_t cap,uint32_t *n)
{
    if(!s||!out||!n)return BS_ARGUMENT;
    *n=0;if(!s->reply_pending)return BS_BUSY;
    if(cap<s->reply_size)return BS_ARGUMENT;
    memcpy(out,s->reply,s->reply_size);*n=s->reply_size;s->reply_pending=0;return 0;
}
static uint32_t Record(const uint8_t *p,const uint32_t uid[3],uint32_t purpose)
{
    if(U32(p)!=0x314a4643U||U32(p+4)!=1||U32(p+8)!=purpose||U32(p+16)>3968||U32(p+4092)!=0x31544d43U||CRC(p,4088)!=U32(p+4088))return 0;
    for(uint32_t i=0;i<3;++i)if(U32(p+20+4*i)!=uid[i])return 0;
    return 1;
}
/* Content validation precedes any disk mutation. Recovery has a pinned stock
 * SHA. CFW containers are freshly initialized, UID-bound stable records; no
 * arbitrary filename or replacement of an existing file is accepted. */
static uint32_t Content(BootstrapStorage *s)
{
    if(s->resource_update)return BootstrapResources_Content(s);
    uint8_t h[32];
    if(s->kind==BS_LOG){
        if(!EventLog_CheckIdentity(s->arena+EVENT_LOG_IDENTITY,s->uid))return BS_FORMAT;
        for(uint32_t o=0;o<EVENT_LOG_IDENTITY;o+=4096){uint8_t *p=s->arena+o;uint32_t blank=1;
            for(uint32_t i=0;i<4096;i++)if(p[i]!=255){blank=0;break;}
            if(!blank&&!EventLog_CheckRecord(p,s->uid))return BS_FORMAT;
        }return 0;
    }
    if(s->kind==BS_GATE_A||s->kind==BS_GATE_B){GateImageInfo info;
        if(!GateIdentity_Decode(s->arena+GATE_IDENTITY_OFFSET,s->uid,s->kind-BS_GATE_A))return BS_FORMAT;
        if(!GateImage_Decode(s->arena,s->uid,&info))return BS_FORMAT;
        Hash(s->arena+4096,GATE_PRODUCT_BYTES,h);
        if(memcmp(h,info.sha256,32)||memcmp(s->arena+4096+0x200,info.requirement,44))return BS_HASH;
        memcpy(s->gate_sha[s->kind-BS_GATE_A],h,32);
        uint32_t sp=U32(s->arena+4096),rv=U32(s->arena+4100);
        return sp==GATE_MAILBOX_ADDRESS&&(rv&1)&&rv>=GATE_PRODUCT_BASE&&rv<GATE_PRODUCT_BASE+GATE_PRODUCT_BYTES?0:BS_FORMAT;
    }
    if(s->kind==BS_GATE_JOURNAL){GateJournalRecord j;
        if(!GateJournal_Decode(s->arena,s->uid,&j)||U32(s->arena+4)!=2||j.state!=GATE_J_READY||j.active!=0||j.candidate!=GATE_NO_SLOT||j.attempts||j.flags!=GATE_F_TRIAL||j.previous!=GATE_NO_SLOT)return BS_FORMAT;
        for(uint32_t i=4096;i<s->bytes;i++)if(s->arena[i]!=0xff)return BS_FORMAT;
        memcpy(s->gate_boot_sha,j.active_sha,32);return 0;
    }
    if(s->kind==BS_RECOVERY){if(RecoveryStore_CheckHeader(s->arena,s->uid))return BS_FORMAT;
        if(s->resident){if(RecoveryStore_CheckTarget(s->arena,s->resident))return BS_HASH;}
        else if(U32(s->arena+4)==2)return BS_HASH;
        Hash(s->arena+4096,RECOVERY_IMAGE_BYTES,h);return memcmp(h,recovery_stock_sha256,32)?BS_HASH:0;}
    if(s->kind==BS_RESOURCE){
        for(uint32_t slot=0;slot<2;++slot){uint8_t *p=s->arena+slot*524288;
            if(Resources_CheckHeader(p,(const uint8_t[])RESOURCES_EXPECTED_SHA))continue;
            UpdateSha256 a;UpdateSha256_Init(&a);UpdateSha256_Feed(&a,p+48,U32(p+12)*16);
            UpdateSha256_Feed(&a,p+4096,U32(p+8));UpdateSha256_Final(&a,h);
            if(!memcmp(h,p+16,32))return 0;
        }return BS_FORMAT;
    }
    if(s->kind==BS_CONFIG||s->kind==BS_RIDE){uint32_t valid=0;
        for(uint32_t off=0;off<s->bytes;off+=4096){uint8_t *p=s->arena+off;
            if(U32(p)==0x314a4643U&&U32(p+4092)==0x31544d43U&&CRC(p,4088)==U32(p+4088)&&U32(p+4)!=1)return BS_FORMAT;
            if(Record(p,s->uid,s->kind))++valid;}return valid?0:BS_FORMAT;}
    if(!Record(s->arena,s->uid,s->kind))return BS_FORMAT;
    if(s->kind==BS_PHOTO){if(U32(s->arena+16)!=8||U32(s->arena+64)!=0x31465043U||U32(s->arena+68)!=1)return BS_FORMAT;
        for(uint32_t i=0;i<3;++i){uint32_t valid=0,blank=1;
            for(uint32_t bank=0;bank<2;++bank){uint8_t *p=s->arena+65536+i*327680+bank*163840;
                /* PhotoStore_ResetSlots invalidates only the 4KiB header.
                 * An erased header is an empty bank even when obsolete JPEG
                 * bytes remain behind it, exactly as PhotoStore_Process scans.
                 * A non-erased header still needs a committed UID-bound record
                 * and body CRC (or a valid partner); never adopt a torn header. */
                for(uint32_t j=0;j<4096;j++)if(p[j]!=255){blank=0;break;}
                if(Record(p,s->uid,16+i)&&U32(p+16)==12&&U32(p+64)==0x314a5043U&&U32(p+68)&&U32(p+68)<=131072&&CRC(p+4096,U32(p+68))==U32(p+72))++valid;}
            if(!valid&&!blank)return BS_FORMAT;}}
    return 0;
}
uint32_t BootstrapStorage_Inspect(BootstrapStorage *s,uint32_t kind)
{
    if(!s||kind>=BS_FILE_COUNT||!s->arena||sizes[kind]>s->capacity)return BS_ARGUMENT;
    if(s->state!=BS_IDLE&&s->state!=BS_SAVED&&s->state!=BS_FAILED)return BS_BUSY;
    s->reseed=0;s->resource_update=0;s->kind=kind;s->bytes=sizes[kind];s->position=s->phase=s->error=0;s->verified_files&=~(1U<<kind);
    RecoveryStore_Init(&s->audit,s->io.read_raw,s->io.context,s->uid);s->audit.resident=s->resident;s->state=BS_INSPECTING;return 0;
}
uint32_t BootstrapStorage_FilesReady(const BootstrapStorage *s){return s&&(s->verified_files&31U)==31U;}
uint32_t BootstrapStorage_ResourceCompatible(const BootstrapStorage *s,const uint8_t hash[32])
{static const uint8_t expected[32]=RESOURCES_EXPECTED_SHA;return s&&hash&&(s->verified_files&1U)&&!memcmp(hash,expected,32);}
uint32_t BootstrapStorage_BenchActive(const BootstrapStorage *s){return s&&s->bench_active;}
static void BenchBarrier(void)
{
#if defined(__arm__) || defined(__thumb__)
    __asm volatile("dmb" ::: "memory");
#else
    __asm volatile("" ::: "memory");
#endif
}
static void BenchAck(volatile BootstrapStorageBenchMailbox *m,uint32_t seq,uint32_t error)
{m->error=error;BenchBarrier();m->ack=seq;}
void BootstrapStorage_BenchProcess(BootstrapStorage *s)
{
    volatile BootstrapStorageBenchMailbox *m=&g_bootstrap_storage_bench;
    if(!s)return;
    if(!m->magic){m->version=1;m->buffer=(uint32_t)s->arena;m->capacity=s->capacity;m->metadata=(uint32_t)s->metadata;BenchBarrier();m->magic=0x31575342;}
    m->state=s->state;if(s->bench_sequence)m->error=s->error;m->first=s->first;m->bytes=s->bytes;
    /* Host publishes fields first and a new sequence last. Copy every request
     * field between sequence reads; a partial SWD update is never consumed. */
    uint32_t seq=m->sequence;BenchBarrier();BootstrapStorageBenchMailbox request=*m;BenchBarrier();
    if(seq!=m->sequence)return;
    /* A new physical cancellation revokes the next NOR operation even while
     * a previous prepare/commit is waiting for its terminal acknowledgement. */
    if(seq&&seq!=m->ack&&request.command==3){
        s->io.lock(s->io.context);s->bench_active=0;s->bench_sequence=0;
        BootstrapStorage_SetSession(s,0,0);m->state=s->state;BenchAck(m,seq,s->error);return;
    }
    if(s->bench_sequence){
        if(s->state==BS_PREPARED||s->state==BS_SAVED||s->state==BS_FAILED){
            if(s->state==BS_SAVED&&s->bench_command==4)
                for(uint32_t i=0;i<32;++i)m->image_sha256[i]=s->expected_hash[i];
            BenchAck(m,s->bench_sequence,s->error);s->bench_sequence=0;
            if(s->state!=BS_PREPARED){s->bench_active=0;s->io.lock(s->io.context);s->authenticated=0;s->epoch=0;}}
        return;
    }
    if(!seq||seq==m->ack)return;
    uint32_t cmd=request.command,bad=request.token!=0x42414b32U||!s->io.ign_on(s->io.context);
    for(uint32_t i=0;i<3;++i)if(request.uid[i]!=s->uid[i])bad=1;
    if(bad||cmd<1||cmd>9){BenchAck(m,seq,BS_DENIED);return;}
    if(cmd==4){
        if(s->authenticated||s->bench_active||s->request_pending||s->reply_pending||(s->state!=BS_IDLE&&s->state!=BS_SAVED&&s->state!=BS_FAILED)){
            BenchAck(m,seq,BS_BUSY);return;}
        BootstrapStorage_SetSession(s,0x53574401U,1);s->bench_active=1;s->position=0;s->bytes=0x8000000;
        s->error=0;s->state=BS_HASHING;UpdateSha256_Init(&s->hash);
    }else if(cmd==1||cmd>=5){
        if(s->authenticated||s->bench_active||s->request_pending||s->reply_pending||(s->state!=BS_IDLE&&s->state!=BS_SAVED&&s->state!=BS_FAILED)||s->capacity<524288){BenchAck(m,seq,BS_BUSY);return;}
        uint32_t nonzero=0;for(uint32_t i=0;i<32;++i)nonzero|=request.backup_sha256[i];
        if(!nonzero){BenchAck(m,seq,BS_DENIED);return;}
        BootstrapStorage_SetSession(s,0x53574401U,1);s->bench_active=1;s->backup_valid=1;
        for(uint32_t i=0;i<32;++i){s->backup_hash[i]=request.backup_sha256[i];s->expected_hash[i]=request.image_sha256[i];}
        s->reseed=s->resource_preserve_slot=0;
        s->resource_update=cmd==8;s->kind=cmd==8?BS_RESOURCE:cmd==1?BS_RECOVERY:cmd==9?BS_LOG:BS_GATE_A+cmd-5;s->bytes=cmd==8?524288:sizes[s->kind];s->position=0;s->error=0;s->state=BS_VERIFYING;UpdateSha256_Init(&s->hash);
    }else{
        if(!s->bench_active||s->state!=BS_PREPARED){BenchAck(m,seq,BS_DENIED);return;}
        for(uint32_t i=0;i<32;++i)if(s->expected_hash[i]!=request.image_sha256[i]){BenchAck(m,seq,BS_DENIED);return;}
        s->write_lease=1;s->state=BS_WRITING;s->phase=0;s->position=0;s->sector=0;UpdateSha256_Init(&s->hash);
    }
    m->error=0;s->bench_sequence=seq;s->bench_command=cmd;
}
static uint32_t Fat(BootstrapStorage *s,uint32_t c){uint32_t v=U16(s->metadata+4096+c+c/2);return c&1?v>>4:v&4095;}
static void SetFat(BootstrapStorage *s,uint32_t c,uint32_t n)
{for(uint32_t b=4096;b<=12288;b+=8192){uint8_t *p=s->metadata+b+c+c/2;uint32_t v=U16(p);P16(p,c&1?(v&15)|(n<<4):(v&0xf000)|n);}}
static uint32_t Prepare(BootstrapStorage *s)
{
    if(s->resource_update)return BootstrapResources_Prepare(s);
    if(s->reseed){
        /* Only an independently audited, UID-owned CFW image/journal can be
         * reseeded. FAT and directory bytes remain byte-for-byte unchanged. */
        uint32_t found=0;
        for(uint32_t i=0;i<512;i++){const uint8_t *p=s->metadata+0x5000+i*32;if(!p[0])break;
            if(p[0]!=0xe5&&p[11]!=15&&!memcmp(p,names[s->kind],11)){
                if((p[11]&0x18)||U32(p+28)!=s->bytes||U16(p+26)<2||
                   0x9000+(U16(p+26)-2)*32768!=s->scope_first[s->kind])return BS_FORMAT;
                for(uint32_t j=0;j<s->bytes/32768;j++){uint32_t next=Fat(s,U16(p+26)+j);
                    if(j+1<s->bytes/32768?next!=U16(p+26)+j+1:next<0xff8)return BS_FORMAT;}
                found++;
            }
        }
        if(found!=1)return BS_FORMAT;
        Hash(s->metadata,sizeof(s->metadata),s->metadata_hash);s->first=s->scope_first[s->kind];
        s->state=BS_PREPARED;s->phase=0;return 0;
    }
    uint32_t count=s->bytes/32768,entry=512,first=0;
    for(uint32_t i=0;i<512;++i){uint8_t *p=s->metadata+0x5000+i*32;
        if(p[0]&&p[0]!=0xe5&&p[11]!=15&&!memcmp(p,names[s->kind],11))return BS_COLLISION;
        if(entry==512&&(!p[0]||p[0]==0xe5))entry=i;
        if(!p[0])break;}
    if(entry==512)return BS_SPACE;
    for(uint32_t c=2;c+count<=4080;++c){if(0x9000+(c-2+count)*32768>RECOVERY_STORE_SAFE_END)break;
        uint32_t i=0;while(i<count&&!Fat(s,c+i))++i;if(i==count){first=c;break;}}
    if(!first)return BS_SPACE;
    Hash(s->metadata,sizeof(s->metadata),s->metadata_hash);
    for(uint32_t i=0;i<count;++i)SetFat(s,first+i,i+1==count?0xfff:first+i+1);
    uint8_t *p=s->metadata+0x5000+entry*32;memset(p,0,32);memcpy(p,names[s->kind],11);p[11]=0x20;P16(p+26,first);P32(p+28,s->bytes);
    s->first=0x9000+(first-2)*32768;s->root_index=entry;s->state=BS_PREPARED;s->phase=0;return 0;
}
#include "bootstrap_fast.inc"
static void Request(BootstrapStorage *s)
{
    const uint8_t *p=s->request;uint32_t n=s->request_size,op=s->request_op,e=0,extra=0;
    if(op!=0x56&&op!=0x57&&!Allowed(s)){e=BS_DENIED;goto done;}
    if((op>=0x80&&op<=0x83)||op==0x87||op==0x88||op==0x89||op==0x8a){e=FastRequest(s,op,p,n,&extra);}
    else if(op==0x50){
        if(n!=4||U32(p)>1||(s->state!=BS_IDLE&&s->state!=BS_SAVED)){e=BS_ARGUMENT;goto done;}
        uint32_t pass=U32(p);if(pass&&s->backup_pass!=1){e=BS_ORDER;goto done;}
        if(!pass){s->backup_pass=0;s->backup_valid=0;}s->backup_position=0;s->state=pass?BS_BACKUP_B:BS_BACKUP_A;UpdateSha256_Init(&s->hash);
    }else if(op==0x51){
        if(n!=8||U32(p)!=s->backup_position||!U32(p+4)||U32(p+4)>BOOTSTRAP_STORAGE_CHUNK||U32(p+4)>0x8000000U-s->backup_position||(s->state!=BS_BACKUP_A&&s->state!=BS_BACKUP_B)){e=BS_ORDER;goto done;}
        uint32_t k=U32(p+4);if(s->io.read_raw(s->io.context,s->backup_position,s->reply+32,k)){e=BS_IO;goto done;}
        UpdateSha256_Feed(&s->hash,s->reply+32,k);s->backup_position+=k;extra=k;
        if(s->backup_position==0x8000000U){uint8_t h[32];UpdateSha256_Final(&s->hash,h);
            if(s->state==BS_BACKUP_A){memcpy(s->backup_hash,h,32);s->backup_pass=1;s->state=BS_IDLE;}
            else if(memcmp(h,s->backup_hash,32)){Fail(s,BS_HASH);e=BS_HASH;}else{s->backup_pass=2;s->backup_valid=1;s->state=BS_IDLE;}}
    }else if(op==0x52){
        /* Existing-container adoption grants no create/overwrite capability. */
        if(s->scope_reuse||(s->scope_v2&&U32(p)<BS_FILE_COUNT&&(s->scope_existing&(1U<<U32(p))))){e=BS_DENIED;goto done;}
        if(n!=72||!s->backup_valid||memcmp(p+40,s->backup_hash,32)||U32(p)>=BS_FILE_COUNT||U32(p+4)!=sizes[U32(p)]||U32(p+4)>s->capacity||!s->arena||(s->state!=BS_IDLE&&s->state!=BS_SAVED)){e=BS_DENIED;goto done;}
        s->scope_verified=0;s->reseed=0;s->resource_update=0;s->kind=U32(p);s->bytes=U32(p+4);memcpy(s->expected_hash,p+8,32);s->position=0;s->error=0;s->state=BS_UPLOADING;
    }else if(op==0x53){
        if(s->state!=BS_UPLOADING||n<=4||n>4+BOOTSTRAP_STORAGE_CHUNK||U32(p)!=s->position||n-4>s->bytes-s->position){e=BS_ORDER;goto done;}
        memcpy(s->arena+s->position,p+4,n-4);s->position+=n-4;
    }else if(op==0x54){
        if(n||s->state!=BS_UPLOADING||s->position!=s->bytes){e=BS_ORDER;goto done;}
        s->position=0;s->state=BS_VERIFYING;UpdateSha256_Init(&s->hash);
    }else if(op==0x55){
        if(n!=32||memcmp(p,s->expected_hash,32)||s->state!=BS_PREPARED){e=BS_DENIED;goto done;}
        if(s->scoped&&s->first!=s->scope_first[s->kind]+(s->resource_update?(1U-s->resource_preserve_slot)*524288U:0U)){e=BS_HASH;goto done;}
        s->write_lease=1;s->state=BS_WRITING;s->phase=0;s->position=0;s->sector=0;UpdateSha256_Init(&s->hash);
    }else if(op==0x56){
        if(!s->authenticated){e=BS_DENIED;goto done;}
        if(!n){memcpy(s->reply+32,s->backup_hash,32);extra=32;}
        else if(n==4&&U32(p)<0x9000&&s->state==BS_PREPARED){uint32_t k=0x9000-U32(p);if(k>480)k=480;memcpy(s->reply+32,s->metadata+U32(p),k);extra=k;}
        else e=BS_ARGUMENT;
    }else if(op==0x57){if(n){e=BS_ARGUMENT;goto done;}BootstrapStorage_SetSession(s,0,0);}
    else if(op==0x59){if(n!=4)e=BS_ARGUMENT;else e=BootstrapStorage_Inspect(s,U32(p));}
done:
    P32(s->reply,e);P32(s->reply+4,s->state);P32(s->reply+8,s->error);P32(s->reply+12,s->position);P32(s->reply+16,s->first);P32(s->reply+20,s->bytes);P32(s->reply+24,s->backup_position);P32(s->reply+28,s->backup_valid);
    s->reply_size=32+extra;s->request_pending=0;s->reply_pending=1;
}
/* Each poll performs at most one read, erase or page program. Incoming status
 * requests do not mutate progress. An epoch/IGN loss stops before next write;
 * already issued NOR commands cannot be undone. Initial FAT creation is not
 * atomic and host must retain before/after metadata before COMMIT. */
void BootstrapStorage_Process(BootstrapStorage *s)
{
    if(!s)return;
    if(s->state!=BS_WRITING){s->write_lease=0;if(s->draining){s->draining=0;BootstrapStorage_SetSession(s,0,0);}}
    if(s->request_pending){Request(s);return;}
    if(s->state==BS_RESEED_CHECK){
        if(!Allowed(s)){Fail(s,BS_DENIED);return;}
        if(Logical(s,s->first+s->position,s->arena+s->position,4096)){Fail(s,BS_IO);return;}
        UpdateSha256_Feed(&s->hash,s->arena+s->position,4096);s->position+=4096;
        if(s->position==(s->resource_update?1048576U:s->bytes)){uint8_t h[32];UpdateSha256_Final(&s->hash,h);
            if(memcmp(h,s->reseed_before,32)){Fail(s,BS_HASH);return;}
            uint32_t owned=0;
            if(s->resource_update){uint32_t e=BootstrapResources_Select(s);if(e){Fail(s,e);return;}owned=1;}
            else if(s->kind!=BS_GATE_JOURNAL){GateImageInfo image;
                /* The independent tail identity survives interrupted image
                 * writes. A complete header is the fallback if tail erase was
                 * interrupted after all image bytes were already verified. */
                owned=GateIdentity_Decode(s->arena+GATE_IDENTITY_OFFSET,s->uid,s->kind-BS_GATE_A);
                if(!owned&&GateImage_Decode(s->arena,s->uid,&image)){
                    Hash(s->arena+4096,GATE_PRODUCT_BYTES,h);owned=!memcmp(h,image.sha256,32);}
            }else for(uint32_t at=0;at<s->bytes;at+=4096){GateJournalRecord j;uint8_t *p=s->arena+at;
                if(U32(p)==GATE_JOURNAL_MAGIC&&U32(p+4)>2){Fail(s,BS_FORMAT);return;}
                if(GateJournal_Decode(p,s->uid,&j))owned=1;
            }
            if(!owned){Fail(s,BS_FORMAT);return;}
            s->position=0;s->state=BS_UPLOADING;
        }return;
    }
    if(s->state==BS_LOCAL_COPY){
        if(!Allowed(s)){Fail(s,BS_DENIED);return;}
        uint32_t n=s->local_remaining;if(n>4096)n=4096;
        if(s->local_source==2){
            memset(s->arena+s->position,255,n);s->position+=n;s->local_remaining-=n;
            if(!s->local_remaining){
                uint8_t *b=s->arena;
                if(s->kind==BS_LOG)EventLog_Identity(b+EVENT_LOG_IDENTITY,s->uid);
                else if(s->kind==BS_GATE_JOURNAL){GateJournalRecord j={0};
                    j.sequence=1;j.state=GATE_J_READY;j.candidate=j.previous=GATE_NO_SLOT;j.active_generation=1;j.flags=GATE_F_TRIAL;
                    memcpy(j.uid,s->uid,12);memcpy(j.active_sha,s->gate_sha[0],32);GateJournal_Encode(b,&j);
                }else{
                    P32(b,0x314a4643);P32(b+4,1);P32(b+8,s->kind);P32(b+12,0);P32(b+16,s->kind==BS_PHOTO?8:0);
                    memcpy(b+20,s->uid,12);if(s->kind==BS_PHOTO){P32(b+64,0x31465043);P32(b+68,1);}
                    P32(b+4088,CRC(b,4088));P32(b+4092,0x31544d43);
                }
                s->state=BS_UPLOADING;
            }return;
        }
        uint32_t e=s->local_source?BootstrapStorage_ReadProduct(s,s->local_offset,s->arena+s->position,n):
            (!s->read_embedded||s->read_embedded(s->io.context,s->local_offset,s->arena+s->position,n));
        if(e){Fail(s,BS_IO);return;}s->position+=n;s->local_offset+=n;s->local_remaining-=n;
        if(!s->local_remaining)s->state=BS_UPLOADING;
        return;
    }
    if(s->state==BS_SCOPE_AUDIT||s->state==BS_SCOPE_HASH){FastProcess(s);return;}
    if(s->state==BS_HASHING){
        if(!Allowed(s)){Fail(s,BS_DENIED);return;}
        if(s->io.read_raw(s->io.context,s->position,s->work,4096)){Fail(s,BS_IO);return;}
        UpdateSha256_Feed(&s->hash,s->work,4096);s->position+=4096;
        if(s->position==0x8000000){UpdateSha256_Final(&s->hash,s->expected_hash);s->state=BS_SAVED;}
        return;
    }
    if(s->state==BS_INSPECTING){
        if(!s->phase){uint32_t state=RecoveryStore_Process(&s->audit);if(state==RECOVERY_STORE_FAILED){Fail(s,BS_FORMAT);return;}
            if(state==RECOVERY_STORE_READY||state==RECOVERY_STORE_MISSING)s->phase=1;
            return;}
        if(s->phase==1){if(Logical(s,s->position,s->metadata+s->position,4096)){Fail(s,BS_IO);return;}s->position+=4096;
            if(s->position!=sizeof(s->metadata))return;
            uint32_t found=0;
            for(uint32_t i=0;i<512;++i){uint8_t *p=s->metadata+0x5000+i*32;if(!p[0])break;
                if(p[0]!=0xe5&&p[11]!=15&&!memcmp(p,names[s->kind],11)){
                    if(found||(p[11]&0x18)||U32(p+28)!=s->bytes){Fail(s,BS_FORMAT);return;}found=1;s->inspect_cluster=U16(p+26);}}
            if(!found){Fail(s,BS_COLLISION);return;}s->position=0;s->phase=2;return;
        }
        if(s->inspect_cluster<2||s->inspect_cluster>=4080||0x9000+(s->inspect_cluster-2)*32768>RECOVERY_STORE_SAFE_END-32768){Fail(s,BS_FORMAT);return;}
        uint32_t address=0x9000+(s->inspect_cluster-2)*32768+(s->position%32768);
        if(Logical(s,address,s->arena+s->position,4096)){Fail(s,BS_IO);return;}s->position+=4096;
        if(!(s->position%32768)&&s->position<s->bytes)s->inspect_cluster=Fat(s,s->inspect_cluster);
        if(s->position==s->bytes){Hash(s->arena,s->bytes,s->inspected_hash);uint32_t e=Content(s);if(e){Fail(s,e);return;}s->verified_files|=1U<<s->kind;s->state=BS_SAVED;}
        return;
    }
    if(s->state==BS_VERIFYING){uint32_t n=s->bytes-s->position;if(n>4096)n=4096;
        UpdateSha256_Feed(&s->hash,s->arena+s->position,n);s->position+=n;
        if(s->position==s->bytes){uint8_t h[32];UpdateSha256_Final(&s->hash,h);if(memcmp(h,s->expected_hash,32)){Fail(s,BS_HASH);return;}
            uint32_t e=Content(s);if(e){Fail(s,e);return;}RecoveryStore_Init(&s->audit,s->io.read_raw,s->io.context,s->uid);s->audit.resident=s->resident;s->state=BS_AUDITING;s->phase=0;s->position=0;}return;
    }
    if(s->state==BS_AUDITING){
        if(s->resource_update&&s->phase>=2){uint32_t e=BootstrapResources_Audit(s);if(e)Fail(s,e);return;}
        if(!s->phase){uint32_t state=RecoveryStore_Process(&s->audit);if(state==RECOVERY_STORE_FAILED){Fail(s,BS_FORMAT);return;}
            if(state==RECOVERY_STORE_READY||state==RECOVERY_STORE_MISSING)s->phase=1;
            return;}
        if(Logical(s,s->position,s->metadata+s->position,4096)){Fail(s,BS_IO);return;}s->position+=4096;
        if(s->position==sizeof(s->metadata)){uint32_t e=Prepare(s);if(e)Fail(s,e);}return;
    }
    if(s->state!=BS_WRITING)return;
    if(!s->write_lease&&!Allowed(s)){Fail(s,BS_DENIED);return;}
    if(s->resource_update){uint32_t e=BootstrapResources_Write(s);if(e)Fail(s,e);return;}
    if(s->phase==0||s->phase==4){
        if(Logical(s,s->position,s->work,4096)){Fail(s,BS_IO);return;}
        UpdateSha256_Feed(&s->hash,s->work,4096);s->position+=4096;
        if(s->position==0x9000){uint8_t h[32];UpdateSha256_Final(&s->hash,h);if(memcmp(h,s->metadata_hash,32)){Fail(s,BS_HASH);return;}
            if(s->phase==0&&s->reseed){s->phase=6;s->position=0;UpdateSha256_Init(&s->hash);return;}
            if(s->phase==0){if(s->io.grant(s->io.context,s->first,s->bytes,0)){Fail(s,BS_DENIED);return;}s->phase=1;s->position=0;s->page=0;}
            else{if(s->io.grant(s->io.context,s->first,s->bytes,0x1fe)){Fail(s,BS_DENIED);return;}s->phase=5;s->position=4096;s->page=0;}}return;
    }
    if(s->phase==6){
        if(Logical(s,s->first+s->position,s->work,4096)){Fail(s,BS_IO);return;}
        UpdateSha256_Feed(&s->hash,s->work,4096);s->position+=4096;
        if(s->position==s->bytes){uint8_t h[32];UpdateSha256_Final(&s->hash,h);
            if(memcmp(h,s->reseed_before,32)){Fail(s,BS_HASH);return;}
            if(s->io.grant(s->io.context,s->first,s->bytes,0)){Fail(s,BS_DENIED);return;}
            /* Journal backup record is published in the last sector first.
             * One UID-bearing record survives every subsequent erase, even
             * when power fails after erasing the original first sector. */
            s->phase=s->kind==BS_GATE_JOURNAL?7:1;s->position=0;s->page=0;
        }return;
    }
    if(s->phase==1||s->phase==5||s->phase==7){uint32_t address=s->phase==7?s->first+s->bytes-4096:s->phase==1?s->first+s->position:s->position;
        const uint8_t *source=s->phase==7?s->arena:s->phase==1?s->arena+s->position:s->metadata+s->position;
        if(s->phase==5&&!s->page){
            if(Logical(s,address,s->work,4096)){Fail(s,BS_IO);return;}
            if(!memcmp(s->work,source,4096)){s->position+=4096;if(s->position==0x9000){s->io.lock(s->io.context);s->verified_files|=1U<<s->kind;s->state=BS_SAVED;}return;}
            s->page=18;return;
        }
        if(s->page==18){if(s->io.erase(s->io.context,address)){Fail(s,BS_IO);return;}s->page=1;return;}
        if(!s->page){if(s->io.erase(s->io.context,address)){Fail(s,BS_IO);return;}s->page=1;return;}
        if(s->page<=16){uint8_t wire[256];const uint8_t *p=source+(s->page-1)*256;
            for(uint32_t i=0;i<256;i+=2){wire[i]=p[i+1];wire[i+1]=p[i];}
            uint32_t blank=1;for(uint32_t i=0;i<256;i++)if(wire[i]!=255){blank=0;break;}
            if(!blank&&s->io.program(s->io.context,address+(s->page-1)*256,wire,256)){Fail(s,BS_IO);return;}++s->page;return;}
        if(Logical(s,address,s->work,4096)||memcmp(s->work,source,4096)){Fail(s,BS_IO);return;}
        if(s->phase==7){s->phase=1;s->position=s->page=0;return;}
        s->position+=4096;s->page=0;
        if(s->phase==1&&s->position==s->bytes){s->phase=2;s->position=0;UpdateSha256_Init(&s->hash);}
        else if(s->phase==5&&s->position==0x9000){s->io.lock(s->io.context);s->verified_files|=1U<<s->kind;s->state=BS_SAVED;}
        return;
    }
    if(s->phase==2){
        if(Logical(s,s->first+s->position,s->work,4096)){Fail(s,BS_IO);return;}
        UpdateSha256_Feed(&s->hash,s->work,4096);s->position+=4096;
        if(s->position==s->bytes){uint8_t h[32];UpdateSha256_Final(&s->hash,h);if(memcmp(h,s->expected_hash,32)){Fail(s,BS_HASH);return;}
            if(s->reseed){s->io.lock(s->io.context);s->verified_files|=1U<<s->kind;s->state=BS_SAVED;return;}
            s->phase=4;s->position=0;UpdateSha256_Init(&s->hash);}return;
    }
}
