#include "Bootstrap_Storage.h"
#include "gate_store.h"
#include <string.h>
static RecoveryStore recovery;
static BootstrapStorage service;
static GateStore gate_store;
static uint32_t uid[3]={1,2,3},permit_first,permit_bytes,permit_mask,ign=1;
volatile uint32_t test_assertions,test_mutations,test_bad_write;
volatile uint32_t test_read_bytes;
#define CHECK(c) do{++test_assertions;if(!(c))return __LINE__;}while(0)
static uint8_t *const raw=(uint8_t *)0x90000000;
static uint8_t *const arena=(uint8_t *)0xc0000000;
static uint8_t *const input=(uint8_t *)0x11000000;
void *memcpy(void *d,const void *s,size_t n){unsigned char *a=d;const unsigned char *b=s;while(n--)*a++=*b++;return d;}
void *memset(void *d,int v,size_t n){unsigned char *a=d;while(n--)*a++=(unsigned char)v;return d;}
int memcmp(const void *a,const void *b,size_t n){const unsigned char *x=a,*y=b;while(n--){if(*x!=*y)return *x-*y;++x;++y;}return 0;}
static uint32_t Read(void *c,uint32_t off,void *p,uint32_t n){(void)c;if(!p||off>=0x8000000||n>0x8000000-off)return 1;test_read_bytes+=n;memcpy(p,raw+off,n);return 0;}
static uint32_t Grant(void *c,uint32_t first,uint32_t bytes,uint32_t mask){(void)c;permit_first=first;permit_bytes=bytes;permit_mask=mask;return 0;}
static void Lock(void *c){(void)c;permit_bytes=permit_mask=0;}
static uint32_t Allowed(uint32_t off,uint32_t n){return permit_bytes&&((off>=permit_first&&off-permit_first<permit_bytes&&n<=permit_bytes-(off-permit_first))||
    (off/4096>=1&&off/4096<=8&&(permit_mask&(1U<<(off/4096)))&&n<=4096-(off&4095)));}
static uint32_t Erase(void *c,uint32_t off){(void)c;if(!Allowed(off,4096)||(off&4095)){++test_bad_write;return 1;}memset(raw+off,255,4096);++test_mutations;return 0;}
static uint32_t Program(void *c,uint32_t off,const void *p,uint32_t n){(void)c;if(!Allowed(off,n)||!n||n>256||n>256-(off&255)){++test_bad_write;return 1;}
    const uint8_t *b=p;for(uint32_t i=0;i<n;++i){if((raw[off+i]&b[i])!=b[i])return 1;raw[off+i]&=b[i];}++test_mutations;return 0;}
static uint32_t Ign(void *c){(void)c;return ign;}
uint32_t Reader(uint32_t expected)
{
    RecoveryStore_Init(&recovery,Read,0,uid);
    for(uint32_t i=0;i<10000&&recovery.state<=RECOVERY_STORE_HASHING;++i)RecoveryStore_Process(&recovery);
    CHECK(recovery.state==expected);
    if(expected==RECOVERY_STORE_READY){uint8_t b[31];
        CHECK(!RecoveryStore_Read(&recovery,1,b,31));CHECK(!memcmp(b,input+4097,31));
        CHECK(!RecoveryStore_Read(&recovery,28671,b,31));CHECK(!memcmp(b,input+4096+28671,31));
        CHECK(RecoveryStore_Read(&recovery,RECOVERY_IMAGE_BYTES,b,1)!=0);
        CHECK(RecoveryStore_Read(&recovery,RECOVERY_IMAGE_BYTES-1,b,2)!=0);
        CHECK(!RecoveryStore_Read(&recovery,RECOVERY_IMAGE_BYTES-1,b,1));CHECK(b[0]==input[4096+RECOVERY_IMAGE_BYTES-1]);}
    return 0;
}
static uint32_t Req(uint32_t op,const void *p,uint32_t n,uint32_t *state)
{
    uint8_t out[512];uint32_t len=0,e=BootstrapStorage_Request(&service,op,p,n);if(e)return e;
    BootstrapStorage_Process(&service);e=BootstrapStorage_TakeReply(&service,out,sizeof(out),&len);if(e)return e;
    if(len<32)return 100;
    uint32_t *v=(uint32_t *)out;if(state)*state=v[1];return v[0];
}
/* Packet-level fast installer harness; Python supplies NOR/BT and accelerates
 * SHA with hashlib. All allocation, guards, write/readback states are ARM C. */
static uint32_t Embedded(void *p,uint32_t o,void *b,uint32_t n){(void)p;if(o>0x70000||n>0x70000-o)return 1;memcpy(b,(const void*)(0x12000000+o),n);return 0;}
uint32_t FastInit(void){BootstrapStoragePlatform io={0,Read,Grant,Erase,Program,Lock,Ign};BootstrapStorage_Init(&service,&io,arena,1048576,uid);service.read_embedded=Embedded;BootstrapStorage_SetSession(&service,1,1);return (uint32_t)&service;}
uint32_t FastDisconnect(void){BootstrapStorage_SetSession(&service,0,0);return service.state;}
uint32_t FastCancel(void){return BootstrapStorage_Cancel(&service);}
uint32_t FastPhase(void){return service.phase;}
uint32_t FastPacket(uint32_t op){uint32_t n;memcpy(&n,input,4);return BootstrapStorage_Request(&service,op,input+4,n);}
uint32_t FastPump(uint32_t count){for(uint32_t i=0;i<count;i++)BootstrapStorage_Process(&service);return service.state;}
uint32_t FastReply(void){uint32_t n=0,e=BootstrapStorage_TakeReply(&service,input+4,1024,&n);memcpy(input,&n,4);return e;}
/* Owner-level timeout must revoke every write proof without another NOR write. */
uint32_t FastTimeout(void){uint32_t mutations=test_mutations;BootstrapStorage_Timeout(&service);
 CHECK(service.state==BS_FAILED&&service.error==BS_TIMEOUT&&!service.backup_valid&&!service.scope_verified&&!permit_bytes);
 for(uint32_t n=0;n<100;n++)BootstrapStorage_Process(&service);
 CHECK(test_mutations==mutations);return 0;}
/* Tests seed a previously established backup receipt only for install paths.
 * Backup ordering/epoch guards are independently exercised below; no claim of
 * simulating complete256MiB wireless transfer is made by this fixture. */
uint32_t Install(uint32_t kind)
{
    BootstrapStoragePlatform io={0,Read,Grant,Erase,Program,Lock,Ign};
    BootstrapStorage_Init(&service,&io,arena,1048576,uid);uint32_t state;
    CHECK(Req(0x50,&kind,4,&state)==BS_DENIED);
    BootstrapStorage_SetSession(&service,1,1);uint32_t pass=1;
    CHECK(Req(0x50,&pass,4,&state)==BS_ORDER);pass=0;CHECK(!Req(0x50,&pass,4,&state));
    uint32_t invalid[2]={1,1};CHECK(Req(0x51,invalid,8,&state)==BS_ORDER);
    BootstrapStorage_SetSession(&service,2,1);CHECK(!service.backup_valid&&service.state==BS_IDLE);
    uint8_t begin[72]={0},digest[32];uint32_t sizes[9]={1048576,131072,262144,1048576,524288,524288,524288,65536,262144};
    uint32_t n=sizes[kind];UpdateSha256 h;UpdateSha256_Init(&h);UpdateSha256_Feed(&h,input,n);UpdateSha256_Final(&h,digest);
    memcpy(begin,&kind,4);memcpy(begin+4,&n,4);memcpy(begin+8,digest,32);
    CHECK(Req(0x52,begin,72,&state)==BS_DENIED);
    service.backup_valid=1;memset(service.backup_hash,0,32);
    CHECK(!Req(0x52,begin,72,&state));
    for(uint32_t pos=0;pos<n;){uint8_t packet[484];uint32_t k=n-pos;if(k>480)k=480;memcpy(packet,&pos,4);memcpy(packet+4,input+pos,k);CHECK(!Req(0x53,packet,k+4,&state));pos+=k;}
    CHECK(!Req(0x54,0,0,&state));
    for(uint32_t i=0;i<10000&&service.state!=BS_PREPARED&&service.state!=BS_FAILED;++i)BootstrapStorage_Process(&service);
    CHECK(service.state==BS_PREPARED);CHECK(test_mutations==0);
    uint8_t wrong[32]={0};CHECK(Req(0x55,wrong,32,&state)==BS_DENIED);CHECK(!test_mutations);
    CHECK(!Req(0x55,digest,32,&state));
    for(uint32_t i=0;i<100000&&service.state==BS_WRITING;++i)BootstrapStorage_Process(&service);
    CHECK(service.state==BS_SAVED);CHECK(!test_bad_write);CHECK(!permit_bytes);
    /* Attempted repeat is rejected before the first additional mutation. */
    uint32_t writes=test_mutations;CHECK(!Req(0x52,begin,72,&state));memcpy(arena,input,n);service.position=n;CHECK(!Req(0x54,0,0,&state));
    for(uint32_t i=0;i<10000&&service.state!=BS_PREPARED&&service.state!=BS_FAILED;++i)BootstrapStorage_Process(&service);
    CHECK(service.state==BS_FAILED&&service.error==BS_COLLISION);CHECK(test_mutations==writes);
    return 0;
}
uint32_t Size(void){return sizeof(RecoveryStore);}
/* Isolated gate's allocation-free FAT reader shares no Product service state. */
uint32_t GateReader(uint32_t scenario)
{
    GateStore_Init(&gate_store,Read,0,uid);
    for(uint32_t i=0;i<10000&&gate_store.state==GATE_STORE_AUDIT;++i)GateStore_Process(&gate_store);
    if(scenario==3){CHECK(gate_store.state==GATE_STORE_ERROR);return 0;}
    CHECK(gate_store.state==GATE_STORE_READY);CHECK(!test_mutations);
    uint8_t b[33];uint32_t physical;
    if(!scenario){CHECK(!gate_store.found);CHECK(GateStore_Read(&gate_store,0,0,b,1));return 0;}
    CHECK(gate_store.found==31);
    CHECK(gate_store.identity==(scenario==2?2U:3U));
    if(scenario==2){CHECK(GateStore_Read(&gate_store,0,0,b,1));return 0;}
    CHECK(!GateStore_Read(&gate_store,0,4097,b,33));CHECK(!memcmp(b,input+4097,33));
    CHECK(!GateStore_Read(&gate_store,0,32767,b,33));CHECK(!memcmp(b,input+32767,33));
    CHECK(!GateStore_Read(&gate_store,0,0x7ffff,b,1));CHECK(b[0]==input[0x7ffff]);
    CHECK(GateStore_Read(&gate_store,0,0x7ffff,b,2));
    CHECK(GateStore_Read(&gate_store,0,0x80000,b,1));
    CHECK(GateStore_Read(&gate_store,5,0,b,1));
    CHECK(!GateStore_Address(&gate_store,0,32768,&physical));CHECK(physical<0x07f70000);
    return 0;
}
uint32_t Inspect(uint32_t kind)
{
    BootstrapStoragePlatform io={0,Read,Grant,Erase,Program,Lock,Ign};
    BootstrapStorage_Init(&service,&io,arena,1048576,uid);
    CHECK(!BootstrapStorage_Inspect(&service,kind));
    for(uint32_t i=0;i<10000&&service.state==BS_INSPECTING;++i){BootstrapStorage_SetSession(&service,0,0);BootstrapStorage_Process(&service);}
    CHECK(service.state==BS_SAVED);CHECK(service.verified_files==(1U<<kind));CHECK(test_mutations==0);
    CHECK(!BootstrapStorage_FilesReady(&service));
    BootstrapStorage_SetSession(&service,1,1);service.state=BS_WRITING;permit_bytes=32768;
    BootstrapStorage_SetSession(&service,2,1);CHECK(service.state==BS_FAILED&&service.error==BS_DENIED);CHECK(!permit_bytes&&!service.verified_files);
    BootstrapStorage_SetSession(&service,2,1);CHECK(service.state==BS_FAILED);CHECK(test_mutations==0);
    return 0;
}
/* Physical bench transport invokes the same verifier and NOR writer; tokens
 * only declare intent. Independent host A/B evidence is outside this fixture. */
uint32_t Bench(uint32_t kind)
{
    uint32_t n=kind==7?65536:524288,command=kind==4?1:kind;BootstrapStoragePlatform io={0,Read,Grant,Erase,Program,Lock,Ign};
    BootstrapStorage_Init(&service,&io,arena,1048576,uid);BootstrapStorage_BenchProcess(&service);
    volatile BootstrapStorageBenchMailbox *m=&g_bootstrap_storage_bench;
    CHECK(sizeof(*m)==128&&m->magic==0x31575342&&m->capacity==1048576);
    m->command=command;m->sequence=1;BootstrapStorage_BenchProcess(&service);
    CHECK(m->ack==1&&m->error==BS_DENIED&&!BootstrapStorage_BenchActive(&service));
    BootstrapStorage_BenchProcess(&service);CHECK(m->error==BS_DENIED);
    m->token=0x42414b32;for(uint32_t i=0;i<3;++i)m->uid[i]=uid[i];
    m->sequence=2;BootstrapStorage_BenchProcess(&service);CHECK(m->ack==2&&m->error==BS_DENIED);
    memcpy(arena,input,n);uint8_t digest[32];UpdateSha256 h;
    UpdateSha256_Init(&h);UpdateSha256_Feed(&h,input,n);UpdateSha256_Final(&h,digest);
    for(uint32_t i=0;i<32;++i){m->backup_sha256[i]=(uint8_t)(i+1);m->image_sha256[i]=digest[i];}
    m->sequence=3;BootstrapStorage_BenchProcess(&service);CHECK(BootstrapStorage_BenchActive(&service));
    for(uint32_t i=0;i<10000&&m->ack!=3;++i){BootstrapStorage_Process(&service);BootstrapStorage_BenchProcess(&service);}
    CHECK(m->ack==3&&m->error==0&&m->state==BS_PREPARED);CHECK(!test_mutations);
    m->image_sha256[0]^=1;m->command=2;m->sequence=4;BootstrapStorage_BenchProcess(&service);
    CHECK(m->ack==4&&m->error==BS_DENIED&&service.state==BS_PREPARED);
    BootstrapStorage_BenchProcess(&service);CHECK(m->error==BS_DENIED);CHECK(!test_mutations);
    m->image_sha256[0]^=1;m->sequence=5;BootstrapStorage_BenchProcess(&service);
    for(uint32_t i=0;i<10000&&m->ack!=5;++i){BootstrapStorage_Process(&service);BootstrapStorage_BenchProcess(&service);}
    CHECK(m->ack==5&&m->error==0&&m->state==BS_SAVED);CHECK(!BootstrapStorage_BenchActive(&service));
    CHECK(!permit_bytes&&!test_bad_write&&test_mutations>0);
    /* Pending request cancellation must stop before any additional mutation. */
    uint32_t writes=test_mutations;service.state=BS_WRITING;service.bench_active=1;service.bench_sequence=6;
    service.authenticated=1;service.epoch=1;permit_bytes=32768;m->command=3;m->sequence=7;
    BootstrapStorage_BenchProcess(&service);BootstrapStorage_Process(&service);
    CHECK(m->ack==7&&!BootstrapStorage_BenchActive(&service)&&!permit_bytes);
    CHECK(service.state==BS_FAILED&&test_mutations==writes);
    return 0;
}

/* Actual empty-resource-B updater, including interruption before commit. */
uint32_t ResourceB(uint32_t scenario)
{
    BootstrapStoragePlatform io={0,Read,Grant,Erase,Program,Lock,Ign};
    BootstrapStorage_Init(&service,&io,arena,1048576,uid);BootstrapStorage_BenchProcess(&service);
    volatile BootstrapStorageBenchMailbox *m=&g_bootstrap_storage_bench;
    memcpy(arena,input,524288);UpdateSha256 h;uint8_t digest[32];UpdateSha256_Init(&h);UpdateSha256_Feed(&h,input,524288);UpdateSha256_Final(&h,digest);
    m->command=8;m->token=0x42414b32;for(uint32_t i=0;i<3;i++)m->uid[i]=uid[i];
    for(uint32_t i=0;i<32;i++){m->backup_sha256[i]=1;m->image_sha256[i]=digest[i];}
    m->sequence=1;
    for(uint32_t i=0;i<10000&&m->ack!=1;i++){BootstrapStorage_BenchProcess(&service);BootstrapStorage_Process(&service);}
    BootstrapStorage_BenchProcess(&service);CHECK(m->ack==1);CHECK(!test_mutations);
    if(scenario>=1&&scenario<=4){CHECK(service.state==BS_FAILED);CHECK(m->error!=0);CHECK(!permit_bytes);return 0;}
    CHECK(service.state==BS_PREPARED&&m->error==0);CHECK(service.bytes==524288);
    if(scenario==6)arena[4096]^=1;
    m->command=2;m->sequence=2;
    for(uint32_t i=0;i<10000&&m->ack!=2;i++){
        BootstrapStorage_BenchProcess(&service);BootstrapStorage_Process(&service);
        if(scenario==5&&test_mutations>=32)ign=0;
    }
    BootstrapStorage_BenchProcess(&service);CHECK(m->ack==2);CHECK(!permit_bytes&&!test_bad_write);
    if(scenario==5||scenario==6){CHECK(service.state==BS_FAILED);for(uint32_t i=0;i<4;i++)CHECK(raw[service.first+4092+i]==255);return 0;}
    CHECK(service.state==BS_SAVED&&m->error==0);CHECK(service.verified_files&1);return 0;
}
