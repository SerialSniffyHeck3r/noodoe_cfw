#ifndef UPDATE_SERVICE_H
#define UPDATE_SERVICE_H
#include "NDCP.h"
#include "InstallSession.h"
#include "Recovery_Target.h"
#define UPDATE_APP_BASE 0x08010000U
#define UPDATE_APP_BYTES 0x00070000U
#define UPDATE_STAGE_BASE 0x07F90000U
#define UPDATE_STAGE_ARM 0x53544147U
#define UPDATE_COMMIT_ARM 0x434F4D54U
#define UPDATE_RESET_ARM 0x52535421U
#define UPDATE_QUEUE_DEPTH 2U
#define UPDATE_REPLY_MAX 544U
#define UPDATE_OP_LOCAL_DATA 0x49U
#define UPDATE_DATA_MAX 960U
enum { UPDATE_OP_BEGIN=0x40,UPDATE_OP_DATA,UPDATE_OP_FINISH,UPDATE_OP_COMMIT,
       UPDATE_OP_ABORT,UPDATE_OP_STATUS,UPDATE_OP_READ_STAGE,UPDATE_OP_RESET };
typedef enum { UPDATE_IDLE=0,UPDATE_RECEIVING,UPDATE_VERIFYING,UPDATE_VERIFIED,
               UPDATE_COMMITTED,UPDATE_RESET_WAIT,UPDATE_FAILED } UpdateState;
typedef enum { UPDATE_OK=0,UPDATE_ARGUMENT,UPDATE_LOCKED,UPDATE_BUSY,UPDATE_STATE,
               UPDATE_PENDING,UPDATE_IO,UPDATE_HASH,UPDATE_VECTOR,UPDATE_OFFSET,
               UPDATE_NOT_CONNECTED,UPDATE_COMMIT_AMBIGUOUS,UPDATE_COMMIT_REJECTED } UpdateResult;
/* BEGIN's formerly reserved final word is a typed target. Legacy0 remains
 * CFW and retains the resource contract. STOCK is exact approved V5.16 only. */
typedef enum {UPDATE_TARGET_CFW=0,UPDATE_TARGET_STOCK=1,UPDATE_TARGET_GATE_CFW=2,UPDATE_TARGET_UNINSTALL=3,UPDATE_TARGET_DIAGNOSTIC=4} UpdateTarget;
#define UPDATE_GATE_BYTES 0x60000U
#define UPDATE_GATE_BASE 0x08020000U

/* callbacks는 StorageTask에서만 실행한다.0은성공이며 read/program/erase는
 * 명시한 범위를 모두 처리하거나 실패해야 한다. metadata_commit은 sector2만
 * 변경하고 CRC를 마지막에 기록한다. reset은 명시RESET응답 확인 또는 별도 물리
 * 승인으로 시작한 local_install의 전체 검증·확정 뒤에만 호출된다. */
typedef struct {
    void *context;
    uint32_t (*read)(void *,uint32_t,void *,uint32_t);
    uint32_t (*enable)(void *,uint32_t);
    uint32_t (*erase4k)(void *,uint32_t);
    uint32_t (*program)(void *,uint32_t,const void *,uint32_t);
    void (*disable)(void *);
    uint32_t (*metadata_read)(void *,uint32_t words[5]);
    uint32_t (*metadata_commit)(void *,uint32_t version,uint32_t crc,uint32_t arm);
    void (*reset)(void *);
    /* Gate target commits an external journal instead of resident S2. */
    uint32_t (*gate_committed)(void *,const uint8_t sha[32]);
    uint32_t (*resume)(void *,uint32_t,uint32_t,const uint8_t sha[32],uint32_t *offset);
    uint32_t (*checkpoint)(void *,uint32_t offset);
    /* Optional proof that the failed commit did not start a persistent write.
     * Absent/false is ambiguous. Never infer this from an empty CRC alone. */
    uint32_t (*commit_untouched)(void *);
    /* Optional Bootstrap-only verified local Product source. Address is an
     * APP-relative offset; Product adapter leaves this capability absent. */
    uint32_t (*local_source)(void *,uint32_t,void *,uint32_t);
} UpdatePlatform;
typedef struct { uint32_t words[8],used,total;uint8_t block[64]; } UpdateSha256;
typedef struct { uint32_t opcode,sequence,length,generation;uint8_t payload[NDCP_PAYLOAD_MAX]; } UpdateRequest;
typedef struct { uint32_t opcode,sequence,flags,length,generation;uint8_t payload[UPDATE_REPLY_MAX]; } UpdateReply;
typedef struct {
    UpdatePlatform platform;
    volatile uint32_t request_write,request_read,reply_write,reply_read;
    volatile uint32_t connected,authorization,link_generation,sent_sequence,sent_ms,sent_valid;
    UpdateRequest requests[UPDATE_QUEUE_DEPTH];
    UpdateReply replies[UPDATE_QUEUE_DEPTH];
    uint32_t generation,state,result,transaction,version,expected_crc,received,verified;
    uint32_t target,resident_version,commit_uncertain;
    uint32_t local_install,local_reset_ms;
    uint32_t finish_sequence,reset_sequence,reset_generation,crc_state,erased_until;
    uint32_t dropped_requests,processed_requests;
    uint8_t expected_sha[32],actual_sha[32],scratch[4096];
    UpdateSha256 sha;
    InstallSession install;
} UpdateService;

/* 같은 link의 IOTask만 Handle/TakeReply/NotifyReplyTransmitted를 호출하고,
 * StorageTask만 Process를 호출한다. request/reply는각각SPSC acquire/release큐다. */
void UpdateService_Init(UpdateService *,const UpdatePlatform *);
void UpdateService_SetConnected(UpdateService *,uint32_t connected);
/* full NOR backup 검증 뒤 상위 소유자가 명시 호출한다. Begin 패킷 자체는이잠금을
 * 해제하지않는다.0 또는틀린token은잠그며기본/연결종료후는잠김이다. */
void UpdateService_Authorize(UpdateService *,uint32_t token);
uint32_t UpdateService_Handle(UpdateService *,const NDCP_Frame *request);
void UpdateService_Process(UpdateService *,uint32_t now_ms);
/* Storage owner only, between physical operations. Never rolls back a commit. */
uint32_t UpdateService_CancelUncommitted(UpdateService *);
/* Storage owner, physical user approval only. Re-read the complete image,
 * commit once and restart independently of the phone. Never inferred from
 * receiving a file or from a transport ACK. */
uint32_t UpdateService_ConfirmLocal(UpdateService *);
size_t UpdateService_TakeReply(UpdateService *,uint8_t *out,size_t capacity);
/* 전체 해당 NDCP frame의 로컬transport 전송이완료된뒤호출한다. queue에담은것만으로
 * 호출하지않는다. peer수신/설치성공확인은아니며Reset은추가1500ms대기한다.
 * Once the matching RESET ACK is latched, disconnect/other ACKs cannot cancel
 * or postpone the authorized reset. No ACK means no automatic reset. */
void UpdateService_NotifyReplyTransmitted(UpdateService *,uint32_t sequence,uint32_t now_ms);
/* 알려진 SHA256 vector와 실제ARM테스트가동일구현을검증한다. */
void UpdateSha256_Init(UpdateSha256 *);
void UpdateSha256_Feed(UpdateSha256 *,const uint8_t *,size_t);
void UpdateSha256_Final(UpdateSha256 *,uint8_t digest[32]);
uint32_t UpdateService_ImageBytes(const UpdateService *);
#endif
