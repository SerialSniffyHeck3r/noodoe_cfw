#if !NOODOE_PRODUCT
#if NOODOE_PRODUCT
#include "Resources.h"
#include "ResourceStore.h"
static ResourceRequirement required_resources;
static uint32_t resource_checked_transaction,resource_requirement_valid;
#endif
#include "RuntimeUpdate.h"
#include "Update_Metadata.h"
#include "Recovery_Core.h"
#include <stddef.h>
#include <string.h>

/* 모델 시험에서도 실제 adapter C를 사용하며 보드 접근 함수 경계만 치환한다. */
#ifdef RUNTIME_UPDATE_TEST_PORT
#include "runtime_update_test_port.h"
#else
#include "BSP_RAM.h"
#include "BSP_NOR.h"
#include "NoodoeBluetooth.h"
#include "stm32f4xx.h"
#define RU_CONTEXT_OK() (__get_IPSR()==0U && (__get_CONTROL()&1U)==0U \
                        && __get_PRIMASK()==0U && __get_BASEPRI()==0U && __get_FAULTMASK()==0U)
#define RU_RESET() NVIC_SystemReset()
#endif

#if NOODOE_BOOTSTRAP
/* Bootstrap may install a Product APP only after its immutable resources and
 * independent stock recovery source have been proven by the maintenance owner.
 * Missing integration fails closed; STOCK still requires its pinned image hash. */
__attribute__((weak)) uint32_t BootstrapUpdate_ValidateTarget(uint32_t target,
    const uint8_t requirement44[44],uint32_t version,const uint8_t sha[32])
{(void)target;(void)requirement44;(void)version;(void)sha;return RUNTIME_UPDATE_LOCKED;}
#endif

#define RU_ALIGNMENT 32U
#define RU_SCRATCH_BYTES UPDATE_METADATA_SECTOR_SIZE
#define RU_BT_QUIESCE_TIMEOUT_MS 1000U
#define RU_SERVICE_BYTES ((sizeof(UpdateService)+RU_ALIGNMENT-1U)&~(size_t)(RU_ALIGNMENT-1U))
#define RU_ARENA_BYTES (RU_SERVICE_BYTES+RU_SCRATCH_BYTES)

volatile RuntimeUpdate_Diagnostics g_runtime_update;
static UpdateService *service;
static void *metadata_scratch;
static uint32_t transaction,process_time;
static volatile uint32_t published;
__attribute__((weak)) uint32_t RuntimeUpdate_FlashReady(void){return 1;}

/* main SRAM의 단일 aligned word로 publish한다. 큰 SDRAM 객체를 모두 초기화한 뒤의
 * release store와 소비자의 acquire load가 부분 초기화 상태의 큐 접근을 막는다. */
static uint32_t Ready(void)
{return __atomic_load_n(&published,__ATOMIC_ACQUIRE);}

/* RAM/NOR의 이전 오류 코드를 무조건 성공으로 덮지 않는다. 현재 ready, 실제 capacity,
 * 대상 JEDEC를 매 operation 전에 다시 확인하고 실패하면 보드 함수를 호출하지 않는다. */
static uint32_t HardwareReady(void)
{return g_bsp_ram.ready && g_bsp_ram.result==0U && g_bsp_ram.capacity_bytes<=BSP_RAM_GEOMETRY_BYTES
    && g_bsp_ram.capacity_bytes>=BSP_RAM_TEST_BYTES+RU_ARENA_BYTES
    && g_bsp_nor.ready && g_bsp_nor.capacity_bytes==BSP_NOR_CAPACITY_BYTES
    && g_bsp_nor.jedec_id==BSP_NOR_EXPECTED_JEDEC_ID;}

/* Context 포인터를 adapter의 객체와 대조하고 Thread/IRQ/하드웨어 상태를 확인한다.
 * 이 검사는 태스크 소유권 자체를 식별하지 않으므로 StorageTask 단독 호출 계약은 필요하다. */
static uint32_t ContextReady(void *context)
{return Ready() && context==service && RU_CONTEXT_OK() && HardwareReady();}

/* 결과 진단은 StorageTask가 쓴다. 외부 snapshot을 원자적 상태 머신으로 해석하지 않는다. */
static uint32_t Result(uint32_t result)
{g_runtime_update.last_platform_result=result;return result;}

/* APP staging만 허용하며 address+length overflow를 차감 비교로 피한다. BL staging,
 * 파일시스템/NVM, NOR 끝을 넘는 구간은 읽기 callback에서도 받아들이지 않는다. */
static uint32_t StageRange(uint32_t address,uint32_t length)
{return address>=UPDATE_STAGE_BASE && address<UPDATE_STAGE_BASE+UPDATE_APP_BYTES
    && length!=0U && length<=UPDATE_STAGE_BASE+UPDATE_APP_BYTES-address;}

/* 연결과 명시 authorization뿐 아니라 StorageTask가 수락한 link epoch를 재검사한다.
 * 한 page/erase 도중 연결이 바뀌고 재승인되어도 다음 operation은 예전 DATA를 계속하지
 * 않는다. 이미 시작한 물리 operation은 취소할 수 없고 transaction도 별도로 대조한다. */
static uint32_t Authorized(void)
{return (service->local_install==1&&service->state==UPDATE_VERIFIED)||
    (__atomic_load_n(&service->connected,__ATOMIC_ACQUIRE)
    && __atomic_load_n(&service->authorization,__ATOMIC_ACQUIRE)
    && __atomic_load_n(&service->link_generation,__ATOMIC_ACQUIRE)==service->generation);}

/* 순정 BL의 SPI16 DMA는 wire 두 byte를 little-endian halfword로 메모리에 받는다.
 * 현재 BSP_NOR는 wire 순서를 그대로 제공하므로 APP staging 전용 adapter가 교환한다.
 * first/length는 짝수 부분만 이 helper에 넘기며 개인 FS/NVM/raw backup에는 적용하지 않는다. */
static void SwapPairs(uint8_t *bytes,uint32_t length)
{
    uint32_t index;
    for(index=0U;index<length;index+=2U){
        uint8_t first=bytes[index];bytes[index]=bytes[index+1U];bytes[index+1U]=first;
    }
}

/* ReadStage/최종 SHA 검증은 canonical APP byte 순서로 반환한다. 논리 주소 a의 byte는
 * physical a^1에 있다. 홀수 선두/짝수 꼬리1byte는 그 물리 주소만 읽고, 짝수 중간은
 * 한번에 읽은 뒤 pair-swap한다. 임의 offset/길이와 unaligned destination을 지원한다.
 * 이 읽기는 쓰기 승인 없이 가능하고 BSP raw API/원본 백업 표현을 변경하지 않는다. */
static uint32_t Read(void *context,uint32_t address,void *destination,uint32_t length)
{
    uint8_t *out=destination;uint32_t result=0U,even;
    if(!ContextReady(context))return Result(RUNTIME_UPDATE_NOT_READY);
    if(!destination||!StageRange(address,length))return Result(RUNTIME_UPDATE_ARGUMENT);
    if(address&1U){
        result=(uint32_t)BSP_NOR_Read(address^1U,out,1U);
        if(result)return Result(result);
        ++address;++out;--length;
    }
    even=length&~1U;
    if(even){
        result=(uint32_t)BSP_NOR_Read(address,out,even);
        if(result)return Result(result);
        SwapPairs(out,even);address+=even;out+=even;length-=even;
    }
    if(length)result=(uint32_t)BSP_NOR_Read(address^1U,out,1U);
    return Result(result);
}

/* BEGIN이 상위에서 인증한 백업 승인과 nonzero transaction을 통과한 뒤에만 호출된다.
 * BSP enable은 능력 플래그만 열며 이 함수 자체로 erase/program은 발생하지 않는다.
 * metadata pending 검사는 BSP와 UpdateService가 각각 수행한다. */
static uint32_t Enable(void *context,uint32_t tx)
{
    uint32_t result;
    if(!ContextReady(context))return Result(RUNTIME_UPDATE_NOT_READY);
    if(!Authorized()||!tx)return Result(RUNTIME_UPDATE_LOCKED);
    if(transaction && transaction!=tx)return Result(RUNTIME_UPDATE_LOCKED);
    result=(uint32_t)BSP_NOR_OTAEnable(tx);
    if(!result){transaction=tx;g_runtime_update.enabled_transaction=tx;}
    return Result(result);
}

/* codec가 나눈 각 물리 program 직전에 epoch/권한을 재검사한다. 이전 single-byte
 * program의 BSP 대기 중 재연결해도 다음 조각은 쓰지 않는다. 이미 BSP에 전달한 하나의
 * 물리 operation은 취소할 수 없다. BL/FS 경계와256-byte page도 다시 확인한다. */
static uint32_t ProgramPhysical(void *context,uint32_t address,const void *source,uint32_t length)
{
    if(!ContextReady(context))return Result(RUNTIME_UPDATE_NOT_READY);
    if(!Authorized()||!transaction||transaction!=service->transaction)return Result(RUNTIME_UPDATE_LOCKED);
    if(!source||length>256U||!StageRange(address,length)||length>256U-(address&255U))return Result(RUNTIME_UPDATE_ARGUMENT);
    return Result((uint32_t)BSP_NOR_OTAProgram(address,source,length));
}

/* canonical APP를 순정 BL이 해석할 physical pair-swap 형식으로 기록한다. 논리256-byte
 * page 경계도 짝수라 xor1 mapping이 다른 page/sector로 넘어가지 않는다. 최대3개 조각:
 * 홀수 선두1byte, 짝수 중간(전용 stack256B에서교환), 짝수 꼬리1byte다. source를 고치거나
 * 이웃 byte를 읽어 다시 쓰는 RMW를 하지 않으므로 홀수로 나뉜 연속 DATA에도 안전하다.
 * 전체 정당성은 상위의 erase-first와 최종 canonical SHA/CRC 검증까지 요구한다. */
static uint32_t Program(void *context,uint32_t address,const void *source,uint32_t length)
{
    const uint8_t *in=source;uint8_t wire[256];uint32_t result,even,index;
    if(!ContextReady(context))return Result(RUNTIME_UPDATE_NOT_READY);
    if(!Authorized()||!transaction||transaction!=service->transaction)return Result(RUNTIME_UPDATE_LOCKED);
    if(!source||length>256U||!StageRange(address,length)||length>256U-(address&255U))return Result(RUNTIME_UPDATE_ARGUMENT);
    if(address&1U){
        result=ProgramPhysical(context,address^1U,in,1U);
        if(result)return result;
        ++address;++in;--length;
    }
    even=length&~1U;
    if(even){
        for(index=0U;index<even;index+=2U){wire[index]=in[index+1U];wire[index+1U]=in[index];}
        result=ProgramPhysical(context,address,wire,even);
        if(result)return result;
        address+=even;in+=even;length-=even;
    }
    if(length)return ProgramPhysical(context,address^1U,in,1U);
    return Result(0U);
}

/* 지우기는4KiB 정렬 sector 한 개이며 BL/FS 경계를 넘기지 않는다. 실제 write enable,
 * WIP/WEL/timeout/readback 검증은 BSP가 담당하고 오류를 성공으로 정규화하지 않는다. */
static uint32_t Erase(void *context,uint32_t address)
{
    if(!ContextReady(context))return Result(RUNTIME_UPDATE_NOT_READY);
    if(!Authorized()||!transaction||transaction!=service->transaction)return Result(RUNTIME_UPDATE_LOCKED);
    if((address&4095U)||!StageRange(address,4096U))return Result(RUNTIME_UPDATE_ARGUMENT);
    return Result((uint32_t)BSP_NOR_OTAErase4K(address));
}

/* 오류/완료/해제 때 volatile OTA capability만 닫는다. 하드웨어 ready가 이미 떨어져도
 * 이 잠금은 닫혀야 하므로 HardwareReady 검사를 하지 않는다. 어떤 영역도 지우지 않는다. */
static void Disable(void *context)
{
    if(!Ready()||context!=service)return;
    BSP_NOR_OTADisable();transaction=0U;g_runtime_update.enabled_transaction=0U;
}

/* 원본5워드를 읽으며 FFFFFFFF를0으로 바꾸지 않는다. 성공 후 남은 slot/length와 CRC0은
 * 정상이다. 읽기 자체에는 authorization이 필요하지 않다. */
static uint32_t MetadataRead(void *context,uint32_t words[5])
{
    if(!ContextReady(context))return Result(RUNTIME_UPDATE_NOT_READY);
    if(!words)return Result(RUNTIME_UPDATE_ARGUMENT);
    return Result((uint32_t)UpdateMetadata_Read(words));
}

/* 최종 COMT 명령을 내부 UMC2 token으로 바꾸는 유일한 경계다. 서비스의 전체448KiB
 * 검증 완료와 요청 version/ISO CRC/SHA 일치를 재검사하고 전용16KiB scratch만 넘긴다.
 * 이 CRC는 순정 APP 전송 CRC32/ISO이며 별도 STM32 word-CRC 함수로 치환하지 않는다.
 * IRQ를 막는 critical section은 사용하지 않는다. 먼저 BT 소유 태스크에 bounded pause를
 * 요청하고 성공 전에는 erase하지 않는다. pause 대기 중 epoch 변경도 다시 검사한다.
 * writer가 sector2 배타성과 CRC-last를 담당하며 모든 반환 경로에서 BT resume를 요청한다.
 * writer 실패는 설치 실패이며, writer 성공 뒤 radio resume 실패는 별도 진단에
 * 보존한다. 통신 실패를 이유로 이미 검증한 metadata를 다시 erase하지 않는다. */
static uint32_t MetadataCommitOperation(void *context,uint32_t version,uint32_t crc,uint32_t arm)
{
    uint32_t token=0U,writer_result;
    int quiet,resumed;
    if(!ContextReady(context))return Result(RUNTIME_UPDATE_NOT_READY);
    if(arm!=UPDATE_COMMIT_ARM||!Authorized()||service->state!=UPDATE_VERIFIED
       ||service->received!=UPDATE_APP_BYTES||service->verified!=UPDATE_APP_BYTES
       ||!service->transaction||version!=service->version||crc!=service->expected_crc
       ||!crc||crc==UINT32_MAX||version==UINT32_MAX
       ||memcmp(service->actual_sha,service->expected_sha,32U)!=0)
        return Result(RUNTIME_UPDATE_LOCKED);
    if(service->target==UPDATE_TARGET_STOCK&&
       (!RecoveryStock_Matches(version,service->actual_sha)||
        !RecoveryTarget_Verify((const uint8_t*)0x08000000U)))return Result(RUNTIME_UPDATE_LOCKED);
#if NOODOE_PRODUCT
    if(service->target==UPDATE_TARGET_STOCK){
        if(!RecoveryStock_Matches(version,service->actual_sha))return Result(RUNTIME_UPDATE_LOCKED);
    }else if(service->target!=UPDATE_TARGET_CFW||!resource_requirement_valid||resource_checked_transaction!=service->transaction||
        (required_resources.required&&!ResourceStore_Compatible(required_resources.sha256)))return Result(RUNTIME_UPDATE_LOCKED);
#endif
#if NOODOE_BOOTSTRAP
    {
        g_runtime_update.commit_phase=2;
        uint8_t requirement[44];const uint8_t *view=NULL;
        if(service->target==UPDATE_TARGET_STOCK){
            if(!RecoveryStock_Matches(version,service->actual_sha))return Result(RUNTIME_UPDATE_LOCKED);
        }else if(service->target==UPDATE_TARGET_CFW){
            if(Read(context,UPDATE_STAGE_BASE+0x10200U,requirement,sizeof(requirement)))return Result(RUNTIME_UPDATE_LOCKED);
            view=requirement;
        }else return Result(RUNTIME_UPDATE_LOCKED);
        g_runtime_update.commit_detail=BootstrapUpdate_ValidateTarget(service->target,view,version,service->actual_sha);
        if(g_runtime_update.commit_detail)return Result(RUNTIME_UPDATE_LOCKED);
    }
#endif
    /* Target verification (<=1500ms) and HCI drain (<=1000ms) are distinct
     * completed steps. Check real owner progress between them as well as
     * before FLASH; never turn their combined duration into a late heartbeat. */
    g_runtime_update.commit_phase=5;
    if(!RuntimeUpdate_FlashReady())return Result(RUNTIME_UPDATE_LOCKED);
    Disable(context);
    g_runtime_update.commit_phase=3;
    quiet=Bluetooth_QuiesceTransport(RU_BT_QUIESCE_TIMEOUT_MS,&token);
    if(quiet!=0)return Result((uint32_t)quiet);
    g_runtime_update.commit_phase=4;
    if(!token||!ContextReady(context)||!Authorized()){
        resumed=Bluetooth_ResumeTransport(token);
        return Result(resumed!=0?(uint32_t)resumed:RUNTIME_UPDATE_LOCKED);
    }
    g_runtime_update.commit_phase=5;
    if(!RuntimeUpdate_FlashReady()){
        g_runtime_update.resume_result=(uint32_t)Bluetooth_ResumeTransport(token);
        return Result(RUNTIME_UPDATE_LOCKED);
    }
    ++g_runtime_update.commit_calls;
    g_runtime_update.commit_phase=6;g_runtime_update.commit_destructive=1;
    writer_result=(uint32_t)UpdateMetadata_Commit(version,crc,UPDATE_METADATA_ARM_TOKEN,
                                                metadata_scratch,RU_SCRATCH_BYTES);
    UpdateMetadataDiagnostics diagnostic={.destructive_started=1};
    UpdateMetadata_GetDiagnostics(&diagnostic);
    g_runtime_update.commit_destructive=diagnostic.destructive_started;
    g_runtime_update.writer_result=writer_result;
    if(!writer_result)g_runtime_update.commit_phase=7;
    resumed=Bluetooth_ResumeTransport(token);g_runtime_update.resume_result=(uint32_t)resumed;
    /* Full physical metadata readback already completed inside the writer.
     * Losing HCI afterwards cannot turn a durable commit into an ambiguous
     * flash write. Retain the radio error separately; permit local restart. */
    return Result(writer_result);
}
/* Keep this diagnostic immutable across subsequent STATUS/READ requests. */
static uint32_t MetadataCommit(void *context,uint32_t version,uint32_t crc,uint32_t arm)
{
    g_runtime_update.commit_phase=1;g_runtime_update.commit_result=0;
    g_runtime_update.commit_detail=0;g_runtime_update.commit_destructive=0;
    g_runtime_update.writer_result=g_runtime_update.resume_result=0;
    uint32_t result=MetadataCommitOperation(context,version,crc,arm);
    g_runtime_update.commit_result=result;
    if(!result)g_runtime_update.commit_phase=8;
    return result;
}
static uint32_t CommitUntouched(void *context)
{return context==service&&g_runtime_update.commit_phase&&!g_runtime_update.commit_destructive;}

/* UpdateService가 RESET 요청 ACK 또는 물리 승인 설치의 완료 뒤1500ms를 기다린 경로에서만
 * reset을 요청한다. 해당 transaction/version/CRC의 pending 메타를 다시 읽고 틀리면
 * 재부팅하지 않는다. reset 반환/실패를 설치 성공으로 표시하지 않는다. */
static void Reset(void *context)
{
    uint32_t words[5];
    if(!ContextReady(context)||service->state!=UPDATE_COMMITTED||!service->transaction
       ||(service->local_install!=3&&(!__atomic_load_n(&service->sent_valid,__ATOMIC_ACQUIRE)
       ||__atomic_load_n(&service->sent_sequence,__ATOMIC_ACQUIRE)!=service->reset_sequence
       ||process_time-__atomic_load_n(&service->sent_ms,__ATOMIC_ACQUIRE)<1500U)))return;
    if(MetadataRead(context,words)||!RECOVERY_VERSION_SUPPORTED(words[0])
       ||words[1]!=service->version||words[2]!=UPDATE_METADATA_APP_BLOCK
       ||words[3]!=UPDATE_APP_BYTES||words[4]!=service->expected_crc)return;
    Disable(context);++g_runtime_update.reset_calls;RU_RESET();
}

/* 한 monotonic allocation을32-byte 정렬 service와16KiB scratch로 분할한다. 두 영역은
 * 겹치지 않으며 다른 SDRAM client에게 공개하지 않는다. BSP bring-up scratch64KiB도
 * 제외한다. Init 성공 뒤 allocation/초기화 반복으로 기존 큐를 덮어쓰지 않는다. */
uint32_t RuntimeUpdate_Init(void)
{
    void *arena;uintptr_t address;uint32_t capacity;
    UpdatePlatform platform;
    if(Ready())return RUNTIME_UPDATE_OK;
    if(!RU_CONTEXT_OK())return g_runtime_update.result=RUNTIME_UPDATE_CONTEXT;
    if(!HardwareReady())return g_runtime_update.result=RUNTIME_UPDATE_NOT_READY;
    capacity=g_bsp_ram.capacity_bytes;
    arena=BSP_RAM_Allocate(RU_ARENA_BYTES);
    if(!arena)return g_runtime_update.result=RUNTIME_UPDATE_NO_MEMORY;
    address=(uintptr_t)arena;
    if(address<BSP_RAM_BASE+BSP_RAM_TEST_BYTES||(address&(RU_ALIGNMENT-1U))
       ||address-BSP_RAM_BASE>capacity||RU_ARENA_BYTES>capacity-(address-BSP_RAM_BASE))
        return g_runtime_update.result=RUNTIME_UPDATE_BAD_MEMORY;
    service=(UpdateService *)arena;
    metadata_scratch=(void *)(address+RU_SERVICE_BYTES);
    memset(&platform,0,sizeof(platform));platform.context=service;
    platform.read=Read;platform.enable=Enable;platform.erase4k=Erase;platform.program=Program;
    platform.disable=Disable;platform.metadata_read=MetadataRead;
    platform.metadata_commit=MetadataCommit;platform.reset=Reset;
    platform.commit_untouched=CommitUntouched;
    UpdateService_Init(service,&platform);
    BSP_NOR_OTADisable();transaction=0U;
    g_runtime_update.service_address=(uint32_t)address;
    g_runtime_update.service_bytes=sizeof(*service);
    g_runtime_update.scratch_address=(uint32_t)(uintptr_t)metadata_scratch;
    g_runtime_update.scratch_bytes=RU_SCRATCH_BYTES;
    g_runtime_update.result=RUNTIME_UPDATE_OK;g_runtime_update.ready=1U;
    __atomic_store_n(&published,1U,__ATOMIC_RELEASE);
    return RUNTIME_UPDATE_OK;
}

/* SRAM publication word만 acquire로 읽고 검증된 service 포인터를 반환한다. failed Init
 * 직후에는 아직 객체가 공개되지 않아 IOTask가 부분 초기화된 큐를 만질 수 없다. */
UpdateService *RuntimeUpdate_GetService(void)
{return Ready()?service:NULL;}

/* StorageTask는 초기화 완료 뒤 호출한다. 정상 metadata writer에 필요한 privileged
 * Thread/IRQ 상태를 확인하고 IRQ를 가리는 포괄 critical section을 추가하지 않는다. */
void RuntimeUpdate_Process(uint32_t now_ms)
{
    if(!Ready())return;
    if(!RU_CONTEXT_OK()){g_runtime_update.result=RUNTIME_UPDATE_CONTEXT;return;}
#if NOODOE_PRODUCT
    if(service->state==UPDATE_RECEIVING||service->state==UPDATE_IDLE){resource_checked_transaction=0;resource_requirement_valid=0;}
    if(service->state==UPDATE_VERIFIED&&service->target==UPDATE_TARGET_CFW){
        if(resource_checked_transaction!=service->transaction){
            resource_requirement_valid=!Read(service,UPDATE_STAGE_BASE+0x200,&required_resources,sizeof(required_resources))&&
                required_resources.magic==0x51534352U&&required_resources.version==1&&required_resources.required<=1;
            resource_checked_transaction=service->transaction;
        }
        if(resource_requirement_valid&&required_resources.required)(void)ResourceStore_Compatible(required_resources.sha256);
    }
#endif
    process_time=now_ms;++g_runtime_update.polls;
    UpdateService_Process(service,now_ms);
    /* StorageTask 상태와 IOTask link/권한의 최근 관측값을 게시한다. 같은 순간의 원자적
     * transaction이라 주장하지 않으며 critical section으로 metadata 경로를 가리지 않는다. */
    g_runtime_update.state=service->state;g_runtime_update.service_result=service->result;
    g_runtime_update.connected=__atomic_load_n(&service->connected,__ATOMIC_ACQUIRE);
    g_runtime_update.authorized=__atomic_load_n(&service->authorization,__ATOMIC_ACQUIRE);
    g_runtime_update.received_bytes=service->received;g_runtime_update.verified_bytes=service->verified;
}

#endif /* Product uses the gate-only update adapter */
