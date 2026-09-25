#include "StorageSWD.h"
#include "storage_swd_test_port.h"
#include "storage_swd_fixture.h"

#define TEST_BUFFER ((uint8_t *)0xC0010000UL)
#define CHECK(condition) do { ++storage_swd_test_assertions; if(!(condition)){ \
    storage_swd_test_failure_line=__LINE__;return __LINE__; } } while(0)
volatile uint32_t storage_swd_test_assertions,storage_swd_test_failure_line;
static uint32_t ram_ready,nor_ready,ram_capacity,allocate_fail,allocations;
static uint32_t reads,read_failure,read_faults,last_read_address,last_read_length;
static uint32_t seq_reads,seq_change_at,seq_always_changes,sync_calls,publish_faults;

/* freestanding 시험의 구조체 초기화 경계를 제공한다. 제품은 통상 C runtime을 쓴다. */
void *memset(void *destination,int value,size_t length)
{uint8_t *p=destination;size_t i;for(i=0U;i<length;++i)p[i]=(uint8_t)value;return destination;}
/* 구조체 복사용 최소 libc다. 겹치지 않는 source/destination만 사용한다. */
void *memcpy(void *destination,const void *source,size_t length)
{uint8_t *d=destination;const uint8_t *s=source;size_t i;for(i=0U;i<length;++i)d[i]=s[i];return destination;}

/* ready 경계는 테스트가 직접 바꿔 Init/Process의 사전 검사를 확인한다. */
uint32_t StorageSWDTest_RamReady(void){return ram_ready;}
/* geometry와 달리 검증된 실제 용량으로 취급하는 값이다. */
uint32_t StorageSWDTest_RamCapacity(void){return ram_capacity;}
/* 전용 allocation 크기를 검사하고 Unicorn의 별도 SDRAM map 주소를 돌려준다. */
void *StorageSWDTest_Allocate(size_t bytes)
{
    ++allocations;
    if(bytes!=STORAGE_SWD_BUFFER_BYTES||!ram_ready)++read_faults;
    return allocate_fail?NULL:TEST_BUFFER;
}
/* NOR ready가 꺼지면 새 읽기를 하지 않아야 한다. */
uint32_t StorageSWDTest_NorReady(void){return nor_ready;}
/* 실제 UID 레지스터 대신 고정 세 word를 사용한다. */
uint32_t StorageSWDTest_UID(uint32_t index){return 0x10203040UL+index;}

/* NOR fixture byte는 전체 128MiB 주소 공간의 결정적 함수이며 Python zlib fixture와
 * 독립적으로 비교한다. address 변화가 bytes에 반영되어 offset 혼동을 검출한다. */
static uint8_t NorByte(uint32_t address)
{return (uint8_t)(((address*17U)+(address>>8U))^0xA5U);}

/* 최대4KiB 읽기와 NOR/SDRAM 목적지 범위를 실제 C의 callback 인수에서 검사한다.
 * ERROR 직전까지 destination을 안 바꾸며 성공일 때만 지정 범위를 채운다. */
uint32_t StorageSWDTest_Read(uint32_t address,void *destination,uint32_t length)
{
    uint8_t *out=destination;uint32_t at=(uint32_t)(uintptr_t)out,index;
    ++reads;last_read_address=address;last_read_length=length;
    if(!ram_ready||!nor_ready||!length||length>4096U||address>STORAGE_SWD_NOR_BYTES
       ||length>STORAGE_SWD_NOR_BYTES-address||at<(uint32_t)(uintptr_t)TEST_BUFFER
       ||at-(uint32_t)(uintptr_t)TEST_BUFFER>STORAGE_SWD_BUFFER_BYTES
       ||length>STORAGE_SWD_BUFFER_BYTES-(at-(uint32_t)(uintptr_t)TEST_BUFFER)
       ||g_storage_swd.state!=STORAGE_SWD_BUSY||g_storage_swd.response_seq!=0U){
        ++read_faults;return 1U;
    }
    if(read_failure!=0U&&reads==read_failure)return 1U;
    for(index=0U;index<length;++index)out[index]=NorByte(address+index);
    return 0U;
}
/* DMB 호출 자체의 실제 bus ordering은 모의하지 않는다. */
void StorageSWDTest_Barrier(void){ }
/* 응답 commit 직전 DSB 경계에서 seq가 아직0인지 검사한다. */
void StorageSWDTest_Sync(void)
{++sync_calls;if(g_storage_swd.response_seq!=0U)++publish_faults;}
/* 두 번째 seq 읽기에서 변경을 주입해 torn request가 실행되지 않는지 검사한다. */
uint32_t StorageSWDTest_RequestSeq(void)
{
    ++seq_reads;
    if((seq_change_at!=0U&&seq_reads==seq_change_at)||seq_always_changes!=0U)
        ++g_storage_swd.request_seq;
    return g_storage_swd.request_seq;
}

/* Host 계약과 같은 무효화→필드→seq 순서로 요청을 게시한다. */
static void Request(uint32_t sequence,uint32_t offset,uint32_t length)
{
    g_storage_swd.request_seq=0U;
    g_storage_swd.request_magic=STORAGE_SWD_REQUEST_MAGIC;
    g_storage_swd.request_arm=STORAGE_SWD_READ_ARM;
    g_storage_swd.request_offset=offset;g_storage_swd.request_length=length;
    g_storage_swd.request_seq=sequence;
}

/* 준비 전에는 allocator/read가 호출되지 않고 성공 Init은한 번만 할당하는지 확인한다. */
static uint32_t TestInit(void)
{
    StorageSWD_Mailbox snapshot;
    ram_capacity=0x04000000UL;
    CHECK(StorageSWD_Init()==STORAGE_SWD_NOT_READY&&allocations==0U&&reads==0U);
    CHECK(g_storage_swd.magic==STORAGE_SWD_MAGIC&&g_storage_swd.init_result==STORAGE_SWD_NOT_READY);
    ram_ready=1U;
    CHECK(StorageSWD_Init()==STORAGE_SWD_NOT_READY&&allocations==0U);
    nor_ready=1U;ram_capacity=0x800000U;
    CHECK(StorageSWD_Init()==STORAGE_SWD_NO_MEMORY&&allocations==0U);
    ram_capacity=0x04000000UL;allocate_fail=1U;
    CHECK(StorageSWD_Init()==STORAGE_SWD_NO_MEMORY&&allocations==1U);
    allocate_fail=0U;
    CHECK(StorageSWD_Init()==STORAGE_SWD_OK&&allocations==2U);
    CHECK(g_storage_swd.abi==1U&&g_storage_swd.mailbox_bytes==128U);
    CHECK(g_storage_swd.state==STORAGE_SWD_IDLE&&g_storage_swd.buffer_capacity==0x800000U);
    CHECK(g_storage_swd.buffer_address==0xC0010000UL&&g_storage_swd.nor_capacity==0x8000000U);
    CHECK(g_storage_swd.uid0==0x10203040UL&&g_storage_swd.uid1==0x10203041UL&&g_storage_swd.uid2==0x10203042UL);
    CHECK(g_storage_swd.jedec_id==0x00C2201BUL);
    TEST_BUFFER[0]=0x55U;
    CHECK(StorageSWD_Init()==STORAGE_SWD_OK&&allocations==2U&&TEST_BUFFER[0]==0x55U);
    CHECK(StorageSWD_GetDiagnostics(NULL)==0U);
    CHECK(StorageSWD_GetDiagnostics(&snapshot)==1U&&snapshot.buffer_address==0xC0010000UL);
    return 0U;
}

/* 잘못된 요청은 응답만 남기고 기존 SDRAM을 덮지 않는다. NOR 끝 overflow와 잘못된
 * arm,0길이,8MiB 초과를 각각 확인한다. double-seq 불일치는 consume하지 않는다. */
static uint32_t TestRejected(void)
{
    uint32_t before=reads;
    Request(1U,0U,0U);StorageSWD_Process();
    CHECK(g_storage_swd.response_seq==1U&&g_storage_swd.response_result==STORAGE_SWD_ARGUMENT);
    CHECK(TEST_BUFFER[0]==0x55U&&reads==before);
    Request(2U,0U,STORAGE_SWD_BUFFER_BYTES+1U);StorageSWD_Process();
    CHECK(g_storage_swd.response_seq==2U&&g_storage_swd.response_result==STORAGE_SWD_ARGUMENT);
    Request(3U,STORAGE_SWD_NOR_BYTES,1U);StorageSWD_Process();
    CHECK(g_storage_swd.response_result==STORAGE_SWD_ARGUMENT);
    Request(4U,UINT32_MAX,16U);StorageSWD_Process();
    CHECK(g_storage_swd.response_result==STORAGE_SWD_ARGUMENT);
    Request(5U,STORAGE_SWD_NOR_BYTES-1U,2U);StorageSWD_Process();
    CHECK(g_storage_swd.response_result==STORAGE_SWD_ARGUMENT);
    Request(6U,0U,4U);g_storage_swd.request_arm=0U;StorageSWD_Process();
    CHECK(g_storage_swd.response_result==STORAGE_SWD_ARGUMENT&&reads==before);
    Request(7U,0U,4U);g_storage_swd.request_magic=0U;StorageSWD_Process();
    CHECK(g_storage_swd.response_result==STORAGE_SWD_ARGUMENT&&TEST_BUFFER[0]==0x55U);
    Request(8U,0U,4U);g_storage_swd.request_seq=0U;StorageSWD_Process();
    CHECK(g_storage_swd.last_request_seq==7U&&reads==before);
    Request(8U,0U,4U);seq_reads=0U;seq_change_at=2U;StorageSWD_Process();
    CHECK(g_storage_swd.last_request_seq==7U&&reads==before);
    seq_change_at=0U;g_storage_swd.request_seq=0U;
    return 0U;
}

/* 임의 길이4097, 마지막1byte,0..8192 읽기로 범위/CRC/commit-last를 검증한다.
 * 완료 버퍼와 descriptor는 새 요청이 없으면 Process를 반복해도 바뀌지 않는다. */
static uint32_t TestReadAndBusy(void)
{
    uint32_t before=reads,i,crc,seq;StorageSWD_Mailbox snapshot;
    Request(10U,123U,4097U);StorageSWD_Process();
    CHECK(reads==before+1U&&last_read_address==123U&&last_read_length==4096U);
    CHECK(g_storage_swd.state==STORAGE_SWD_BUSY&&g_storage_swd.response_seq==0U&&g_storage_swd.response_completed==4096U);
    Request(11U,9000U,8U);StorageSWD_Process();
    CHECK(g_storage_swd.request_result==STORAGE_SWD_BUSY_RESULT&&g_storage_swd.rejected_seq==11U);
    CHECK(g_storage_swd.busy_rejections==1U&&g_storage_swd.response_seq==10U);
    CHECK(g_storage_swd.state==STORAGE_SWD_READY&&g_storage_swd.response_length==4097U);
    CHECK(last_read_address==4219U&&last_read_length==1U&&g_storage_swd.response_crc32==SWD_CRC_123_4097);
    for(i=0U;i<4097U;++i)CHECK(TEST_BUFFER[i]==NorByte(123U+i));
    crc=g_storage_swd.response_crc32;before=reads;
    for(i=0U;i<10U;++i)StorageSWD_Process();
    CHECK(reads==before&&g_storage_swd.response_seq==10U&&g_storage_swd.response_crc32==crc);
    CHECK(StorageSWD_Init()==STORAGE_SWD_OK&&allocations==2U&&g_storage_swd.response_seq==10U);
    Request(12U,0U,0U);StorageSWD_Process();
    CHECK(g_storage_swd.state==STORAGE_SWD_ERROR&&reads==before&&TEST_BUFFER[0]==NorByte(123U));
    Request(13U,STORAGE_SWD_NOR_BYTES-1U,1U);StorageSWD_Process();
    CHECK(g_storage_swd.response_seq==13U&&g_storage_swd.response_completed==1U);
    CHECK(TEST_BUFFER[0]==NorByte(STORAGE_SWD_NOR_BYTES-1U)&&g_storage_swd.response_crc32==SWD_CRC_LAST_1);
    before=reads;StorageSWD_Process();CHECK(reads==before);
    CHECK(StorageSWD_GetDiagnostics(&snapshot)==1U&&snapshot.response_seq==13U);
    snapshot.magic=0x11223344UL;seq=g_storage_swd.request_seq;seq_always_changes=1U;
    CHECK(StorageSWD_GetDiagnostics(&snapshot)==0U&&snapshot.magic==0x11223344UL);
    seq_always_changes=0U;g_storage_swd.request_seq=seq;
    return 0U;
}

/* 중간 NOR 오류는 ERROR를 게시하고 부분 byte 수만 남긴다. 준비 상태가 내려가면
 * BSP read를 호출하지 않으며 재시도는 반드시 새 request seq로만 시작한다. */
static uint32_t TestReadFailures(void)
{
    uint32_t before;
    Request(20U,500U,8192U);read_failure=reads+2U;StorageSWD_Process();StorageSWD_Process();
    CHECK(g_storage_swd.state==STORAGE_SWD_ERROR&&g_storage_swd.response_result==STORAGE_SWD_READ_ERROR);
    CHECK(g_storage_swd.response_seq==20U&&g_storage_swd.response_completed==4096U&&g_storage_swd.response_crc32==0U);
    before=reads;StorageSWD_Process();CHECK(reads==before);
    read_failure=0U;Request(21U,0U,8192U);StorageSWD_Process();before=reads;nor_ready=0U;StorageSWD_Process();
    CHECK(reads==before&&g_storage_swd.response_seq==21U&&g_storage_swd.response_result==STORAGE_SWD_NOT_READY);
    Request(22U,0U,8192U);StorageSWD_Process();CHECK(reads==before&&g_storage_swd.response_seq==22U);
    nor_ready=1U;ram_ready=0U;Request(23U,0U,1U);StorageSWD_Process();
    CHECK(reads==before&&g_storage_swd.response_result==STORAGE_SWD_NOT_READY);
    ram_ready=1U;Request(24U,0U,8192U);StorageSWD_Process();StorageSWD_Process();
    CHECK(g_storage_swd.state==STORAGE_SWD_READY&&g_storage_swd.response_crc32==SWD_CRC_0_8192);
    CHECK(!read_faults&&!publish_faults&&sync_calls!=0U);
    return 0U;
}

/* 실제 C 단위 검사 진입점. 전체8MiB 검사와 분리하여 O0/Os의 짧은 경계 검증을 한다. */
uint32_t storage_swd_test_main(void)
{
    uint32_t result;
    result=TestInit();if(result)return result;
    result=TestRejected();if(result)return result;
    result=TestReadAndBusy();if(result)return result;
    return TestReadFailures();
}

/* 최대 길이를 실제2048번 Process로 읽는다. image/CRC는 runner의 zlib과 비교하며
 * 실행당4KiB 계약과 buffer 끝 범위를 동시에 검증한다. */
uint32_t storage_swd_test_full(void)
{
    uint32_t i,before=reads;
    Request(100U,STORAGE_SWD_NOR_BYTES-STORAGE_SWD_BUFFER_BYTES,STORAGE_SWD_BUFFER_BYTES);
    for(i=0U;i<2048U;++i)StorageSWD_Process();
    CHECK(reads==before+2048U&&g_storage_swd.state==STORAGE_SWD_READY);
    CHECK(g_storage_swd.response_seq==100U&&g_storage_swd.response_completed==STORAGE_SWD_BUFFER_BYTES);
    CHECK(g_storage_swd.response_crc32==SWD_CRC_FULL);
    CHECK(TEST_BUFFER[0]==NorByte(STORAGE_SWD_NOR_BYTES-STORAGE_SWD_BUFFER_BYTES));
    CHECK(TEST_BUFFER[STORAGE_SWD_BUFFER_BYTES-1U]==NorByte(STORAGE_SWD_NOR_BYTES-1U));
    CHECK(!read_faults&&!publish_faults);
    return 0U;
}
