#include "RuntimeUpdate.h"
#include "Recovery_Core.h"
#include "Update_Metadata.h"
#include "Bootstrap_Recovery.h"
#include "runtime_update_test_port.h"
#include "runtime_update_fixture.h"
#include <string.h>

#define ARENA_ADDRESS 0xC0020000UL
#define FAKE_NOR ((uint8_t *)0x11000000UL)
#define CHECK(condition) do { ++runtime_update_assertions;if(!(condition)){ \
    runtime_update_failure_line=__LINE__;return __LINE__; } } while(0)
volatile uint32_t runtime_update_assertions,runtime_update_failure_line;
volatile uint32_t test_command_opcode,test_command_expected,test_command_actual;
volatile BSP_RAM_Diagnostics g_bsp_ram;
volatile BSP_NOR_Diagnostics g_bsp_nor;
static uint32_t context_fault,allocation_address,allocation_calls,allocation_bytes,early_visibility;
static uint32_t enables,disables,reads,programs,erases,metadata_reads,metadata_commits,resets;
static uint32_t enabled_tx,nor_result,metadata_read_result,metadata_commit_result,callback_faults;
static uint32_t last_commit_version,last_commit_crc,last_commit_token,last_commit_scratch,last_commit_bytes;
static uint32_t metadata[5],sequence,now;
static uint32_t init_phase=1U,quiet_calls,resume_calls,paused_token;
static uint32_t epoch_in_program,epoch_in_erase,epoch_in_quiet,unready_in_quiet,zero_quiet_token;
static uint32_t program_trace_count,program_address[8],program_length[8];
static int quiet_result,resume_result;
static uint32_t flash_denied,flash_checks,writer_destructive;
uint32_t RuntimeUpdate_FlashReady(void){++flash_checks;return !flash_denied;}
void UpdateMetadata_GetDiagnostics(UpdateMetadataDiagnostics *out)
{memset(out,0,sizeof(*out));out->destructive_started=writer_destructive;}
static uint8_t reply[NDCP_FRAME_MAX],wire_data[1024],manifest[56];
static UpdateService *s;
#if NOODOE_BOOTSTRAP
static uint32_t gate_denied,gate_calls,gate_target,gate_version,gate_view;
static uint8_t gate_requirement[44],gate_sha[32];
uint32_t BootstrapUpdate_ValidateTarget(uint32_t target,const uint8_t req[44],uint32_t version,const uint8_t sha[32])
{++gate_calls;gate_target=target;gate_version=version;gate_view=req!=NULL;
 if(req){memcpy(gate_requirement,req,44);}
 memcpy(gate_sha,sha,32);return gate_denied;}
#endif

/* 독립 freestanding 시험의 libc다. SDRAM 서비스 초기화 중에 객체를 조기 공개하면
 * 여기서 GetService를 관측해 잡는다. 제품 memcpy/memset 자체의 성능 시험은 아니다. */
void *memset(void *destination,int value,size_t length)
{
    uint8_t *out=destination;size_t i;
    if(init_phase&&(uintptr_t)destination==ARENA_ADDRESS&&RuntimeUpdate_GetService()!=NULL)++early_visibility;
    for(i=0U;i<length;++i)out[i]=(uint8_t)value;
    return destination;
}
/* 표준 byte 복사만 제공하며 adapter/UpdateService 동작은 실제 소스를 사용한다. */
void *memcpy(void *destination,const void *source,size_t length)
{uint8_t *d=destination;const uint8_t *p=source;size_t i;for(i=0U;i<length;++i)d[i]=p[i];return destination;}
/* NDCP resync가 사용하는 겹친 buffer 이동을 지원한다. */
void *memmove(void *destination,const void *source,size_t length)
{uint8_t *d=destination;const uint8_t *p=source;size_t i;if(d<p){for(i=0U;i<length;++i)d[i]=p[i];}else{for(i=length;i>0U;--i)d[i-1U]=p[i-1U];}return destination;}
/* 메모리 비교의 반환 부호를 실제 SHA/재전송 검증 코드가 사용한다. */
int memcmp(const void *left,const void *right,size_t length)
{const uint8_t *a=left,*b=right;size_t i;for(i=0U;i<length;++i)if(a[i]!=b[i])return (int)a[i]-(int)b[i];return 0;}

/* 각 bit를 IRQ/비특권/PRIMASK/BASEPRI/FAULTMASK 거부 조건의 모형으로 사용한다.
 * 실제 CMSIS register expression의 검증은 제품 소스 별도 컴파일과 읽기 검토 범위다. */
uint32_t RuntimeUpdateTest_ContextOkay(void){return context_fault==0U;}
/* 시스템 리셋 대신 횟수를 기록한다. 정확한 callback 문맥도 확인한다. */
void RuntimeUpdateTest_Reset(void)
{if(context_fault!=0U)++callback_faults;++resets;}

/* blocking BT pause 경계에 연결 epoch 변경/미준비를 주입한다. 실제 UART나 IRQ는
 * 건드리지 않으며 timeout1000ms와 capability 닫기 순서만 검증한다. */
int Bluetooth_QuiesceTransport(uint32_t timeout_ms,uint32_t *token_out)
{
    ++quiet_calls;
    if(timeout_ms!=1000U||!token_out||context_fault||enabled_tx)++callback_faults;
    if(quiet_result)return quiet_result;
    paused_token=zero_quiet_token?0U:0xA1B2C3D4UL;*token_out=paused_token;
    if(epoch_in_quiet){UpdateService_SetConnected(s,0U);UpdateService_SetConnected(s,1U);UpdateService_Authorize(s,UPDATE_STAGE_ARM);}
    if(unready_in_quiet)g_bsp_ram.ready=0U;
    return BLUETOOTH_OK;
}
/* pause 성공 뒤 writer 성공/오류/사전 검사 거부에서도 정확한 token을 반납해야 한다. */
int Bluetooth_ResumeTransport(uint32_t token)
{
    ++resume_calls;
    if(token!=paused_token)++callback_faults;
    paused_token=0U;
    if(!token)return BLUETOOTH_INVALID;
    return resume_result;
}

/* allocator 안에서 GetService를 호출하여 Init 중간의 acquire 조회를 검사한다.
 * 테스트가 지정한 주소만 돌려주고 실제 SDRAM 초기화/기존 allocation은 건드리지 않는다. */
void *BSP_RAM_Allocate(size_t bytes)
{
    ++allocation_calls;allocation_bytes=(uint32_t)bytes;
    if(RuntimeUpdate_GetService()!=NULL)++early_visibility;
    return (void *)(uintptr_t)allocation_address;
}

/* fake BSP도 별도 범위를 검사하므로 adapter가 놓친 범위를 callback_faults로 잡는다. */
static uint32_t InStage(uint32_t address,uint32_t length)
{return address>=UPDATE_STAGE_BASE&&address<UPDATE_STAGE_BASE+UPDATE_APP_BYTES&&length&&length<=UPDATE_STAGE_BASE+UPDATE_APP_BYTES-address;}

/* FAKE_NOR는 SPI 물리 주소의 byte 배열이며 여기서 pair-swap/주소 xor1을 하지 않는다.
 * 논리 APP codec은 실제 adapter만 수행해야 한다. 반환 오류 코드를 그대로 주입하여
 * adapter가 다른 API enum의0으로 잘못 바꾸지 않는지 검사한다. */
BSP_NOR_Status BSP_NOR_Read(uint32_t address,void *destination,uint32_t length)
{
    ++reads;
    if(context_fault||!g_bsp_ram.ready||!g_bsp_nor.ready||!destination||!InStage(address,length)){
        ++callback_faults;return BSP_NOR_ARGUMENT;
    }
    if(nor_result)return (BSP_NOR_Status)nor_result;
    memcpy(destination,FAKE_NOR+address-UPDATE_STAGE_BASE,length);return BSP_NOR_OK;
}
/* nonzero transaction/pending gate만 모델링한다. 전체백업 승인은 adapter/service가
 * 별도로 검사해야 하므로 이 함수의 호출 횟수가 승인 우회 검출 기준이다. */
BSP_NOR_Status BSP_NOR_OTAEnable(uint32_t tx)
{
    ++enables;
    if(context_fault||!tx||metadata[0]!=UPDATE_METADATA_RESIDENT||metadata[4])return BSP_NOR_LOCKED;
    if(nor_result)return (BSP_NOR_Status)nor_result;
    if(enabled_tx&&enabled_tx!=tx)return BSP_NOR_BUSY;
    enabled_tx=tx;return BSP_NOR_OK;
}
/* software capability를 닫는 함수는 하드웨어 미준비 상태에서도 호출 가능하다. */
void BSP_NOR_OTADisable(void){++disables;enabled_tx=0U;}
/* 페이지 경계/1→0 조건을 모의한다. NOR 실제 program opcode는 실행하지 않는다. */
BSP_NOR_Status BSP_NOR_OTAProgram(uint32_t address,const void *source,uint32_t length)
{
    const uint8_t *p=source;uint32_t i,offset=address-UPDATE_STAGE_BASE;
    ++programs;
    if(program_trace_count<8U){program_address[program_trace_count]=address;program_length[program_trace_count]=length;}
    ++program_trace_count;
    if(context_fault||!enabled_tx||!source||!InStage(address,length)||length>256U||length>256U-(address&255U)){
        ++callback_faults;return BSP_NOR_ARGUMENT;
    }
    if(nor_result)return (BSP_NOR_Status)nor_result;
    for(i=0U;i<length;++i){if((FAKE_NOR[offset+i]&p[i])!=p[i])return BSP_NOR_VERIFY;FAKE_NOR[offset+i]=p[i];}
    /* N번째 실제 물리 program이 끝나는 순간에만 연결 epoch를 바꾼다. 논리 callback
     * 하나가 lead/middle/tail로 나뉠 때 매 mutation의 재검사 누락을 드러낸다. */
    if(epoch_in_program&&--epoch_in_program==0U){UpdateService_SetConnected(s,0U);UpdateService_SetConnected(s,1U);UpdateService_Authorize(s,UPDATE_STAGE_ARM);}
    return BSP_NOR_OK;
}
/* staging의 정확한4KiB 경계만 지워진 것처럼 RAM을 채운다. */
BSP_NOR_Status BSP_NOR_OTAErase4K(uint32_t address)
{
    ++erases;
    if(context_fault||!enabled_tx||!InStage(address,4096U)||(address&4095U)){
        ++callback_faults;return BSP_NOR_ARGUMENT;
    }
    if(nor_result)return (BSP_NOR_Status)nor_result;
    memset(FAKE_NOR+address-UPDATE_STAGE_BASE,255,4096U);
    if(epoch_in_erase){epoch_in_erase=0U;UpdateService_SetConnected(s,0U);UpdateService_SetConnected(s,1U);UpdateService_Authorize(s,UPDATE_STAGE_ARM);}
    return BSP_NOR_OK;
}

/* 원본 메타5words를 정규화 없이 복사하고 오류는 caller 출력에 손대지 않는다. */
UpdateMetadataResult UpdateMetadata_Read(uint32_t words[5])
{
    ++metadata_reads;
    if(context_fault||!words){++callback_faults;return UPDATE_METADATA_CONTEXT;}
    if(metadata_read_result)return (UpdateMetadataResult)metadata_read_result;
    memcpy(words,metadata,sizeof(metadata));return UPDATE_METADATA_OK;
}
/* metadata 엔진 자체는 별도 검증 대상이다. 이 시험은 adapter가 넘긴 내부 arm token,
 * version/ISO CRC 및16KiB 독점 scratch의 정확한 인수와 오류 전파만 확인한다. */
UpdateMetadataResult UpdateMetadata_Commit(uint32_t version,uint32_t crc,uint32_t arm,void *scratch,uint32_t bytes)
{
    writer_destructive=metadata_commit_result==0||metadata_commit_result==UPDATE_METADATA_AMBIGUOUS;
    ++metadata_commits;
    last_commit_version=version;last_commit_crc=crc;last_commit_token=arm;
    last_commit_scratch=(uint32_t)(uintptr_t)scratch;last_commit_bytes=bytes;
    if(context_fault||arm!=UPDATE_METADATA_ARM_TOKEN||bytes!=0x4000U
       ||last_commit_scratch!=g_runtime_update.scratch_address||enabled_tx||paused_token!=0xA1B2C3D4UL){
        ++callback_faults;return UPDATE_METADATA_ARGUMENT;
    }
    if(metadata_commit_result)return (UpdateMetadataResult)metadata_commit_result;
    if(metadata[4])return UPDATE_METADATA_PENDING;
    metadata[0]=UPDATE_METADATA_RESIDENT;metadata[1]=version;metadata[2]=UPDATE_METADATA_APP_BLOCK;
    metadata[3]=UPDATE_METADATA_APP_LENGTH;metadata[4]=crc;return UPDATE_METADATA_OK;
}

/* ABI 정수는 little-endian으로 구성한다. */
static void Put(uint8_t *p,uint32_t value)
{uint32_t i;for(i=0U;i<4U;++i)p[i]=(uint8_t)(value>>(i*8U));}
/* NDCP 응답의 명시 byte 순서를 읽는다. */
static uint32_t Get(const uint8_t *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8U)|((uint32_t)p[2]<<16U)|((uint32_t)p[3]<<24U);}
/* 정상 RAM/NOR ready는 누락 필드를 hidden default로 만들지 않도록 모두 지정한다. */
static void SetReady(void)
{
    context_fault=0U;g_bsp_ram.ready=1U;g_bsp_ram.result=0U;g_bsp_ram.capacity_bytes=BSP_RAM_GEOMETRY_BYTES;
    g_bsp_nor.ready=1U;g_bsp_nor.capacity_bytes=BSP_NOR_CAPACITY_BYTES;g_bsp_nor.jedec_id=BSP_NOR_EXPECTED_JEDEC_ID;
}

/* 초기화 실패와 잘못된 allocator 주소에서는 객체가 공개되지 않는지 검사한다.
 * 마지막 정상 Init 이후에는 실제 Runtime의 재초기화를 호출해 idempotence를 본다. */
static uint32_t TestInit(void)
{
    uint32_t before,i,service_bytes=((sizeof(UpdateService)+31U)&~31U);
    static const uint32_t invalid[]={0x20000000U,0xC0000000U,ARENA_ADDRESS+1U,0xC4000000U-32U,0xFFFFFFE0U};
    CHECK(RuntimeUpdate_GetService()==NULL);
    RuntimeUpdate_Process(10U);CHECK(g_runtime_update.polls==0U);
    context_fault=1U;CHECK(RuntimeUpdate_Init()==RUNTIME_UPDATE_CONTEXT&&allocation_calls==0U);
    context_fault=0U;CHECK(RuntimeUpdate_Init()==RUNTIME_UPDATE_NOT_READY&&allocation_calls==0U);
    SetReady();g_bsp_nor.jedec_id^=1U;CHECK(RuntimeUpdate_Init()==RUNTIME_UPDATE_NOT_READY);
    SetReady();g_bsp_nor.capacity_bytes-=1U;CHECK(RuntimeUpdate_Init()==RUNTIME_UPDATE_NOT_READY);
    SetReady();g_bsp_ram.result=1U;CHECK(RuntimeUpdate_Init()==RUNTIME_UPDATE_NOT_READY);
    SetReady();g_bsp_ram.capacity_bytes=BSP_RAM_TEST_BYTES;CHECK(RuntimeUpdate_Init()==RUNTIME_UPDATE_NOT_READY);
    SetReady();g_bsp_ram.capacity_bytes=BSP_RAM_GEOMETRY_BYTES+1U;CHECK(RuntimeUpdate_Init()==RUNTIME_UPDATE_NOT_READY);
    SetReady();allocation_address=0U;
    CHECK(RuntimeUpdate_Init()==RUNTIME_UPDATE_NO_MEMORY&&RuntimeUpdate_GetService()==NULL);
    for(i=0U;i<sizeof(invalid)/sizeof(invalid[0]);++i){
        allocation_address=invalid[i];CHECK(RuntimeUpdate_Init()==RUNTIME_UPDATE_BAD_MEMORY);
        CHECK(RuntimeUpdate_GetService()==NULL&&g_runtime_update.ready==0U);
    }
    CHECK(!enables&&!programs&&!erases&&!metadata_commits&&!resets);
    allocation_address=ARENA_ADDRESS;CHECK(RuntimeUpdate_Init()==RUNTIME_UPDATE_OK);
    s=RuntimeUpdate_GetService();CHECK((uintptr_t)s==ARENA_ADDRESS&&g_runtime_update.ready==1U);
    CHECK(allocation_bytes==service_bytes+0x4000U);
    CHECK(g_runtime_update.service_bytes==sizeof(*s)&&g_runtime_update.scratch_bytes==0x4000U);
    CHECK(g_runtime_update.scratch_address==ARENA_ADDRESS+service_bytes);
    CHECK(!s->connected&&!s->authorization&&s->state==UPDATE_IDLE&&!enabled_tx&&!early_visibility);
    CHECK(s->platform.context==s&&s->platform.read&&s->platform.enable&&s->platform.erase4k&&s->platform.program);
    CHECK(s->platform.disable&&s->platform.metadata_read&&s->platform.metadata_commit&&s->platform.reset);
    s->transaction=99U;s->request_write=8U;before=allocation_calls;
    CHECK(RuntimeUpdate_Init()==RUNTIME_UPDATE_OK&&allocation_calls==before&&s->transaction==99U&&s->request_write==8U);
    init_phase=0U;
    return 0U;
}

/* 시험 시나리오의 service 상태만 초기화한다. 제품 Runtime Init을 다시 실행하는
 * 패턴이 아니며 adapter callback/context를 그대로 유지한 독립 fixture 재설정이다. */
static void NewSession(void)
{
    UpdatePlatform platform=s->platform;
    platform.disable(s);UpdateService_Init(s,&platform);
    SetReady();nor_result=metadata_read_result=metadata_commit_result=0U;
    quiet_result=resume_result=0;paused_token=epoch_in_program=epoch_in_erase=epoch_in_quiet=unready_in_quiet=zero_quiet_token=0U;
    memset(metadata,0,sizeof(metadata));metadata[0]=UPDATE_METADATA_RESIDENT;
    UpdateService_SetConnected(s,1U);RuntimeUpdate_Process(++now);
}

/* 함수 포인터를 통한 독립 방어 검사다. 실패하면 BSP 호출 횟수가 늘지 않아야 한다.
 * context/hardware/authorization/transaction/page와 full staging 범위를 확인한다. */
static uint32_t TestCallbacks(void)
{
    uint8_t data[256]={0};uint32_t words[5],before,i;
    NewSession();before=enables;
    CHECK(s->platform.enable(s,42U)==RUNTIME_UPDATE_LOCKED&&enables==before);
    UpdateService_Authorize(s,UPDATE_STAGE_ARM);
    CHECK(s->platform.enable(s,0U)==RUNTIME_UPDATE_LOCKED&&enables==before);
    CHECK(s->platform.enable(NULL,42U)==RUNTIME_UPDATE_NOT_READY&&enables==before);
    CHECK(s->platform.enable(s,42U)==BSP_NOR_OK&&enabled_tx==42U);
    s->transaction=42U;
    CHECK(s->platform.enable(s,43U)==RUNTIME_UPDATE_LOCKED&&enabled_tx==42U);
    before=reads;CHECK(s->platform.read(s,UPDATE_STAGE_BASE-1U,data,1U)==RUNTIME_UPDATE_ARGUMENT);
    CHECK(s->platform.read(s,UPDATE_STAGE_BASE+UPDATE_APP_BYTES,data,1U)==RUNTIME_UPDATE_ARGUMENT);
    CHECK(s->platform.read(s,UINT32_MAX,data,2U)==RUNTIME_UPDATE_ARGUMENT);
    CHECK(s->platform.read(s,UPDATE_STAGE_BASE,data,UPDATE_APP_BYTES+1U)==RUNTIME_UPDATE_ARGUMENT);
    CHECK(s->platform.read(s,UPDATE_STAGE_BASE,NULL,1U)==RUNTIME_UPDATE_ARGUMENT);
    CHECK(s->platform.read(s,UPDATE_STAGE_BASE,data,0U)==RUNTIME_UPDATE_ARGUMENT&&reads==before);
    CHECK(s->platform.read(s,UPDATE_STAGE_BASE+UPDATE_APP_BYTES-1U,data,1U)==BSP_NOR_OK&&reads==before+1U);
    before=programs;
    CHECK(s->platform.program(s,UPDATE_STAGE_BASE,data,0U)==RUNTIME_UPDATE_ARGUMENT);
    CHECK(s->platform.program(s,UPDATE_STAGE_BASE,data,257U)==RUNTIME_UPDATE_ARGUMENT);
    CHECK(s->platform.program(s,UPDATE_STAGE_BASE+255U,data,2U)==RUNTIME_UPDATE_ARGUMENT);
    CHECK(s->platform.program(s,UPDATE_STAGE_BASE-256U,data,256U)==RUNTIME_UPDATE_ARGUMENT);
    CHECK(s->platform.program(s,UPDATE_STAGE_BASE+UPDATE_APP_BYTES-1U,data,2U)==RUNTIME_UPDATE_ARGUMENT);
    CHECK(s->platform.program(s,UPDATE_STAGE_BASE,NULL,1U)==RUNTIME_UPDATE_ARGUMENT&&programs==before);
    before=erases;
    CHECK(s->platform.erase4k(s,UPDATE_STAGE_BASE+1U)==RUNTIME_UPDATE_ARGUMENT);
    CHECK(s->platform.erase4k(s,UPDATE_STAGE_BASE-4096U)==RUNTIME_UPDATE_ARGUMENT);
    CHECK(s->platform.erase4k(s,UPDATE_STAGE_BASE+UPDATE_APP_BYTES)==RUNTIME_UPDATE_ARGUMENT&&erases==before);
    CHECK(s->platform.erase4k(s,UPDATE_STAGE_BASE)==BSP_NOR_OK&&erases==before+1U);
    CHECK(s->platform.program(s,UPDATE_STAGE_BASE,data,256U)==BSP_NOR_OK);
    before=programs;s->transaction=43U;
    CHECK(s->platform.program(s,UPDATE_STAGE_BASE,data,1U)==RUNTIME_UPDATE_LOCKED&&programs==before);
    s->transaction=42U;UpdateService_Authorize(s,0U);
    CHECK(s->platform.program(s,UPDATE_STAGE_BASE,data,1U)==RUNTIME_UPDATE_LOCKED&&programs==before);
    CHECK(s->platform.read(s,UPDATE_STAGE_BASE,data,1U)==BSP_NOR_OK);
    metadata[4]=UINT32_MAX;CHECK(s->platform.metadata_read(s,words)==UPDATE_METADATA_OK&&words[4]==UINT32_MAX);metadata[4]=0U;
    UpdateService_Authorize(s,UPDATE_STAGE_ARM);UpdateService_SetConnected(s,0U);
    CHECK(!s->authorization&&s->platform.erase4k(s,UPDATE_STAGE_BASE)==RUNTIME_UPDATE_LOCKED);
    UpdateService_SetConnected(s,1U);UpdateService_Authorize(s,UPDATE_STAGE_ARM);
    for(i=1U;i<=32U;i<<=1U){
        context_fault=i;before=reads+programs+erases+metadata_reads+metadata_commits+enables;
        CHECK(s->platform.read(s,UPDATE_STAGE_BASE,data,1U)==RUNTIME_UPDATE_NOT_READY);
        CHECK(s->platform.program(s,UPDATE_STAGE_BASE,data,1U)==RUNTIME_UPDATE_NOT_READY);
        CHECK(s->platform.erase4k(s,UPDATE_STAGE_BASE)==RUNTIME_UPDATE_NOT_READY);
        CHECK(s->platform.enable(s,42U)==RUNTIME_UPDATE_NOT_READY);
        CHECK(s->platform.metadata_read(s,words)==RUNTIME_UPDATE_NOT_READY);
        CHECK(s->platform.metadata_commit(s,1U,1U,UPDATE_COMMIT_ARM)==RUNTIME_UPDATE_NOT_READY);
        RuntimeUpdate_Process(++now);CHECK(g_runtime_update.result==RUNTIME_UPDATE_CONTEXT);
        CHECK(reads+programs+erases+metadata_reads+metadata_commits+enables==before);
    }
    context_fault=0U;g_bsp_nor.ready=0U;before=reads;
    CHECK(s->platform.read(s,UPDATE_STAGE_BASE,data,1U)==RUNTIME_UPDATE_NOT_READY&&reads==before);
    before=disables;s->platform.disable(s);CHECK(disables==before+1U&&!enabled_tx&&!g_runtime_update.enabled_transaction);
    SetReady();nor_result=BSP_NOR_TIMEOUT;
    CHECK(s->platform.read(s,UPDATE_STAGE_BASE,data,1U)==BSP_NOR_TIMEOUT&&g_runtime_update.last_platform_result==BSP_NOR_TIMEOUT);
    nor_result=0U;metadata_read_result=UPDATE_METADATA_BUSY;
    CHECK(s->platform.metadata_read(s,words)==UPDATE_METADATA_BUSY);
    metadata_read_result=0U;CHECK(!callback_faults);
    return 0U;
}

/* 검증 완료 상태를 가정하는 좁은 callback fixture다. 아래 실제 전체 stage 시험이
 * 그 상태를 만드는 정상 경로도 별도로 수행한다. */
static void SetVerified(void)
{
    NewSession();s->state=UPDATE_VERIFIED;s->received=s->verified=UPDATE_APP_BYTES;
    s->transaction=77U;s->version=0x10001U;s->expected_crc=RUNTIME_FIXTURE_CRC;
    memcpy(s->expected_sha,runtime_fixture_sha,32U);memcpy(s->actual_sha,runtime_fixture_sha,32U);
    UpdateService_Authorize(s,UPDATE_STAGE_ARM);
}

#if NOODOE_BOOTSTRAP
/* Policy denial must occur before BT pause or metadata mutation; the callback
 * receives a new canonical header read at commit, not a stale cached manifest. */
static uint32_t TestBootstrapGate(void)
{
 uint32_t before,qbefore,calls;SetVerified();gate_denied=1;before=metadata_commits;qbefore=quiet_calls;
 FAKE_NOR[0x10201]=0x51;calls=gate_calls;
 CHECK(s->platform.metadata_commit(s,s->version,s->expected_crc,UPDATE_COMMIT_ARM)==RUNTIME_UPDATE_LOCKED);
 CHECK(metadata_commits==before&&quiet_calls==qbefore&&gate_calls==calls+1);
 CHECK(gate_target==UPDATE_TARGET_CFW&&gate_view&&gate_requirement[0]==0x51&&gate_version==s->version);
 FAKE_NOR[0x10201]=0x52;
 CHECK(s->platform.metadata_commit(s,s->version,s->expected_crc,UPDATE_COMMIT_ARM)==RUNTIME_UPDATE_LOCKED);
 CHECK(gate_requirement[0]==0x52&&metadata_commits==before);
 s->target=UPDATE_TARGET_STOCK;calls=gate_calls;
 CHECK(s->platform.metadata_commit(s,s->version,s->expected_crc,UPDATE_COMMIT_ARM)==RUNTIME_UPDATE_LOCKED);
 CHECK(gate_calls==calls&&metadata_commits==before);
 s->version=RECOVERY_STOCK_VERSION;memcpy(s->expected_sha,recovery_stock_sha256,32);memcpy(s->actual_sha,recovery_stock_sha256,32);
 CHECK(s->platform.metadata_commit(s,s->version,s->expected_crc,UPDATE_COMMIT_ARM)==RUNTIME_UPDATE_LOCKED);
 CHECK(gate_calls==calls+1&&!gate_view&&gate_target==UPDATE_TARGET_STOCK);
 gate_denied=0;metadata_commit_result=UPDATE_METADATA_AMBIGUOUS;
 CHECK(s->platform.metadata_commit(s,s->version,s->expected_crc,UPDATE_COMMIT_ARM)==UPDATE_METADATA_AMBIGUOUS);
 CHECK(metadata_commits==before+1&&gate_version==RECOVERY_STOCK_VERSION&&!memcmp(gate_sha,recovery_stock_sha256,32));
 SetVerified();s->target=2;calls=gate_calls;before=metadata_commits;
 CHECK(s->platform.metadata_commit(s,s->version,s->expected_crc,UPDATE_COMMIT_ARM)==RUNTIME_UPDATE_LOCKED);
 CHECK(gate_calls==calls&&metadata_commits==before);return 0;
}
#endif

/* 최종 callback의 독립 COMT/상태/길이/SHA 방어와 내부 token 변환을 확인한다.
 * metadata 오류를 성공으로 바꾸거나 scratch를 service와 겹치게 넘기면 실패한다. */
static uint32_t TestMetadataGuards(void)
{
    uint32_t before,crc=RUNTIME_FIXTURE_CRC;
    SetVerified();before=metadata_commits;
    CHECK(s->platform.metadata_commit(s,s->version,crc,UPDATE_METADATA_ARM_TOKEN)==RUNTIME_UPDATE_LOCKED);
    s->state=UPDATE_RECEIVING;CHECK(s->platform.metadata_commit(s,s->version,crc,UPDATE_COMMIT_ARM)==RUNTIME_UPDATE_LOCKED);s->state=UPDATE_VERIFIED;
    --s->received;CHECK(s->platform.metadata_commit(s,s->version,crc,UPDATE_COMMIT_ARM)==RUNTIME_UPDATE_LOCKED);++s->received;
    --s->verified;CHECK(s->platform.metadata_commit(s,s->version,crc,UPDATE_COMMIT_ARM)==RUNTIME_UPDATE_LOCKED);++s->verified;
    s->actual_sha[31]^=1U;CHECK(s->platform.metadata_commit(s,s->version,crc,UPDATE_COMMIT_ARM)==RUNTIME_UPDATE_LOCKED);s->actual_sha[31]^=1U;
    CHECK(s->platform.metadata_commit(s,s->version+1U,crc,UPDATE_COMMIT_ARM)==RUNTIME_UPDATE_LOCKED);
    CHECK(s->platform.metadata_commit(s,s->version,crc^1U,UPDATE_COMMIT_ARM)==RUNTIME_UPDATE_LOCKED);
    CHECK(s->platform.metadata_commit(s,s->version,0U,UPDATE_COMMIT_ARM)==RUNTIME_UPDATE_LOCKED);
    CHECK(s->platform.metadata_commit(s,s->version,UINT32_MAX,UPDATE_COMMIT_ARM)==RUNTIME_UPDATE_LOCKED);
    UpdateService_Authorize(s,0U);CHECK(s->platform.metadata_commit(s,s->version,crc,UPDATE_COMMIT_ARM)==RUNTIME_UPDATE_LOCKED);
    UpdateService_Authorize(s,UPDATE_STAGE_ARM);g_bsp_ram.ready=0U;
    CHECK(s->platform.metadata_commit(s,s->version,crc,UPDATE_COMMIT_ARM)==RUNTIME_UPDATE_NOT_READY);SetReady();
    CHECK(metadata_commits==before);
    metadata_commit_result=UPDATE_METADATA_AMBIGUOUS;
    CHECK(s->platform.metadata_commit(s,s->version,crc,UPDATE_COMMIT_ARM)==UPDATE_METADATA_AMBIGUOUS);
    CHECK(metadata_commits==before+1U&&last_commit_token==UPDATE_METADATA_ARM_TOKEN);
    CHECK(last_commit_version==s->version&&last_commit_crc==crc&&last_commit_bytes==0x4000U);
    CHECK(last_commit_scratch>=ARENA_ADDRESS+sizeof(*s)&&last_commit_scratch==g_runtime_update.scratch_address);
    CHECK(!enabled_tx&&!callback_faults&&!resets);
    return 0U;
}

/* quiesce 실패면 writer/resume를 호출하지 않는다. 성공 pause 이후에는 모든 결과에서
 * resume를 호출하고 writer 오류가 resume 오류보다 우선 보존되는지 검사한다. */
static uint32_t TestPauseGuards(void)
{
    uint32_t before,resume_before,quiet_before,result;
    SetVerified();before=metadata_commits;resume_before=resume_calls;quiet_before=quiet_calls;
    quiet_result=BLUETOOTH_BUSY;
    result=s->platform.metadata_commit(s,s->version,s->expected_crc,UPDATE_COMMIT_ARM);
    CHECK(result==(uint32_t)BLUETOOTH_BUSY&&quiet_calls==quiet_before+1U);
    CHECK(metadata_commits==before&&resume_calls==resume_before&&!paused_token);
    SetVerified();epoch_in_quiet=1U;before=metadata_commits;resume_before=resume_calls;
    CHECK(s->platform.metadata_commit(s,s->version,s->expected_crc,UPDATE_COMMIT_ARM)==RUNTIME_UPDATE_LOCKED);
    CHECK(metadata_commits==before&&resume_calls==resume_before+1U&&!paused_token);
    CHECK(s->link_generation!=s->generation);
    SetVerified();unready_in_quiet=1U;before=metadata_commits;resume_before=resume_calls;
    CHECK(s->platform.metadata_commit(s,s->version,s->expected_crc,UPDATE_COMMIT_ARM)==RUNTIME_UPDATE_LOCKED);
    CHECK(metadata_commits==before&&resume_calls==resume_before+1U&&!paused_token);
    SetVerified();zero_quiet_token=1U;before=metadata_commits;resume_before=resume_calls;
    CHECK(s->platform.metadata_commit(s,s->version,s->expected_crc,UPDATE_COMMIT_ARM)!=(uint32_t)BSP_NOR_OK);
    CHECK(metadata_commits==before&&resume_calls==resume_before+1U);
    SetVerified();metadata_commit_result=UPDATE_METADATA_AMBIGUOUS;resume_result=BLUETOOTH_NOT_READY;
    before=metadata_commits;resume_before=resume_calls;
    CHECK(s->platform.metadata_commit(s,s->version,s->expected_crc,UPDATE_COMMIT_ARM)==UPDATE_METADATA_AMBIGUOUS);
    CHECK(metadata_commits==before+1U&&resume_calls==resume_before+1U&&!paused_token);
    SetVerified();resume_result=BLUETOOTH_NOT_READY;before=metadata_commits;resume_before=resume_calls;
    CHECK(s->platform.metadata_commit(s,s->version,s->expected_crc,UPDATE_COMMIT_ARM)==0);
    CHECK(g_runtime_update.resume_result==(uint32_t)BLUETOOTH_NOT_READY);
    CHECK(metadata_commits==before+1U&&resume_calls==resume_before+1U&&!paused_token);
    CHECK(metadata[4]==RUNTIME_FIXTURE_CRC&&!callback_faults);
    return 0U;
}

/* queue→실제 Runtime Process→NDCP response를 모두 거친다. 단순 callback 직접 호출과
 * 구분하며 응답의 sequence와 ISO framing CRC까지 확인한다. */
static uint32_t Command(uint32_t opcode,const uint8_t *payload,uint32_t length,uint32_t expected)
{
    NDCP_Frame frame={opcode,0U,++sequence,length,payload};size_t count;
    CHECK(UpdateService_Handle(s,&frame)==UPDATE_OK);RuntimeUpdate_Process(++now);
    count=UpdateService_TakeReply(s,reply,sizeof(reply));CHECK(count>=40U);
    test_command_opcode=opcode;test_command_expected=expected;test_command_actual=Get(reply+16U);
    CHECK(Get(reply+8U)==sequence&&Get(reply+16U)==expected);
    CHECK(NDCP_Crc32(reply,count-4U)==Get(reply+count-4U));return 0U;
}

/* 실제DATA의 첫 page/erase 안에서 disconnect→reconnect→재승인을 주입한다.
 * 새 연결의 승인으로 예전 DATA 다음page가 계속되지 않고 Fail로 닫혀야 한다. */
static uint32_t TestEpochBetweenWrites(void)
{
    uint32_t variant,before,result;
    for(variant=0U;variant<2U;++variant){
        NewSession();memset(manifest,0,sizeof(manifest));Put(manifest,101U);
        Put(manifest+4U,0x10001U);Put(manifest+8U,UPDATE_APP_BYTES);Put(manifest+12U,RUNTIME_FIXTURE_CRC);
        memcpy(manifest+16U,runtime_fixture_sha,32U);Put(manifest+48U,UPDATE_STAGE_ARM);
        UpdateService_Authorize(s,UPDATE_STAGE_ARM);
        result=Command(UPDATE_OP_BEGIN,manifest,sizeof(manifest),UPDATE_OK);if(result)return result;
        Put(wire_data,101U);Put(wire_data+4U,0U);memset(wire_data+8U,0,512U);
        before=programs;epoch_in_program=variant==0U;epoch_in_erase=variant==1U;
        /* 이전 epoch 응답은 TakeReply에서 의도적으로 버려진다. 따라서 Command의
         * 응답 기대 helper 대신 Handle/Process 상태와 BSP 횟수만 관측한다. */
        NDCP_Frame frame={UPDATE_OP_DATA,0U,++sequence,520U,wire_data};
        CHECK(UpdateService_Handle(s,&frame)==UPDATE_OK);RuntimeUpdate_Process(++now);
        CHECK(s->state==UPDATE_FAILED&&!s->authorization&&!enabled_tx&&s->received==0U);
        CHECK(programs==before+(variant==0U?1U:0U));
        CHECK(UpdateService_TakeReply(s,reply,sizeof(reply))==0U);
        RuntimeUpdate_Process(++now);CHECK(programs==before+(variant==0U?1U:0U));
    }
    return 0U;
}

/* 실제 service가 metadata 모호 오류를 받으면 권한을 닫고 자동 재시도/reset을 하지
 * 않는지 확인한다. callback 직접 시험과 달리 여기서는 Fail 상태 전이도 실제 코드다. */
static uint32_t TestAmbiguous(void)
{
    uint32_t result,before;
    SetVerified();Put(wire_data,77U);Put(wire_data+4U,UPDATE_COMMIT_ARM);memcpy(wire_data+8U,runtime_fixture_sha,32U);
    metadata_commit_result=UPDATE_METADATA_AMBIGUOUS;before=metadata_commits;
    result=Command(UPDATE_OP_COMMIT,wire_data,40U,UPDATE_COMMIT_AMBIGUOUS);if(result)return result;
    CHECK(s->state==UPDATE_FAILED&&!s->authorization&&!enabled_tx&&metadata_commits==before+1U);
    RuntimeUpdate_Process(now+10000U);CHECK(metadata_commits==before+1U&&!resets);
    result=Command(UPDATE_OP_COMMIT,wire_data,40U,UPDATE_STATE);if(result)return result;
    CHECK(metadata_commits==before+1U&&s->result==UPDATE_COMMIT_AMBIGUOUS&&s->commit_uncertain);
    result=Command(UPDATE_OP_STATUS,0,0,UPDATE_OK);if(result)return result;
    CHECK(s->result==UPDATE_COMMIT_AMBIGUOUS&&s->commit_uncertain);
    result=Command(UPDATE_OP_ABORT,wire_data,4,UPDATE_PENDING);if(result)return result;
    CHECK(BootstrapRecovery_Drain(s,1,0)==BOOT_DRAIN_UNRESOLVED);
    CHECK(BootstrapRecovery_Drain(s,1,60001)==BOOT_DRAIN_UNRESOLVED);
    CHECK(s->state==UPDATE_FAILED&&metadata_commits==before+1&&!resets);
    return 0U;
}

/* Exact field failure class: a refusal before S2 erase used to latch ambiguity
 * and drive the local restore loop into its deliberate 60s watchdog failure. */
static uint32_t TestRejectedRecovery(void)
{
    for(uint32_t variant=0;variant<3;variant++){
        SetVerified();uint32_t before=metadata_commits,checked=flash_checks;
        Put(wire_data,77);Put(wire_data+4,UPDATE_COMMIT_ARM);memcpy(wire_data+8,runtime_fixture_sha,32);
        if(variant==0)quiet_result=BLUETOOTH_BUSY;
        if(variant==1)flash_denied=1;
        if(variant==2)metadata_commit_result=UPDATE_METADATA_HARDWARE;
        uint32_t r=Command(UPDATE_OP_COMMIT,wire_data,40,UPDATE_COMMIT_REJECTED);if(r)return r;
        CHECK(s->state==UPDATE_FAILED&&!s->commit_uncertain&&!s->authorization);
        CHECK(metadata_commits==before+(variant==2)&&flash_checks==checked+1+(variant==2));
        r=Command(UPDATE_OP_STATUS,0,0,UPDATE_OK);if(r)return r;
        CHECK(s->result==UPDATE_COMMIT_REJECTED);
        CHECK(BootstrapRecovery_Drain(s,0,59999)==BOOT_DRAIN_WAIT);
        CHECK(BootstrapRecovery_Drain(s,0,60000)==BOOT_DRAIN_TIMEOUT);
        CHECK(BootstrapRecovery_Drain(s,1,10)==BOOT_DRAIN_READY);
        CHECK(s->state==UPDATE_IDLE&&!resets);flash_denied=0;
    }
    /* Resume failure AFTER a successful commit must never become cancellable. */
    SetVerified();resume_result=BLUETOOTH_BUSY;
    Put(wire_data,77);Put(wire_data+4,UPDATE_COMMIT_ARM);memcpy(wire_data+8,runtime_fixture_sha,32);
    uint32_t r=Command(UPDATE_OP_COMMIT,wire_data,40,UPDATE_OK);if(r)return r;
    CHECK(metadata[4]==RUNTIME_FIXTURE_CRC&&!s->commit_uncertain&&s->state==UPDATE_COMMITTED);
    r=Command(UPDATE_OP_STATUS,0,0,UPDATE_OK);if(r)return r;
    CHECK(BootstrapRecovery_Drain(s,1,0)==BOOT_DRAIN_UNRESOLVED&&!resets);
    return 0;
}

/* Python hashlib/zlib fixture와 같은 byte 생성 규칙이다. 처음8bytes는 실제 APP vector다. */
static uint8_t ImageByte(uint32_t offset)
{static const uint8_t vector[8]={0U,0U,2U,0x20U,1U,1U,1U,8U};return offset<8U?vector[offset]:(uint8_t)(offset*13U+7U);}

/* 주소 parity와 무관한 독립 기대값은 physical[p]=logical[p xor1]이다. 이 helper는
 * 제품 codec을 호출하지 않으며 지정한 raw 범위의 전체 byte를 순회한다. */
static uint32_t RawImageMatches(uint32_t length)
{
    uint32_t i;
    for(i=0U;i<length;++i)if(FAKE_NOR[i]!=ImageByte(i^1U))return 0U;
    return 1U;
}

/* 홀수 시작/길이, page 끝과 staging 양 끝에서 실제 callback을 호출한다. 소스는
 * canary 안쪽의 일부러 홀수 RAM 주소다. RMW용 이웃 read가 없고 물리 program 합계가
 * 요청 byte 수와 같으며 요청 외 물리 byte는 A5 그대로인지 별도 oracle로 검사한다. */
static uint32_t TestPhysicalCodec(void)
{
    static const uint32_t offsets[]={0U,1U,2U,3U,253U,254U,255U,256U,257U,
        4095U,4096U,UPDATE_APP_BYTES-257U,UPDATE_APP_BYTES-256U,
        UPDATE_APP_BYTES-3U,UPDATE_APP_BYTES-2U,UPDATE_APP_BYTES-1U};
    static const uint32_t lengths[]={1U,2U,3U,7U,127U,254U,255U,256U};
    uint8_t source[258],destination[515];
    uint32_t oi,li,i,j,at,n,page,before_reads,before_programs,total,expected,logical;
    NewSession();UpdateService_Authorize(s,UPDATE_STAGE_ARM);
    CHECK(s->platform.enable(s,203U)==BSP_NOR_OK);s->transaction=203U;
    for(oi=0U;oi<sizeof(offsets)/sizeof(offsets[0]);++oi){
        at=offsets[oi];page=at&~255U;
        for(li=0U;li<sizeof(lengths)/sizeof(lengths[0]);++li){
            n=lengths[li];if(n>256U-(at&255U)||n>UPDATE_APP_BYTES-at)continue;
            memset(FAKE_NOR+page,0xA5U,256U);memset(source,0xC3U,sizeof(source));
            for(i=0U;i<n;++i){source[i+1U]=ImageByte(at+i);FAKE_NOR[(at+i)^1U]=255U;}
            before_reads=reads;before_programs=programs;program_trace_count=0U;
            CHECK(s->platform.program(s,UPDATE_STAGE_BASE+at,source+1U,n)==BSP_NOR_OK);
            CHECK(reads==before_reads&&program_trace_count>=1U&&program_trace_count<=3U);
            CHECK(programs==before_programs+program_trace_count&&source[0]==0xC3U&&source[n+1U]==0xC3U);
            total=0U;
            for(i=0U;i<program_trace_count;++i){
                total+=program_length[i];
                for(j=0U;j<program_length[i];++j){
                    logical=(program_address[i]+j-UPDATE_STAGE_BASE)^1U;
                    CHECK(logical>=at&&logical-at<n);
                }
            }
            CHECK(total==n);
            for(i=0U;i<256U;++i){
                logical=(page+i)^1U;
                expected=logical>=at&&logical-at<n?ImageByte(logical):0xA5U;
                CHECK(FAKE_NOR[page+i]==expected);
            }
            memset(destination,0x5AU,sizeof(destination));
            CHECK(s->platform.read(s,UPDATE_STAGE_BASE+at,destination+1U,n)==BSP_NOR_OK);
            CHECK(memcmp(destination+1U,source+1U,n)==0&&destination[0]==0x5AU&&destination[n+1U]==0x5AU);
        }
    }
    /* 읽기는 page 경계를 넘어도 합법이다. 쓰기 승인 없이 raw에서 canonical 순서로
     * 복원하며 staging 마지막 byte의 xor1 주소도 밖으로 벗어나지 않아야 한다. */
    s->platform.disable(s);UpdateService_Authorize(s,0U);
    for(i=0U;i<UPDATE_APP_BYTES;++i)FAKE_NOR[i]=ImageByte(i^1U);
    for(oi=0U;oi<sizeof(offsets)/sizeof(offsets[0]);++oi){
        at=offsets[oi];
        for(li=0U;li<sizeof(lengths)/sizeof(lengths[0]);++li){
            n=lengths[li];if(n>UPDATE_APP_BYTES-at)continue;
            memset(destination,0x5AU,sizeof(destination));
            CHECK(s->platform.read(s,UPDATE_STAGE_BASE+at,destination+1U,n)==BSP_NOR_OK);
            for(i=0U;i<n;++i)CHECK(destination[i+1U]==ImageByte(at+i));
            CHECK(destination[0]==0x5AU&&destination[n+1U]==0x5AU);
        }
    }
    CHECK(!callback_faults);return 0U;
}

/* 한 논리 program 안의 첫/둘째 물리 호출에서 재접속과 재승인을 주입한다.
 * 이미 끝난 byte만 남고 나머지 요청 byte는 FF여야 한다. 페이지 경계가 아닌
 * pair-swap lead/middle/tail 사이에서도 예전 epoch의 권한을 재사용하면 실패한다. */
static uint32_t TestEpochInsideCodec(void)
{
    uint8_t data[8]={0x11U,0x23U,0x45U,0x67U,0x89U,0xABU,0xCDU,0xEFU};
    uint32_t variant,i,j,physical,expected,before_reads,before_programs;
    for(variant=1U;variant<=2U;++variant){
        NewSession();UpdateService_Authorize(s,UPDATE_STAGE_ARM);
        CHECK(s->platform.enable(s,204U)==BSP_NOR_OK);s->transaction=204U;
        memset(FAKE_NOR,255U,256U);program_trace_count=0U;
        epoch_in_program=variant;before_reads=reads;before_programs=programs;
        CHECK(s->platform.program(s,UPDATE_STAGE_BASE+1U,data,6U)==RUNTIME_UPDATE_LOCKED);
        CHECK(programs==before_programs+variant&&program_trace_count==variant&&reads==before_reads);
        CHECK(s->connected&&s->authorization&&s->link_generation!=s->generation);
        CHECK(program_address[0]==UPDATE_STAGE_BASE&&program_length[0]==1U);
        if(variant==2U)CHECK(program_address[1]==UPDATE_STAGE_BASE+2U&&program_length[1]==4U);
        for(i=0U;i<256U;++i){
            expected=255U;
            for(j=0U;j<variant;++j){
                physical=program_address[j]-UPDATE_STAGE_BASE;
                if(i>=physical&&i-physical<program_length[j])expected=data[(i^1U)-1U];
            }
            CHECK(FAKE_NOR[i]==expected);
        }
        CHECK(s->platform.program(s,UPDATE_STAGE_BASE+8U,data,1U)==RUNTIME_UPDATE_LOCKED);
        CHECK(programs==before_programs+variant);
    }
    CHECK(!callback_faults);return 0U;
}

/* Wire DATA follows the current even-byte/4KiB checkpoint contract. The raw
 * adapter's odd physical accesses are covered independently above. 같은
 * chunk 재전송은 논리 readback으로만 확인하고 raw NOR를 다시 쓰거나 지우면 안 된다.
 * READ_STAGE 응답도 wire canonical byte여야 하며 root/앨범/실제 NOR는 사용하지 않는다. */
static uint32_t TestOddDataAndReads(void)
{
    static const uint32_t chunks[]={2U,2U,252U,254U,258U,4U,512U,510U,8U,514U,510U,254U,2U,256U,258U,512U,250U};
    static const uint32_t read_cases[][2]={{0U,1U},{1U,1U},{1U,3U},{254U,3U},{255U,512U},
        {4095U,2U},{UPDATE_APP_BYTES-3U,3U},{UPDATE_APP_BYTES-1U,1U}};
    uint32_t ci,offset=0U,n,i,result,before_programs,before_erases,before_reads;
    NewSession();memset(FAKE_NOR,255U,UPDATE_APP_BYTES);memset(manifest,0,sizeof(manifest));
    Put(manifest,205U);Put(manifest+4U,0x20003U);Put(manifest+8U,UPDATE_APP_BYTES);
    Put(manifest+12U,RUNTIME_FIXTURE_CRC);memcpy(manifest+16U,runtime_fixture_sha,32U);Put(manifest+48U,UPDATE_STAGE_ARM);
    UpdateService_Authorize(s,UPDATE_STAGE_ARM);
    result=Command(UPDATE_OP_BEGIN,manifest,sizeof(manifest),UPDATE_OK);if(result)return result;
    for(ci=0U;ci<sizeof(chunks)/sizeof(chunks[0]);++ci){
        n=chunks[ci];if(n>4096U-(offset&4095U))n=4096U-(offset&4095U);
        Put(wire_data,205U);Put(wire_data+4U,offset);
        for(i=0U;i<n;++i)wire_data[8U+i]=ImageByte(offset+i);
        result=Command(UPDATE_OP_DATA,wire_data,n+8U,UPDATE_OK);if(result)return result;
        CHECK(s->received==offset+n);
        before_programs=programs;before_erases=erases;before_reads=reads;
        result=Command(UPDATE_OP_DATA,wire_data,n+8U,UPDATE_OK);if(result)return result;
        CHECK(programs==before_programs&&erases==before_erases&&reads>before_reads&&s->received==offset+n);
        if(ci==3U){
            wire_data[8U]^=1U;result=Command(UPDATE_OP_DATA,wire_data,n+8U,UPDATE_OFFSET);if(result)return result;
            CHECK(programs==before_programs&&erases==before_erases&&s->state==UPDATE_RECEIVING);
        }
        offset+=n;
    }
    CHECK(offset>4096U);
    for(i=0U;i<offset;++i)CHECK(FAKE_NOR[i^1U]==ImageByte(i));
    UpdateService_Authorize(s,0U);before_programs=programs;before_erases=erases;
    for(ci=0U;ci<sizeof(read_cases)/sizeof(read_cases[0]);++ci){
        offset=read_cases[ci][0];n=read_cases[ci][1];Put(wire_data,offset);Put(wire_data+4U,n);
        result=Command(UPDATE_OP_READ_STAGE,wire_data,8U,UPDATE_OK);if(result)return result;
        CHECK(Get(reply+36U)==offset&&Get(reply+40U)==n);
        for(i=0U;i<n;++i)CHECK(reply[44U+i]==FAKE_NOR[(offset+i)^1U]);
    }
    Put(wire_data,UPDATE_APP_BYTES-1U);Put(wire_data+4U,2U);
    result=Command(UPDATE_OP_READ_STAGE,wire_data,8U,UPDATE_ARGUMENT);if(result)return result;
    CHECK(programs==before_programs&&erases==before_erases&&!callback_faults);
    return 0U;
}

/* 전체448KiB를 실제 adapter callback으로 써서 FINISH의 전체readback SHA/ISO CRC,
 * COMMIT의token 변환,RESET grace까지 정상 경로를 검사한다. 모든 저장 대상은RAM이다. */
static uint32_t TestEndToEnd(void)
{
    uint32_t result,offset,i,before_erase,before_program,before_commit,before_reset,ack;size_t count;
    NDCP_Frame finish;
    NewSession();memset(FAKE_NOR,0xA5U,UPDATE_APP_BYTES);memset(manifest,0,sizeof(manifest));
    Put(manifest,88U);Put(manifest+4U,0x20002U);Put(manifest+8U,UPDATE_APP_BYTES);
    Put(manifest+12U,RUNTIME_FIXTURE_CRC);memcpy(manifest+16U,runtime_fixture_sha,32U);Put(manifest+48U,UPDATE_STAGE_ARM);
    before_erase=erases;before_program=programs;before_commit=metadata_commits;
    result=Command(UPDATE_OP_BEGIN,manifest,56U,UPDATE_LOCKED);if(result)return result;
    CHECK(erases==before_erase&&programs==before_program&&metadata_commits==before_commit);
    UpdateService_Authorize(s,UPDATE_STAGE_ARM);
    result=Command(UPDATE_OP_BEGIN,manifest,56U,UPDATE_OK);if(result)return result;
    CHECK(enabled_tx==88U);
    for(offset=0U;offset<UPDATE_APP_BYTES;offset+=512U){
        Put(wire_data,88U);Put(wire_data+4U,offset);
        for(i=0U;i<512U;++i)wire_data[8U+i]=ImageByte(offset+i);
        result=Command(UPDATE_OP_DATA,wire_data,520U,UPDATE_OK);if(result)return result;
    }
    CHECK(erases==before_erase+112U&&programs==before_program+1792U&&metadata_commits==before_commit);
    CHECK(RawImageMatches(UPDATE_APP_BYTES));
    /* Last DATA reached NOR, but the phone lost its socket before FINISH.
     * The same in-memory manifest admits readback without any retransmission. */
    UpdateService_SetConnected(s,0);RuntimeUpdate_Process(++now);
    CHECK(s->state==UPDATE_FAILED&&s->result==UPDATE_NOT_CONNECTED);
    UpdateService_SetConnected(s,1);RuntimeUpdate_Process(++now);
    Put(wire_data,88U);finish=(NDCP_Frame){UPDATE_OP_FINISH,0U,++sequence,4U,wire_data};
    CHECK(UpdateService_Handle(s,&finish)==UPDATE_OK);
    for(i=0U;i<112U;++i)RuntimeUpdate_Process(++now);
    CHECK(s->state==UPDATE_VERIFIED&&s->received==UPDATE_APP_BYTES&&s->verified==UPDATE_APP_BYTES&&!enabled_tx);
    CHECK(memcmp(s->actual_sha,runtime_fixture_sha,32U)==0);
    count=UpdateService_TakeReply(s,reply,sizeof(reply));CHECK(count==72U&&Get(reply+16U)==UPDATE_OK);
    /* Reconnect retry asks for a fresh whole-image read, not another BEGIN
     * or any NOR erase/program. This is the phone's no-retransfer path. */
    before_erase=erases;before_program=programs;finish.sequence=++sequence;
    CHECK(UpdateService_Handle(s,&finish)==UPDATE_OK);
    for(i=0;i<112;i++)RuntimeUpdate_Process(++now);
    CHECK(s->state==UPDATE_VERIFIED&&s->verified==UPDATE_APP_BYTES);
    count=UpdateService_TakeReply(s,reply,sizeof(reply));CHECK(count==72U&&Get(reply+16U)==UPDATE_OK);
    CHECK(erases==before_erase&&programs==before_program);
    UpdateService_SetConnected(s,0U);RuntimeUpdate_Process(++now);
    CHECK(s->state==UPDATE_VERIFIED&&!s->authorization);
    UpdateService_SetConnected(s,1U);RuntimeUpdate_Process(++now);
    Put(wire_data+4U,UPDATE_COMMIT_ARM);memcpy(wire_data+8U,runtime_fixture_sha,32U);
    result=Command(UPDATE_OP_COMMIT,wire_data,40U,UPDATE_LOCKED);if(result)return result;
    CHECK(metadata_commits==before_commit);
    UpdateService_Authorize(s,UPDATE_STAGE_ARM);
    result=Command(UPDATE_OP_COMMIT,wire_data,40U,UPDATE_OK);if(result)return result;
    CHECK(s->state==UPDATE_COMMITTED&&!s->authorization&&metadata_commits==before_commit+1U);
    CHECK(!s->commit_uncertain);
    CHECK(last_commit_crc==RUNTIME_FIXTURE_CRC&&metadata[4]==RUNTIME_FIXTURE_CRC&&!enabled_tx);
    /* ACK가 없거나 다른seq이면 reset하지 않는다. uint32 시간 wrap도 검사한다. */
    Put(wire_data+4U,UPDATE_RESET_ARM);result=Command(UPDATE_OP_RESET,wire_data,8U,UPDATE_OK);if(result)return result;
    before_reset=resets;RuntimeUpdate_Process(now+10000U);CHECK(resets==before_reset);
    UpdateService_NotifyReplyTransmitted(s,sequence+1U,now);RuntimeUpdate_Process(now+10000U);CHECK(resets==before_reset);
    ack=0xFFFFFF00UL;UpdateService_NotifyReplyTransmitted(s,sequence,ack);
    UpdateService_NotifyReplyTransmitted(s,sequence+1U,ack+1000U);
    UpdateService_NotifyReplyTransmitted(s,sequence,ack+1200U);
    CHECK(s->sent_ms==ack&&s->sent_sequence==sequence);
    RuntimeUpdate_Process(ack+1499U);CHECK(resets==before_reset);
    RuntimeUpdate_Process(ack+1500U);CHECK(resets==before_reset+1U&&s->state==UPDATE_FAILED&&s->commit_uncertain);
    RuntimeUpdate_Process(ack+2000U);CHECK(resets==before_reset+1U);
    /* Each independent fixture starts committed. A returning reset latches an
     * error; only the test resets fixture state between fault injections. */
    for(i=0U;i<5U;++i){
        uint32_t saved=metadata[i];before_reset=resets;
        s->state=UPDATE_COMMITTED;s->commit_uncertain=0;s->result=UPDATE_OK;
        result=Command(UPDATE_OP_RESET,wire_data,8U,UPDATE_OK);if(result)return result;
        UpdateService_NotifyReplyTransmitted(s,sequence,now);metadata[i]^=1U;
        RuntimeUpdate_Process(now+1500U);CHECK(resets==before_reset&&s->state==UPDATE_FAILED&&s->commit_uncertain);metadata[i]=saved;
    }
    s->state=UPDATE_COMMITTED;s->commit_uncertain=0;s->result=UPDATE_OK;
    result=Command(UPDATE_OP_RESET,wire_data,8U,UPDATE_OK);if(result)return result;
    UpdateService_NotifyReplyTransmitted(s,sequence,now);metadata_read_result=UPDATE_METADATA_HARDWARE;before_reset=resets;
    RuntimeUpdate_Process(now+1500U);CHECK(resets==before_reset&&s->state==UPDATE_FAILED&&s->commit_uncertain);metadata_read_result=0U;
    s->state=UPDATE_COMMITTED;s->commit_uncertain=0;s->result=UPDATE_OK;
    result=Command(UPDATE_OP_RESET,wire_data,8U,UPDATE_OK);if(result)return result;
    UpdateService_NotifyReplyTransmitted(s,sequence,now);UpdateService_SetConnected(s,0U);
    /* An explicit, acknowledged reboot belongs to the committed transaction,
     * not the continued lifetime of the RFCOMM socket. */
    RuntimeUpdate_Process(now+100U);CHECK(resets==before_reset&&s->state==UPDATE_RESET_WAIT);
    RuntimeUpdate_Process(now+1500U);CHECK(resets==before_reset+1U&&s->state==UPDATE_FAILED&&s->commit_uncertain);
    /* Disconnect BEFORE a matching local ACK never invents authorization. */
    s->state=UPDATE_COMMITTED;s->commit_uncertain=0;s->result=UPDATE_OK;
    UpdateService_SetConnected(s,1U);RuntimeUpdate_Process(++now);
    result=Command(UPDATE_OP_RESET,wire_data,8U,UPDATE_OK);if(result)return result;
    before_reset=resets;UpdateService_SetConnected(s,0U);
    UpdateService_NotifyReplyTransmitted(s,sequence,now);
    RuntimeUpdate_Process(now+5000U);CHECK(resets==before_reset&&!s->sent_valid&&s->state==UPDATE_COMMITTED);
    CHECK(!callback_faults);return 0U;
}

/* Local approval must survive a dead phone, but never a changed NOR image.
 * The real adapter rechecks metadata immediately before its reset callback. */
static uint32_t TestLocalInstall(void)
{
    for(uint32_t mode=0;mode<4;mode++){
        NewSession();
        for(uint32_t i=0;i<UPDATE_APP_BYTES;i++)FAKE_NOR[i^1U]=ImageByte(i);
        s->transaction=902;s->version=0x20002;s->expected_crc=RUNTIME_FIXTURE_CRC;
        memcpy(s->expected_sha,runtime_fixture_sha,32);memcpy(s->actual_sha,runtime_fixture_sha,32);
        s->state=UPDATE_VERIFIED;s->received=s->verified=UPDATE_APP_BYTES;
        uint32_t bc=metadata_commits,br=resets,be=erases,bp=programs;
        if(mode==3){s->verified--;CHECK(UpdateService_ConfirmLocal(s)==UPDATE_STATE);continue;}
        CHECK(UpdateService_ConfirmLocal(s)==UPDATE_OK);
        CHECK(UpdateService_ConfirmLocal(s)==UPDATE_STATE);
        if(mode==0)s->reply_write=s->reply_read+UPDATE_QUEUE_DEPTH;
        UpdateService_SetConnected(s,0);
        if(mode==1)FAKE_NOR[450000]^=1;
        for(uint32_t i=0;i<112;i++)RuntimeUpdate_Process(++now);
        if(mode==1){CHECK(s->state==UPDATE_FAILED&&s->result==UPDATE_HASH&&metadata_commits==bc&&resets==br);continue;}
        CHECK(s->state==UPDATE_VERIFIED&&s->verified==UPDATE_APP_BYTES);
        if(mode==0)resume_result=BLUETOOTH_NOT_READY;
        RuntimeUpdate_Process(++now);
        CHECK(s->state==UPDATE_COMMITTED&&metadata_commits==bc+1&&!s->commit_uncertain);
        CHECK(!s->sent_valid); /* Local user approval is not a fabricated ACK. */
        uint32_t committed=now;
        RuntimeUpdate_Process(committed+1499);CHECK(resets==br);
        if(mode==2)metadata[4]^=1;
        RuntimeUpdate_Process(committed+1500);
        CHECK(resets==br+(mode==0));
        /* Reset stubs return; real NVIC reset does not. A returning adapter
         * becomes an explicit error rather than a forever-installing screen. */
        CHECK(s->state==UPDATE_FAILED&&s->commit_uncertain);
        RuntimeUpdate_Process(committed+60000);CHECK(metadata_commits==bc+1&&resets==br+(mode==0));
        CHECK(erases==be&&programs==bp);
    }
    for(uint32_t i=0;i<UPDATE_APP_BYTES;i++)FAKE_NOR[i^1U]=ImageByte(i);
    return 0;
}

/* adapter와 service의 실제ARM 함수를 실행하며 최초 실패 위치를 반환한다. */
uint32_t runtime_update_test_main(void)
{
    uint32_t result;
    result=TestInit();if(result)return result;
    result=TestCallbacks();if(result)return result;
#if NOODOE_BOOTSTRAP
    result=TestBootstrapGate();if(result)return result;
#endif
    result=TestMetadataGuards();if(result)return result;
    result=TestPauseGuards();if(result)return result;
    result=TestEpochBetweenWrites();if(result)return result;
    result=TestAmbiguous();if(result)return result;
    result=TestRejectedRecovery();if(result)return result;
    result=TestPhysicalCodec();if(result)return result;
    result=TestEpochInsideCodec();if(result)return result;
    result=TestOddDataAndReads();if(result)return result;
    result=TestEndToEnd();if(result)return result;
    return TestLocalInstall();
}
