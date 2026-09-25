#ifndef STORAGE_SWD_H
#define STORAGE_SWD_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STORAGE_SWD_MAGIC 0x31445753UL
#define STORAGE_SWD_ABI 1U
#define STORAGE_SWD_MAILBOX_BYTES 128U
#define STORAGE_SWD_REQUEST_MAGIC 0x31514552UL
#define STORAGE_SWD_READ_ARM 0x52454144UL
#define STORAGE_SWD_BUFFER_BYTES 0x00800000UL
#define STORAGE_SWD_NOR_BYTES 0x08000000UL
#define STORAGE_SWD_POLL_BYTES 4096U

typedef enum { STORAGE_SWD_IDLE=0, STORAGE_SWD_BUSY=1,
               STORAGE_SWD_READY=2, STORAGE_SWD_ERROR=3 } StorageSWD_State;
typedef enum { STORAGE_SWD_OK=0, STORAGE_SWD_ARGUMENT=1,
               STORAGE_SWD_NOT_READY=2, STORAGE_SWD_NO_MEMORY=3,
               STORAGE_SWD_BUSY_RESULT=4, STORAGE_SWD_READ_ERROR=5,
               STORAGE_SWD_BUFFER_ERROR=6 } StorageSWD_Result;

/* ABI v1: 전부 little-endian uint32_t, 총 128 bytes, 괄호는 byte offset이다.
 * Host는 request_magic..request_seq(48..64) 다섯 word만 SWD로 쓴다.
 * 나머지는 StorageTask 소유 진단이며 host가 변경하면 계약 밖이다.
 * request_seq=0을 먼저 써 이전 요청을 무효화하고 48..60 필드를 쓴 뒤,
 * 이전 시도와 다른 nonzero request_seq를 마지막에 쓴다. single outstanding만 지원한다.
 * BUSY에서 새 seq는 소비·거절되고 request_result/rejected_seq에만 기록된다.
 * 원 작업은 계속되며 busy 시도 재전송에는 완료 뒤 새 seq가 필요하다.
 * READY/ERROR 완료 응답은 descriptor/버퍼→DMB→response_seq 순서로 게시한다.
 * host는 response_seq/state를 먼저 읽고 descriptor와 버퍼를 읽은 뒤 같은
 * response_seq/state/descriptor인지 다시 확인한다. BUSY의 response_seq=0은 미완료다.
 * 버퍼는 유효한 새 요청을 수락하기 전까지 불변이다. 잘못된 요청과 Init 재호출은
 * 버퍼를 변경하지 않는다. 새 요청을 수락하면 이전 완료 응답은 무효가 된다. */
typedef struct {
    uint32_t magic;                  /* 0 */
    uint32_t abi;                    /* 4 */
    uint32_t mailbox_bytes;          /* 8 */
    uint32_t state;                  /* 12: StorageSWD_State */
    uint32_t init_result;            /* 16: StorageSWD_Result */
    uint32_t buffer_address;         /* 20: 전용 SDRAM byte 주소 */
    uint32_t buffer_capacity;        /* 24: 성공 시 8 MiB */
    uint32_t nor_capacity;           /* 28: 128 MiB */
    uint32_t uid0;                   /* 32: UID_BASE+0 */
    uint32_t uid1;                   /* 36: UID_BASE+4 */
    uint32_t uid2;                   /* 40: UID_BASE+8 */
    uint32_t jedec_id;               /* 44: 관측한 NOR JEDEC */
    uint32_t request_magic;          /* 48: STORAGE_SWD_REQUEST_MAGIC */
    uint32_t request_arm;            /* 52: STORAGE_SWD_READ_ARM */
    uint32_t request_offset;         /* 56: NOR 상대 byte 주소 */
    uint32_t request_length;         /* 60: 1..8 MiB */
    uint32_t request_seq;            /* 64: Host commit LAST */
    uint32_t active_seq;             /* 68: 현재/마지막 수락한 요청 */
    uint32_t response_offset;        /* 72: 완료 응답의 요청 offset */
    uint32_t response_length;        /* 76: 완료 응답의 요청 length */
    uint32_t response_result;        /* 80: StorageSWD_Result */
    uint32_t response_address;       /* 84: 완성/부분 버퍼 byte 주소 */
    uint32_t response_crc32;         /* 88: 성공 시 전체 length의 ISO CRC32 */
    uint32_t response_completed;     /* 92: 성공적으로 읽은 bytes */
    uint32_t response_seq;           /* 96: Device commit LAST, 0=미완료 */
    uint32_t request_result;         /* 100: 마지막 안정 seq의 수락/거절 */
    uint32_t last_request_seq;       /* 104: 마지막 소비한 안정 seq */
    uint32_t rejected_seq;           /* 108: 마지막 거절된 seq */
    uint32_t polls;                  /* 112: Process 호출수, modulo32 */
    uint32_t accepted;               /* 116: 수락수, modulo32 */
    uint32_t errors;                 /* 120: 오류/거절수, modulo32 */
    uint32_t busy_rejections;        /* 124: BUSY 중 거절수, modulo32 */
} StorageSWD_Mailbox;

extern volatile StorageSWD_Mailbox g_storage_swd;

/* SPI5 소유 StorageTask에서 RAM/NOR 초기화 이후 호출한다. 준비 완료/물리 RAM 범위를
 * 검증한 뒤 BSP_RAM_Allocate로 전용 8 MiB를 한 번만 확보한다. 실패 후 재호출은
 * 허용하지만 성공 후 재호출은 mailbox/버퍼를 초기화하지 않는다. 파일시스템을 열거나
 * 장치를 초기화하지 않으며 NOR/내부 FLASH/옵션 바이트에는 쓰지 않는다. */
uint32_t StorageSWD_Init(void);

/* 동일 StorageTask에서 polling한다. 한 번에 NOR 최대 4096 bytes만 읽고 ISO-HDLC
 * CRC32(EDB88320, initial/final FFFFFFFF)를 이어 계산한다. HAL tick/RTOS 의존성이
 * 없는 제어 함수이며 BSP_NOR_Read의 한정된 대기는 포함한다. 다른 태스크/ISR에서
 * 동시에 호출하지 않는다. 자체적으로 FLASH program/erase/unlock을 호출하지 않는다. */
void StorageSWD_Process(void);

/* 전역 진단을 호출자 버퍼로 복사한다. seq/state가 복사 중 바뀌면 최대 3회 시도 후
 * 0을 반환하고 출력은 보존한다. 성공은 1이다. request_seq=0인 host 작성 중 필드는
 * 유효 요청으로 해석하지 않는다. 버퍼 내용까지 복사하거나 장치를 읽지는 않는다. */
uint32_t StorageSWD_GetDiagnostics(StorageSWD_Mailbox *out);

#if defined(__cplusplus)
#define STORAGE_SWD_ASSERT(condition,message) static_assert(condition,message)
#else
#define STORAGE_SWD_ASSERT(condition,message) _Static_assert(condition,message)
#endif
STORAGE_SWD_ASSERT(sizeof(uint32_t)==4U,"StorageSWD requires 32-bit words");
STORAGE_SWD_ASSERT(sizeof(StorageSWD_Mailbox)==128U,"StorageSWD ABI size");
STORAGE_SWD_ASSERT(offsetof(StorageSWD_Mailbox,request_magic)==48U,"StorageSWD request offset");
STORAGE_SWD_ASSERT(offsetof(StorageSWD_Mailbox,request_seq)==64U,"StorageSWD request commit offset");
STORAGE_SWD_ASSERT(offsetof(StorageSWD_Mailbox,response_offset)==72U,"StorageSWD response offset");
STORAGE_SWD_ASSERT(offsetof(StorageSWD_Mailbox,response_seq)==96U,"StorageSWD response commit offset");
STORAGE_SWD_ASSERT(offsetof(StorageSWD_Mailbox,busy_rejections)==124U,"StorageSWD tail offset");
#undef STORAGE_SWD_ASSERT

#ifdef __cplusplus
}
#endif
#endif
