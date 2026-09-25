#define runtime_update_test_main LegacyMain
#include "runtime_update_test.c"
#undef runtime_update_test_main
#include "Resources.h"
static uint32_t resource_ready,resource_calls;
uint32_t ResourceStore_Compatible(const uint8_t sha[32])
{++resource_calls;return resource_ready&&sha[0]==0x42;}
static void Requirement(uint32_t magic,uint32_t version,uint32_t required,uint32_t tx)
{
    SetVerified();s->transaction=tx;
    ResourceRequirement r={magic,version,required,{0x42}};
    for(uint32_t i=0;i<sizeof(r);++i)FAKE_NOR[(0x200+i)^1]=((uint8_t*)&r)[i];
    RuntimeUpdate_Process(++now);
}
uint32_t ResourceGate_Test(void)
{
    uint32_t e=TestInit();if(e)return e;
    uint32_t before=metadata_commits;
    /* Valid image but missing/loading/wrong-ID resources cannot arm BL. */
    Requirement(0x51534352,1,1,101);resource_ready=0;
    CHECK(s->platform.metadata_commit(s,s->version,s->expected_crc,UPDATE_COMMIT_ARM)==RUNTIME_UPDATE_LOCKED);
    CHECK(metadata_commits==before&&!quiet_calls&&resource_calls);
    resource_ready=1;
    CHECK(s->platform.metadata_commit(s,s->version,s->expected_crc,UPDATE_COMMIT_ARM)==0);
    CHECK(metadata_commits==before+1);
    /* The matching check is for this transaction; no stale validation leaks. */
    Requirement(0,1,1,102);before=metadata_commits;
    CHECK(s->platform.metadata_commit(s,s->version,s->expected_crc,UPDATE_COMMIT_ARM)==RUNTIME_UPDATE_LOCKED);
    Requirement(0x51534352,2,1,103);
    CHECK(s->platform.metadata_commit(s,s->version,s->expected_crc,UPDATE_COMMIT_ARM)==RUNTIME_UPDATE_LOCKED);
    Requirement(0x51534352,1,2,104);
    CHECK(s->platform.metadata_commit(s,s->version,s->expected_crc,UPDATE_COMMIT_ARM)==RUNTIME_UPDATE_LOCKED);
    CHECK(metadata_commits==before);
    Requirement(0x51534352,1,0,105);resource_ready=0;
    CHECK(s->platform.metadata_commit(s,s->version,s->expected_crc,UPDATE_COMMIT_ARM)==0);
    CHECK(metadata_commits==before+1);return 0;
}
