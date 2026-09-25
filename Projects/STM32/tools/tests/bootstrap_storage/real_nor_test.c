/* Extends the same NOR boundary stubs with real donor identity and all-file
 * inspection. The Python runner supplies captured bytes, never a live device. */
#include "test.c"
volatile uint32_t test_hash_progress;
uint32_t RealNor(uint32_t expected)
{
    memcpy(uid,input+16,12);
    uint32_t e=Reader(expected);if(e)return e;
    if(expected!=RECOVERY_STORE_READY)return 0;
    CHECK(!RecoveryStore_Read(&recovery,0,arena,RECOVERY_IMAGE_BYTES));
    CHECK(!memcmp(arena,input+4096,RECOVERY_IMAGE_BYTES));
    BootstrapStoragePlatform io={0,Read,Grant,Erase,Program,Lock,Ign};
    BootstrapStorage_Init(&service,&io,arena,1048576,uid);
    for(uint32_t kind=0;kind<BS_FILE_COUNT;++kind){
        CHECK(!BootstrapStorage_Inspect(&service,kind));
        for(uint32_t i=0;i<20000&&service.state==BS_INSPECTING;++i){
            BootstrapStorage_SetSession(&service,0,0);BootstrapStorage_Process(&service);
        }
        CHECK(service.state==BS_SAVED);
    }
    CHECK(BootstrapStorage_FilesReady(&service));CHECK(service.verified_files==31U);
    CHECK(!test_mutations&&!test_bad_write);
    return 0;
}
uint32_t FullHash(uint32_t cancel)
{
    BootstrapStoragePlatform io={0,Read,Grant,Erase,Program,Lock,Ign};
    BootstrapStorage_Init(&service,&io,arena,1048576,uid);BootstrapStorage_BenchProcess(&service);
    volatile BootstrapStorageBenchMailbox *m=&g_bootstrap_storage_bench;
    m->command=4;m->token=0x42414b32;for(uint32_t i=0;i<3;++i)m->uid[i]=uid[i];m->sequence=1;
    BootstrapStorage_BenchProcess(&service);
    CHECK(service.state==BS_HASHING&&service.bytes==0x8000000&&service.position==0);CHECK(!permit_bytes);
    BootstrapStorage_Process(&service);CHECK(service.position==4096);
    if(cancel){
        m->command=3;m->sequence=2;BootstrapStorage_BenchProcess(&service);BootstrapStorage_Process(&service);
        CHECK(m->ack==2&&!BootstrapStorage_BenchActive(&service)&&service.state==BS_IDLE);CHECK(!test_mutations&&!permit_bytes);return 0;
    }
    for(uint32_t i=0;i<40000&&m->ack!=1;++i){BootstrapStorage_Process(&service);BootstrapStorage_BenchProcess(&service);test_hash_progress=service.position;}
    CHECK(m->ack==1&&m->state==BS_SAVED&&m->error==0);CHECK(service.position==0x8000000);
    CHECK(!test_mutations&&!test_bad_write&&!permit_bytes&&!BootstrapStorage_BenchActive(&service));
    uint8_t actual[32];for(uint32_t i=0;i<32;++i)actual[i]=m->image_sha256[i];CHECK(!memcmp(actual,input,32));
    return 0;
}
uint32_t HashOwnership(uint32_t unused)
{
    (void)unused;BootstrapStoragePlatform io={0,Read,Grant,Erase,Program,Lock,Ign};
    BootstrapStorage_Init(&service,&io,arena,1048576,uid);BootstrapStorage_BenchProcess(&service);
    volatile BootstrapStorageBenchMailbox *m=&g_bootstrap_storage_bench;
    m->token=0x42414b32;for(uint32_t i=0;i<3;++i)m->uid[i]=uid[i];
    for(uint32_t command=1;command<=4;command+=3){
        for(uint32_t pending=0;pending<2;++pending){
            service.request_pending=!pending;service.reply_pending=pending;
            m->command=command;++m->sequence;BootstrapStorage_BenchProcess(&service);
            CHECK(m->ack==m->sequence&&m->error==BS_BUSY);
            CHECK(service.state==BS_IDLE&&!BootstrapStorage_BenchActive(&service));
            CHECK(service.request_pending==!pending&&service.reply_pending==pending);
        }
    }
    service.request_pending=service.reply_pending=0;m->command=4;++m->sequence;BootstrapStorage_BenchProcess(&service);
    CHECK(service.state==BS_HASHING&&BootstrapStorage_BenchActive(&service));
    CHECK(BootstrapStorage_Request(&service,0x57,0,0)==BS_BUSY);
    CHECK(!service.request_pending);m->command=3;++m->sequence;BootstrapStorage_BenchProcess(&service);
    CHECK(!BootstrapStorage_BenchActive(&service)&&service.state==BS_IDLE&&!test_mutations);
    return 0;
}
/* Fast boundary fixture: seed an empty SHA context at the final read so the
 * host can independently compare precisely the final 4096 physical bytes.
 * This supplements, rather than claims to replace, the complete sweep test. */
uint32_t HashFinalChunk(uint32_t unused)
{
    (void)unused;BootstrapStoragePlatform io={0,Read,Grant,Erase,Program,Lock,Ign};
    BootstrapStorage_Init(&service,&io,arena,1048576,uid);BootstrapStorage_BenchProcess(&service);
    volatile BootstrapStorageBenchMailbox *m=&g_bootstrap_storage_bench;
    m->command=4;m->token=0x42414b32;for(uint32_t i=0;i<3;++i)m->uid[i]=uid[i];m->sequence=1;
    BootstrapStorage_BenchProcess(&service);CHECK(service.state==BS_HASHING);
    service.position=0x8000000-4096;BootstrapStorage_Process(&service);BootstrapStorage_BenchProcess(&service);
    CHECK(service.position==0x8000000&&m->state==BS_SAVED&&m->ack==1&&!m->error);
    uint8_t h[32];for(uint32_t i=0;i<32;++i)h[i]=m->image_sha256[i];CHECK(!memcmp(h,input,32));
    BootstrapStorage_Process(&service);CHECK(service.position==0x8000000&&service.state==BS_SAVED);
    CHECK(!test_mutations&&!test_bad_write&&!permit_bytes&&!BootstrapStorage_BenchActive(&service));return 0;
}
