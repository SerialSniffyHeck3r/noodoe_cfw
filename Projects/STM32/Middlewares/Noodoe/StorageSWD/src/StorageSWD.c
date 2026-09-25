#include "Noodoe_Crc32.h"
#include "StorageSWD.h"

/* 제품 빌드는 BSP의 읽기/할당 API만 사용한다. 시험은 이 경계만 대역으로 바꾸며
 * mailbox 게시 순서, 범위 검사, CRC와 상태 전이는 같은 실제 C를 실행한다. */
#ifdef STORAGE_SWD_TEST_PORT
#include "storage_swd_test_port.h"
#else
#include "BSP_RAM.h"
#include "BSP_NOR.h"
#include "stm32f4xx.h"
#define SWD_PORT_RAM_READY() (g_bsp_ram.ready != 0U && g_bsp_ram.result == 0U)
#define SWD_PORT_RAM_CAPACITY() (g_bsp_ram.capacity_bytes)
#define SWD_PORT_RAM_ALLOCATE(bytes) BSP_RAM_Allocate(bytes)
#define SWD_PORT_NOR_READY() (g_bsp_nor.ready != 0U && g_bsp_nor.capacity_bytes == STORAGE_SWD_NOR_BYTES)
#define SWD_PORT_JEDEC() (g_bsp_nor.jedec_id)
#define SWD_PORT_UID(index) (*(volatile const uint32_t *)(UID_BASE + 4U * (index)))
#define SWD_PORT_READ(address,destination,length) ((uint32_t)BSP_NOR_Read(address,destination,length))
#define SWD_PORT_BARRIER() __DMB()
#define SWD_PORT_SYNC() __DSB()
#define SWD_PORT_REQUEST_SEQ() (g_storage_swd.request_seq)
#endif

#define SWD_RAM_BASE 0xC0000000UL
#define SWD_RAM_MAX_BYTES 0x04000000UL
#define SWD_RAM_RESERVED_BYTES 65536U

volatile StorageSWD_Mailbox g_storage_swd;
static uint8_t *read_buffer;
static uint32_t initialized, running, consumed_seq;
static uint32_t active_seq, active_offset, active_length, completed, crc_state;

typedef struct { uint32_t magic,arm,offset,length,seq; } RequestSnapshot;

/* CRC32/ISO-HDLC를 software로 이어 계산한다. byte 순서 그대로 처리하며 반환값은
 * final XOR 이전 내부 상태다. empty CRC는 사용하지 않고 length>=1만 수락한다.
 * STM32 word CRC 주변장치를 점유하지 않으므로 다른 CRC 사용자와 겹치지 않는다. */
static uint32_t CrcFeed(uint32_t state,const uint8_t *data,uint32_t length)
{return Noodoe_Crc32Feed(state,data,length);}

/* sequence를 양쪽에서 읽어 SWD가 request 필드를 쓰는 중인 torn snapshot을 버린다.
 * Host는 모든 변경 전에 seq=0, 완료 후 새로운 seq를 마지막에 써야 한다.
 * seq를 그대로 둔 채 필드만 변경하는 host는 이 방식으로 검출할 수 없다. */
static uint32_t CaptureRequest(RequestSnapshot *request)
{
    uint32_t first=SWD_PORT_REQUEST_SEQ();
    if(first==0U||first==consumed_seq)return 0U;
    SWD_PORT_BARRIER();
    request->magic=g_storage_swd.request_magic;
    request->arm=g_storage_swd.request_arm;
    request->offset=g_storage_swd.request_offset;
    request->length=g_storage_swd.request_length;
    SWD_PORT_BARRIER();
    request->seq=SWD_PORT_REQUEST_SEQ();
    return first==request->seq&&request->seq!=0U;
}

/* READY/ERROR 응답은 이전 seq를 먼저 무효화하고 모든 descriptor를 기록한 뒤 마지막
 * seq를 게시한다. ERROR CRC는0이며 completed만 부분 진척을 뜻한다. buffer address
 * 자체는 완료 데이터의 유효성을 의미하지 않으므로 host는 result/state를 검사한다. */
static void Publish(uint32_t sequence,uint32_t offset,uint32_t length,
                    uint32_t result,uint32_t bytes,uint32_t crc)
{
    g_storage_swd.response_seq=0U;
    SWD_PORT_BARRIER();
    g_storage_swd.response_offset=offset;
    g_storage_swd.response_length=length;
    g_storage_swd.response_result=result;
    g_storage_swd.response_address=(uint32_t)(uintptr_t)read_buffer;
    g_storage_swd.response_crc32=crc;
    g_storage_swd.response_completed=bytes;
    g_storage_swd.state=result==STORAGE_SWD_OK?STORAGE_SWD_READY:STORAGE_SWD_ERROR;
    /* SDRAM writes를 끝내고 내부 SRAM descriptor보다 먼저 외부에서 관측되게 한다. */
    SWD_PORT_SYNC();
    SWD_PORT_BARRIER();
    g_storage_swd.response_seq=sequence;
}

/* 검증된 RAM/NOR가 준비되기 전에는 allocator를 호출하지 않는다. successful Init은
 * idempotent라 이미 게시한 버퍼와 host 요청을 보존한다. 실패하면 진단 ABI만 게시하고
 * 정상화 뒤 동일 StorageTask의 명시적 Init 재시도를 기다린다. */
uint32_t StorageSWD_Init(void)
{
    StorageSWD_Mailbox initial={0};
    uint32_t capacity,address,result=STORAGE_SWD_OK;
    if(initialized!=0U)return STORAGE_SWD_OK;
    initial.abi=STORAGE_SWD_ABI;
    initial.mailbox_bytes=sizeof(StorageSWD_Mailbox);
    initial.nor_capacity=STORAGE_SWD_NOR_BYTES;
    initial.uid0=SWD_PORT_UID(0U);initial.uid1=SWD_PORT_UID(1U);initial.uid2=SWD_PORT_UID(2U);
    initial.jedec_id=SWD_PORT_JEDEC();
    capacity=SWD_PORT_RAM_CAPACITY();
    if(!SWD_PORT_RAM_READY()||!SWD_PORT_NOR_READY())result=STORAGE_SWD_NOT_READY;
    else if(capacity>SWD_RAM_MAX_BYTES||capacity<SWD_RAM_RESERVED_BYTES+STORAGE_SWD_BUFFER_BYTES)result=STORAGE_SWD_NO_MEMORY;
    else {
        read_buffer=SWD_PORT_RAM_ALLOCATE(STORAGE_SWD_BUFFER_BYTES);
        if(read_buffer==NULL)result=STORAGE_SWD_NO_MEMORY;
        else {
            address=(uint32_t)(uintptr_t)read_buffer;
            /* geometry와 실제 capacity를 혼동하지 않는다. 32-byte 정렬과 reserved
             * scratch 제외, 덧셈 overflow 없는 차감 범위 검사 후만 buffer를 공개한다. */
            if(address<SWD_RAM_BASE+SWD_RAM_RESERVED_BYTES||(address&31U)!=0U
               ||address-SWD_RAM_BASE>capacity
               ||STORAGE_SWD_BUFFER_BYTES>capacity-(address-SWD_RAM_BASE)){
                read_buffer=NULL;result=STORAGE_SWD_BUFFER_ERROR;
            }
        }
    }
    initial.init_result=result;
    initial.state=result==STORAGE_SWD_OK?STORAGE_SWD_IDLE:STORAGE_SWD_ERROR;
    initial.request_result=result;
    if(result==STORAGE_SWD_OK){
        initial.buffer_address=(uint32_t)(uintptr_t)read_buffer;
        initial.buffer_capacity=STORAGE_SWD_BUFFER_BYTES;
        initialized=1U;
    }
    /* magic=0인 구조체를 먼저 쓰고 version/UID/상태까지 게시한 후 magic을 연다. */
    g_storage_swd=initial;
    SWD_PORT_BARRIER();
    g_storage_swd.magic=STORAGE_SWD_MAGIC;
    return result;
}

/* 안정된 새 seq를 한 번 소비한다. BUSY 중 들어온 요청은 원 작업을 덮거나 예약하지
 * 않는다. invalid request는 버퍼를 건드리지 않고 ERROR 응답만 게시한다. */
static void AcceptOrReject(const RequestSnapshot *request)
{
    uint32_t result=STORAGE_SWD_OK;
    consumed_seq=request->seq;
    g_storage_swd.last_request_seq=request->seq;
    if(running!=0U){
        ++g_storage_swd.busy_rejections;
        result=STORAGE_SWD_BUSY_RESULT;
    } else if(initialized==0U||!SWD_PORT_RAM_READY()||!SWD_PORT_NOR_READY()){
        result=STORAGE_SWD_NOT_READY;
    } else if(request->magic!=STORAGE_SWD_REQUEST_MAGIC||request->arm!=STORAGE_SWD_READ_ARM
              ||request->length==0U||request->length>STORAGE_SWD_BUFFER_BYTES
              ||request->offset>STORAGE_SWD_NOR_BYTES
              ||request->length>STORAGE_SWD_NOR_BYTES-request->offset){
        result=STORAGE_SWD_ARGUMENT;
    }
    g_storage_swd.request_result=result;
    if(result!=STORAGE_SWD_OK){
        ++g_storage_swd.errors;
        g_storage_swd.rejected_seq=request->seq;
        if(running==0U)Publish(request->seq,request->offset,request->length,result,0U,0U);
        return;
    }
    /* 완료 응답 무효화를 먼저 게시한 다음에만 새 NOR bytes가 버퍼를 덮게 한다. */
    g_storage_swd.response_seq=0U;
    SWD_PORT_BARRIER();
    g_storage_swd.state=STORAGE_SWD_BUSY;
    g_storage_swd.response_offset=request->offset;
    g_storage_swd.response_length=request->length;
    g_storage_swd.response_result=STORAGE_SWD_BUSY_RESULT;
    g_storage_swd.response_address=(uint32_t)(uintptr_t)read_buffer;
    g_storage_swd.response_crc32=0U;
    g_storage_swd.response_completed=0U;
    active_seq=request->seq;active_offset=request->offset;active_length=request->length;
    completed=0U;crc_state=UINT32_MAX;running=1U;
    g_storage_swd.active_seq=active_seq;
    ++g_storage_swd.accepted;
    SWD_PORT_BARRIER();
}

/* 호출당 한 request snapshot과 최대 한 4096-byte read만 처리한다. 진행 중의 원래
 * request 필드를 다시 사용하지 않으므로 host가 다음 요청을 써도 범위가 바뀌지 않는다.
 * read error 이후 자동 재시도/추가 read를 하지 않고 ERROR를 commit한다. */
void StorageSWD_Process(void)
{
    RequestSnapshot request;
    uint32_t length,result;
    ++g_storage_swd.polls;
    if(CaptureRequest(&request))AcceptOrReject(&request);
    if(running==0U)return;
    length=active_length-completed;
    if(length>STORAGE_SWD_POLL_BYTES)length=STORAGE_SWD_POLL_BYTES;
    if(!SWD_PORT_RAM_READY()||!SWD_PORT_NOR_READY())result=STORAGE_SWD_NOT_READY;
    else result=SWD_PORT_READ(active_offset+completed,read_buffer+completed,length)==0U
                    ?STORAGE_SWD_OK:STORAGE_SWD_READ_ERROR;
    if(result!=STORAGE_SWD_OK){
        running=0U;++g_storage_swd.errors;
        Publish(active_seq,active_offset,active_length,result,completed,0U);
        return;
    }
    crc_state=CrcFeed(crc_state,read_buffer+completed,length);
    completed+=length;
    g_storage_swd.response_completed=completed;
    if(completed==active_length){
        running=0U;
        Publish(active_seq,active_offset,active_length,STORAGE_SWD_OK,completed,crc_state^UINT32_MAX);
    }
}

/* SWD host용 descriptor와 같은 seq/state 검사를 CPU caller에도 제공한다. IRQ/태스크
 * 선점을 막지 않으며 상위가 실패를 다음 loop에 재시도하도록 최대3회로 제한한다.
 * 요청 작성 중 seq=0인 snapshot은 진단으로만 유효하고 실행 요청으로 쓰지 않는다. */
uint32_t StorageSWD_GetDiagnostics(StorageSWD_Mailbox *out)
{
    uint32_t attempt;
    if(out==NULL)return 0U;
    for(attempt=0U;attempt<3U;++attempt){
        uint32_t req=SWD_PORT_REQUEST_SEQ();
        uint32_t seq=g_storage_swd.response_seq,state=g_storage_swd.state;
        StorageSWD_Mailbox snapshot;
        SWD_PORT_BARRIER();snapshot=g_storage_swd;SWD_PORT_BARRIER();
        if(req==SWD_PORT_REQUEST_SEQ()&&seq==g_storage_swd.response_seq&&state==g_storage_swd.state){
            *out=snapshot;return 1U;
        }
    }
    return 0U;
}
