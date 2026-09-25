#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "RAM codec requires little-endian target"
#endif
#include "Noodoe_Crc32.h"
#include "Update_Service.h"
#include "Recovery_Core.h"
#include <string.h>
#if NOODOE_PRODUCT
#include "Uninstall_Expected.h"
#define UPDATE_ALLOWED_TARGET UPDATE_TARGET_DIAGNOSTIC
#else
#define UPDATE_ALLOWED_TARGET UPDATE_TARGET_GATE_CFW
#endif

/* 큐 생산자/소비자 간 payload 복사 완료를 acquire/release로 전달한다. Cortex-M4의
 * aligned32-bit atomics만 사용하며 mutex/ISR/동적할당이 없다. */
static uint32_t Load(const volatile uint32_t *p){return __atomic_load_n(p,__ATOMIC_ACQUIRE);}
/* 데이터 복사 뒤 index를 publish한다. */
static void Store(volatile uint32_t *p,uint32_t v){__atomic_store_n(p,v,__ATOMIC_RELEASE);}
/* 정렬에 의존하지 않는 wire u32 읽기다. */
static uint32_t U32(const uint8_t *p){uint32_t v;memcpy(&v,p,4);return v;}
/* 진단/응답 정수는 모두 little-endian으로 쓴다. */
/* Preserve the explicit LE32 wire bytes without a byte-store loop at every
 * call site. memcpy also preserves the contract for unaligned RAM buffers. */
#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "This scalar codec requires a little-endian target"
#endif
static void Put(uint8_t *p,uint32_t v){memcpy(p,&v,4);}
/* hash/manifest 비교는 길이가 고정되며 first difference에 관계없이 끝까지 읽는다. */
static uint32_t Equal(const uint8_t *a,const uint8_t *b,uint32_t n){uint32_t i,x=0U;for(i=0;i<n;++i)x|=a[i]^b[i];return x==0U;}
/* streaming ISO CRC는 Final에서만 xor한다. */
static uint32_t CrcFeed(uint32_t crc,const uint8_t *data,uint32_t n){return Noodoe_Crc32Feed(crc,data,n);}
/* 오류가 발생해도 이미쓴NOR/메타를자동지우지않는다. 실패원인만남기고권한을닫는다. */
static void Fail(UpdateService *s,uint32_t error){s->state=UPDATE_FAILED;s->result=error;Store(&s->authorization,0U);if(s->platform.disable)s->platform.disable(s->platform.context);}
/* 부트 resident 및 request=0을 검사한다. 성공후slot/length잔존은정상이다. */
static uint32_t NoPending(UpdateService *s)
{uint32_t m[5];if(!s->platform.metadata_read||s->platform.metadata_read(s->platform.context,m)||
 !RECOVERY_VERSION_SUPPORTED(m[0])||m[4]||(s->resident_version&&s->resident_version!=m[0]))return 0;
 s->resident_version=m[0];return 1;}
/* reply space를확인한뒤StorageTask가생산한다.모든명령에공통20-byte상태를붙인다. */
static uint32_t Reply(UpdateService *s,uint32_t opcode,uint32_t sequence,uint32_t result,const uint8_t *extra,uint32_t n)
{
    uint32_t w=Load(&s->reply_write),r=Load(&s->reply_read);UpdateReply *p;
    if(w-r>=UPDATE_QUEUE_DEPTH || n>UPDATE_REPLY_MAX-20U)return 0U;
    p=&s->replies[w%UPDATE_QUEUE_DEPTH];p->opcode=opcode;p->sequence=sequence;p->flags=NDCP_FLAG_RESPONSE|(result?NDCP_FLAG_ERROR:0U);p->generation=s->generation;p->length=20U+n;
    Put(p->payload,result);Put(p->payload+4,s->state);Put(p->payload+8,s->transaction);Put(p->payload+12,s->received);Put(p->payload+16,s->verified);
    if(n)memcpy(p->payload+20,extra,n);
    Store(&s->reply_write,w+1U);return 1U;
}
/* 순정MSP검사보다엄격하게실제mainSRAM/정렬/자기APP범위ThumbReset을검증한다.
 * CCM/외부RAM stack이나APP외부Reset은이번updater에서허용하지않는다. */
uint32_t UpdateService_ImageBytes(const UpdateService *s)
{return (s->target==UPDATE_TARGET_GATE_CFW||s->target==UPDATE_TARGET_DIAGNOSTIC)?UPDATE_GATE_BYTES:UPDATE_APP_BYTES;}
static uint32_t Vectors(const UpdateService *s,const uint8_t *p)
{uint32_t msp=U32(p),reset=U32(p+4),base=(s->target==UPDATE_TARGET_GATE_CFW||s->target==UPDATE_TARGET_DIAGNOSTIC)?UPDATE_GATE_BASE:UPDATE_APP_BASE;
 #if NOODOE_PRODUCT
 if(s->target==UPDATE_TARGET_UNINSTALL&&(U32(p+0x200)!=0x31424e55U||U32(p+0x204)!=1||U32(p+0x208)!=UPDATE_APP_BASE||U32(p+0x20c)!=UPDATE_APP_BYTES))return 0;
 #endif
 if((s->target==UPDATE_TARGET_GATE_CFW||s->target==UPDATE_TARGET_DIAGNOSTIC)&&(U32(p+0x230)!=0x3250554EU||U32(p+0x234)!=2||U32(p+0x238)!=(s->target==UPDATE_TARGET_DIAGNOSTIC?3U:2U)||U32(p+0x23c)!=UPDATE_GATE_BYTES))return 0;
 return msp>((s->target==UPDATE_TARGET_GATE_CFW||s->target==UPDATE_TARGET_DIAGNOSTIC)?0x20007000U:0x20000000U) && msp<=((s->target==UPDATE_TARGET_GATE_CFW||s->target==UPDATE_TARGET_DIAGNOSTIC)?0x2002FF00U:0x20030000U) && !(msp&7U) && (reset&1U) && (reset&~1U)>=base && (reset&~1U)<base+UpdateService_ImageBytes(s);}
/* 콜백과모든큐/권한을초기화한다. Init은task가시작되기전에한번호출한다. */
void UpdateService_Init(UpdateService *s,const UpdatePlatform *p){if(!s)return;memset(s,0,sizeof(*s));if(p)s->platform=*p;}
/* link epoch로이전연결의응답/요청을새PHONE에노출하지않는다.연결해제는쓰기허가도닫는다. */
void UpdateService_SetConnected(UpdateService *s,uint32_t connected)
{uint32_t old;if(!s)return;connected=!!connected;old=__atomic_exchange_n(&s->connected,connected,__ATOMIC_ACQ_REL);if(old!=connected)__atomic_add_fetch(&s->link_generation,1U,__ATOMIC_ACQ_REL);if(!connected)Store(&s->authorization,0U);}
/* BEGIN의wiretoken과별도로NOR전체backup검증후상위가허용해야쓰기경로가열린다. */
void UpdateService_Authorize(UpdateService *s,uint32_t token){if(s)Store(&s->authorization,token==UPDATE_STAGE_ARM && Load(&s->connected));}
/* IOTask는copy만한다.가득차면BUSY를반환하므로rootdispatcher가같은seq오류응답을
 * 보내거나host가재시도해야한다.실패시metadata/NOR를만지지않는다. */
uint32_t UpdateService_Handle(UpdateService *s,const NDCP_Frame *f)
{
    uint32_t w,r;UpdateRequest *p;
    if(!s||!f||f->opcode<UPDATE_OP_BEGIN||(f->opcode>UPDATE_OP_RESET&&f->opcode!=UPDATE_OP_LOCAL_DATA)||f->flags||f->length>NDCP_PAYLOAD_MAX||(!f->payload&&f->length))return UPDATE_ARGUMENT;
    if(!Load(&s->connected))return UPDATE_NOT_CONNECTED;
    w=Load(&s->request_write);r=Load(&s->request_read);if(w-r>=UPDATE_QUEUE_DEPTH){++s->dropped_requests;return UPDATE_BUSY;}
    p=&s->requests[w%UPDATE_QUEUE_DEPTH];p->opcode=f->opcode;p->sequence=f->sequence;p->length=f->length;p->generation=Load(&s->link_generation);if(f->length)memcpy(p->payload,f->payload,f->length);
    Store(&s->request_write,w+1U);return UPDATE_OK;
}
/* 태스크경계를넘는reply를완성frame으로복사한다. caller버퍼부족은소비하지않는다. */
size_t UpdateService_TakeReply(UpdateService *s,uint8_t *out,size_t cap)
{
    uint32_t r,w;UpdateReply *p;size_t n;
    if(!s)return 0U;
    for(;;){r=Load(&s->reply_read);w=Load(&s->reply_write);if(r==w)return 0U;p=&s->replies[r%UPDATE_QUEUE_DEPTH];if(p->generation==Load(&s->link_generation))break;Store(&s->reply_read,r+1U);}
    n=NDCP_Encode(out,cap,p->opcode,p->flags,p->sequence,p->payload,p->length);if(n)Store(&s->reply_read,r+1U);return n;
}
/* Reset ACK의실제local전송완료시각만publish한다. 아직 peer가받았다는증거는아니다. */
void UpdateService_NotifyReplyTransmitted(UpdateService *s,uint32_t sequence,uint32_t now)
{
    /* Latch only this RESET's first local completion. STATUS/duplicate ACKs
     * must not overwrite its deadline; stale connections cannot arm it. */
    if(!s||s->state!=UPDATE_RESET_WAIT||sequence!=s->reset_sequence||
       Load(&s->link_generation)!=s->reset_generation||Load(&s->sent_valid))return;
    Store(&s->sent_ms,now);Store(&s->sent_sequence,sequence);Store(&s->sent_valid,1U);
}
/* manifest는fixed448KiB,ISOCRCnot0/notFFFFFFFF,nonzeroTXID,SHA256,별도intent다.
 * 동일BEGIN재시도는현재세션만재확인하며부분NOR를다시erase하지않는다. */
static uint32_t Begin(UpdateService *s,const UpdateRequest *r)
{
    const uint8_t *p=r->payload;uint32_t tx,crc;
    if(r->length!=56U||U32(p+48)!=UPDATE_STAGE_ARM||U32(p+52)>UPDATE_ALLOWED_TARGET)return UPDATE_ARGUMENT;
    uint32_t target=U32(p+52),bytes=(target==UPDATE_TARGET_GATE_CFW||target==UPDATE_TARGET_DIAGNOSTIC)?UPDATE_GATE_BYTES:UPDATE_APP_BYTES;
    if(U32(p+8)!=bytes)return UPDATE_ARGUMENT;
#if NOODOE_PRODUCT
    /* Resident CFW installs erase S4 and would destroy the recovery gate. */
    if(target==UPDATE_TARGET_UNINSTALL){
        static const uint8_t approved[32]=UNINSTALL_IMAGE_SHA;
        if(U32(p+4)!=UNINSTALL_TRANSPORT_VERSION||!Equal(p+16,approved,32))return UPDATE_HASH;
    }else if(target!=UPDATE_TARGET_GATE_CFW&&target!=UPDATE_TARGET_DIAGNOSTIC)return UPDATE_LOCKED;
#else
    if(target>=UPDATE_TARGET_GATE_CFW)return UPDATE_LOCKED;
#endif
    if(U32(p+52)==UPDATE_TARGET_STOCK&&!RecoveryStock_Matches(U32(p+4),p+16))return UPDATE_HASH;
    if(s->commit_uncertain)return UPDATE_PENDING;
    tx=U32(p);crc=U32(p+12);if(!tx||!crc||crc==UINT32_MAX||U32(p+4)==UINT32_MAX)return UPDATE_ARGUMENT;
    if(!Load(&s->authorization))return UPDATE_LOCKED;
    if(!NoPending(s))return UPDATE_PENDING;
    if(s->state==UPDATE_RECEIVING && s->transaction==tx && s->version==U32(p+4) && s->expected_crc==crc && s->target==U32(p+52) && Equal(s->expected_sha,p+16,32U))return UPDATE_OK;
    if(s->state!=UPDATE_IDLE&&s->state!=UPDATE_FAILED)return UPDATE_STATE;
    s->target=target;
    if(!s->platform.enable||!s->platform.erase4k||!s->platform.program||!s->platform.read||s->platform.enable(s->platform.context,tx))return UPDATE_IO;
    if(!InstallSession_Active(&s->install))InstallSession_Begin(&s->install,s->install.now,tx);
    s->transaction=tx;s->version=U32(p+4);s->target=U32(p+52);s->expected_crc=crc;memcpy(s->expected_sha,p+16,32U);memset(s->actual_sha,0,32U);
    s->received=s->verified=s->erased_until=0U;
    if((s->target==UPDATE_TARGET_GATE_CFW||s->target==UPDATE_TARGET_DIAGNOSTIC)&&s->platform.resume){
        uint32_t offset=0;
        if(s->platform.resume(s->platform.context,s->version,s->expected_crc,s->expected_sha,&offset)||
           offset>bytes||(offset&4095U)){Fail(s,UPDATE_IO);return UPDATE_IO;}
        s->received=s->erased_until=offset;
    }
    s->state=UPDATE_RECEIVING;s->result=UPDATE_OK;return UPDATE_OK;
}
/* chunk는연속offset만쓰며첫4KiB접근때erase한다.이미ACK된chunk재전송은
 * byte readback이같을때만ACK하고다시쓰지않는다. write/readback실패는세션종료다. */
static uint32_t Data(UpdateService *s,const UpdateRequest *r)
{
    const uint8_t *p=r->payload;uint32_t offset,n,pos=0U;
    if(r->length<=8U||r->length>8U+UPDATE_DATA_MAX||U32(p)!=s->transaction)return UPDATE_ARGUMENT;
    if(s->state!=UPDATE_RECEIVING)return UPDATE_STATE;
    if(!Load(&s->authorization))return UPDATE_LOCKED;
    offset=U32(p+4);n=r->length-8U;if(offset>UpdateService_ImageBytes(s)||n>UpdateService_ImageBytes(s)-offset)return UPDATE_ARGUMENT;
    if(n>4096U-(offset&4095U)||((offset|n)&1U))return UPDATE_ARGUMENT;
    if(offset<s->received){if(n>s->received-offset)return UPDATE_OFFSET;if(s->platform.read(s->platform.context,UPDATE_STAGE_BASE+offset,s->scratch,n))return UPDATE_IO;return Equal(s->scratch,p+8,n)?UPDATE_OK:UPDATE_OFFSET;}
    if(offset!=s->received)return UPDATE_OFFSET;
    if(!NoPending(s))return UPDATE_PENDING;
    while(pos<n){uint32_t at=offset+pos,page=256U-(at&255U),take=n-pos;if(take>page)take=page;
        if(at>=s->erased_until){if(s->platform.erase4k(s->platform.context,UPDATE_STAGE_BASE+(at&~4095U))){Fail(s,UPDATE_IO);return UPDATE_IO;}s->erased_until=(at&~4095U)+4096U;}
        if(s->platform.program(s->platform.context,UPDATE_STAGE_BASE+at,p+8+pos,take)){Fail(s,UPDATE_IO);return UPDATE_IO;}pos+=take;
    }
    if(s->platform.read(s->platform.context,UPDATE_STAGE_BASE+offset,s->scratch,n)||!Equal(s->scratch,p+8,n)){Fail(s,UPDATE_IO);return UPDATE_IO;}
    if((s->target==UPDATE_TARGET_GATE_CFW||s->target==UPDATE_TARGET_DIAGNOSTIC)&&s->platform.checkpoint&&s->platform.checkpoint(s->platform.context,s->received+n)){Fail(s,UPDATE_IO);return UPDATE_IO;}
    s->received+=n;return UPDATE_OK;
}
/* 전체NOR재읽기검증은Process마다4KiB씩진행해StorageTask의긴점유를피한다.
 * Finish응답은최종SHA/CRC일치후에만생성한다. */
static void VerifyStep(UpdateService *s)
{
    uint32_t n=UpdateService_ImageBytes(s)-s->verified;if(n>sizeof(s->scratch))n=sizeof(s->scratch);
    if(n){if(s->platform.read(s->platform.context,UPDATE_STAGE_BASE+s->verified,s->scratch,n)){Fail(s,UPDATE_IO);}
        else if(!s->verified&&!Vectors(s,s->scratch)){Fail(s,UPDATE_VECTOR);}
        else {UpdateSha256_Feed(&s->sha,s->scratch,n);s->crc_state=CrcFeed(s->crc_state,s->scratch,n);s->verified+=n;}}
    if(s->state==UPDATE_VERIFYING&&s->verified==UpdateService_ImageBytes(s)){UpdateSha256_Final(&s->sha,s->actual_sha);if((s->crc_state^UINT32_MAX)!=s->expected_crc||!Equal(s->actual_sha,s->expected_sha,32U))Fail(s,UPDATE_HASH);else{s->state=UPDATE_VERIFIED;s->result=UPDATE_OK;if(s->platform.disable)s->platform.disable(s->platform.context);}}
    if(s->state!=UPDATE_VERIFYING&&!s->local_install)Reply(s,UPDATE_OP_FINISH,s->finish_sequence,s->result,s->actual_sha,32U);
}
/* Commit은NOR전체SHA/CRC검증완료상태·동일TXID/SHA·명시token·pending없음을
 * 요구한다. metadata_commit실패는erase재시도하지않고결과불명확상태로남긴다. */
static uint32_t CommitVerified(UpdateService *s)
{
    uint32_t m[5];
    if(s->state==UPDATE_COMMITTED)return UPDATE_OK;
    if(s->state!=UPDATE_VERIFIED)return UPDATE_STATE;
    if(!s->local_install&&!Load(&s->authorization))return UPDATE_LOCKED;
    if(!NoPending(s))return UPDATE_PENDING;
    if(!s->platform.metadata_commit)return UPDATE_LOCKED;
    s->commit_uncertain=1;
    if(s->platform.metadata_commit(s->platform.context,s->version,s->expected_crc,UPDATE_COMMIT_ARM)) {
        /* Transport/policy rejection before the writer is not an uncertain
         * erase. Require an explicit adapter proof AND unchanged idle metadata. */
        if(s->platform.commit_untouched&&s->platform.commit_untouched(s->platform.context)&&NoPending(s)){
            s->commit_uncertain=0;Fail(s,UPDATE_COMMIT_REJECTED);return UPDATE_COMMIT_REJECTED;
        }
        Fail(s,UPDATE_COMMIT_AMBIGUOUS);return UPDATE_COMMIT_AMBIGUOUS;
    }
    if((s->target==UPDATE_TARGET_GATE_CFW||s->target==UPDATE_TARGET_DIAGNOSTIC)){
        if(!s->platform.gate_committed||!s->platform.gate_committed(s->platform.context,s->expected_sha)){Fail(s,UPDATE_COMMIT_AMBIGUOUS);return UPDATE_COMMIT_AMBIGUOUS;}
    }else if(s->platform.metadata_read(s->platform.context,m)||m[0]!=s->resident_version||m[1]!=s->version||m[2]!=0x7F90U||m[3]!=UPDATE_APP_BYTES||m[4]!=s->expected_crc){Fail(s,UPDATE_COMMIT_AMBIGUOUS);return UPDATE_COMMIT_AMBIGUOUS;}
    s->commit_uncertain=0;s->state=UPDATE_COMMITTED;if(s->platform.disable)s->platform.disable(s->platform.context);Store(&s->authorization,0U);return UPDATE_OK;
}
static uint32_t Commit(UpdateService *s,const UpdateRequest *r)
{
    if(r->length!=40U||U32(r->payload)!=s->transaction||U32(r->payload+4)!=UPDATE_COMMIT_ARM||!Equal(r->payload+8,s->expected_sha,32U))return UPDATE_ARGUMENT;
    return CommitVerified(s);
}
/* A completed, locally approved image owns its immutable staging range until
 * commit/restart. Rechecking never erases it and survives a link disconnect. */
uint32_t UpdateService_ConfirmLocal(UpdateService *s)
{
    if(!s||s->local_install||s->commit_uncertain||s->state!=UPDATE_VERIFIED||
       !s->transaction||s->received!=UpdateService_ImageBytes(s)||
       s->verified!=s->received||!Equal(s->actual_sha,s->expected_sha,32)||
       !s->platform.reset)return UPDATE_STATE;
    s->local_install=1;s->state=UPDATE_VERIFYING;s->verified=0;
    s->crc_state=UINT32_MAX;UpdateSha256_Init(&s->sha);return UPDATE_OK;
}
/* opcode별검사뒤부작용을실행한다.STATUS/READ_STAGE는쓰기허가가없어도읽기만한다. */
static void Request(UpdateService *s,UpdateRequest *r)
{
    uint32_t result=UPDATE_ARGUMENT,extra=0U;uint8_t info[UPDATE_REPLY_MAX-20U];
    if(s->local_install&&r->opcode!=UPDATE_OP_STATUS&&r->opcode!=UPDATE_OP_READ_STAGE){
        Reply(s,r->opcode,r->sequence,UPDATE_PENDING,NULL,0);return;
    }
    switch(r->opcode){
    case UPDATE_OP_BEGIN: result=Begin(s,r);break;
    case UPDATE_OP_DATA: result=Data(s,r);break;
    case UPDATE_OP_LOCAL_DATA:{
        uint32_t offset=U32(r->payload+4),n=U32(r->payload+8);
        if(r->length!=12||U32(r->payload)!=s->transaction||s->target!=UPDATE_TARGET_CFW||
           s->state!=UPDATE_RECEIVING||!Load(&s->authorization)||!s->platform.local_source||
           offset<0x10000||offset>UPDATE_APP_BYTES||!n||n>UPDATE_DATA_MAX||n>UPDATE_APP_BYTES-offset||
           n>4096U-(offset&4095U)||((offset|n)&1U))break;
        if(s->platform.local_source(s->platform.context,offset-0x10000,r->payload+8,n)){result=UPDATE_IO;break;}
        r->length=8+n;result=Data(s,r);break;}

    case UPDATE_OP_FINISH:
        if(r->length!=4U||U32(r->payload)!=s->transaction)break;
        if(s->commit_uncertain){result=UPDATE_PENDING;break;}
        /* A link loss after the last DATA (or during readback) need not erase
         * a complete image. Only that failure admits a fresh full validation. */
        if((s->state!=UPDATE_RECEIVING&&s->state!=UPDATE_VERIFIED&&
           !(s->state==UPDATE_FAILED&&s->result==UPDATE_NOT_CONNECTED))||
           s->received!=UpdateService_ImageBytes(s)){result=UPDATE_STATE;break;}
        s->state=UPDATE_VERIFYING;s->verified=0U;s->crc_state=UINT32_MAX;s->finish_sequence=r->sequence;UpdateSha256_Init(&s->sha);return;
    case UPDATE_OP_COMMIT: result=Commit(s,r);break;
    case UPDATE_OP_ABORT:
        if(r->length!=4U||U32(r->payload)!=s->transaction)break;
        if(s->commit_uncertain||s->state==UPDATE_COMMITTED||s->state==UPDATE_RESET_WAIT){if(s->state==UPDATE_RESET_WAIT&&!Load(&s->sent_valid))s->state=UPDATE_COMMITTED;result=UPDATE_PENDING;break;}
        s->state=UPDATE_IDLE;
        if(s->platform.disable)s->platform.disable(s->platform.context);
        s->result=UPDATE_OK;Store(&s->authorization,0U);result=UPDATE_OK;break;
    case UPDATE_OP_STATUS:
        if(r->length)break;
        Put(info,s->version);Put(info+4,s->expected_crc);Put(info+8,Load(&s->authorization));
        {uint32_t m[5]={0};if(!s->platform.metadata_read||s->platform.metadata_read(s->platform.context,m)){result=UPDATE_IO;break;}for(uint32_t i=0;i<5U;++i)Put(info+12U+4U*i,m[i]);}
        memcpy(info+32,s->actual_sha,32U);extra=64U;result=UPDATE_OK;break;
    case UPDATE_OP_READ_STAGE:
        if(r->length==8U){uint32_t offset=U32(r->payload),n=U32(r->payload+4);if(n&&n<=512U&&offset<=UpdateService_ImageBytes(s)&&n<=UpdateService_ImageBytes(s)-offset){if(!s->platform.read||s->platform.read(s->platform.context,UPDATE_STAGE_BASE+offset,info+8,n))result=UPDATE_IO;else{Put(info,offset);Put(info+4,n);extra=n+8U;result=UPDATE_OK;}}}break;
    case UPDATE_OP_RESET:
        if(r->length!=8U||U32(r->payload)!=s->transaction||U32(r->payload+4)!=UPDATE_RESET_ARM)break;
        if(s->state!=UPDATE_COMMITTED||!s->platform.reset){result=UPDATE_STATE;break;}
        s->state=UPDATE_RESET_WAIT;s->reset_sequence=r->sequence;s->reset_generation=s->generation;Store(&s->sent_valid,0U);result=UPDATE_OK;break;
    default:break;
    }
    /* A successful STATUS response is not a successful update. Keep the
     * operation's first failure independently of later command replies. */
    if(s->state!=UPDATE_FAILED)s->result=result;
    Reply(s,r->opcode,r->sequence,result,info,extra);
}
/* SPI5소유StorageTask에서매loop호출한다.한request와한4KiB검증step만처리한다.
 * replyqueue가차면부작용실행을미뤄ACK없는commit을피한다. */
void UpdateService_Process(UpdateService *s,uint32_t now)
{
    uint32_t generation,r,w;if(!s)return;
    s->install.now=now;
    /* Physical approval is independent of the SPP epoch and reply queue.
     * Any failed validation/commit stays failed; no automatic write retry. */
    if(s->local_install){
        if(s->state==UPDATE_VERIFYING)VerifyStep(s);
        else if(s->state==UPDATE_VERIFIED){
            uint32_t result=CommitVerified(s);
            if(result){if(s->state!=UPDATE_FAILED)Fail(s,result);}
            else{s->local_install=2;s->local_reset_ms=now;}
        }else if(s->state==UPDATE_COMMITTED&&s->local_install==2&&now-s->local_reset_ms>=1500U){
            s->local_install=3;s->platform.reset(s->platform.context);
            s->commit_uncertain=1;Fail(s,UPDATE_COMMIT_AMBIGUOUS);
        }
    }
    /* An acknowledged explicit RESET is already authorized. Losing RFCOMM
     * during its drain interval cannot revoke that decision and strand a
     * committed installation. The adapter still rechecks durable metadata.
     * No COMMIT, write, or RESET request is synthesized here. */
    if(s->state==UPDATE_RESET_WAIT&&Load(&s->sent_valid)&&Load(&s->sent_sequence)==s->reset_sequence&&now-Load(&s->sent_ms)>=1500U){
        s->state=UPDATE_COMMITTED;s->platform.reset(s->platform.context);
        /* A real system reset never returns. A rejected reset must not leave
         * a healthy loop claiming installation is still moving forward. */
        s->commit_uncertain=1;Fail(s,UPDATE_COMMIT_AMBIGUOUS);return;
    }
    generation=Load(&s->link_generation);
    /* 검증완료이미지는다음명시Commit연결에서도사용할수있게RAMmanifest를남긴다.
     * 쓰기허가는연결종료시닫히며새연결의상위backup승인없이는Commit하지못한다. */
    if(s->generation!=generation){s->generation=generation;if(!s->local_install&&(s->state==UPDATE_RECEIVING||s->state==UPDATE_VERIFYING))Fail(s,UPDATE_NOT_CONNECTED);if(s->state==UPDATE_RESET_WAIT&&!Load(&s->sent_valid))s->state=UPDATE_COMMITTED;}
    if(!Load(&s->connected))return;
    if(Load(&s->reply_write)-Load(&s->reply_read)>=UPDATE_QUEUE_DEPTH)return;
    r=Load(&s->request_read);w=Load(&s->request_write);
    if(r!=w){UpdateRequest *p=&s->requests[r%UPDATE_QUEUE_DEPTH];if(p->generation==s->generation){++s->processed_requests;Request(s,p);}Store(&s->request_read,r+1U);}
    if(!s->local_install&&s->state==UPDATE_VERIFYING&&Load(&s->reply_write)-Load(&s->reply_read)<UPDATE_QUEUE_DEPTH)VerifyStep(s);
}

/* Local cancellation is an owner operation between physical operations.
 * Committed/ambiguous metadata can only be reconciled, never discarded. */
uint32_t UpdateService_CancelUncommitted(UpdateService *s)
{
 if(!s)return UPDATE_OK;
 if(s->commit_uncertain||s->state==UPDATE_COMMITTED||s->state==UPDATE_RESET_WAIT||s->result==UPDATE_COMMIT_AMBIGUOUS)return UPDATE_PENDING;
 s->local_install=0;
 if(s->platform.disable)s->platform.disable(s->platform.context);
 Store(&s->authorization,0U);Store(&s->request_read,Load(&s->request_write));
 Store(&s->reply_read,Load(&s->reply_write));s->state=UPDATE_IDLE;s->result=UPDATE_OK;
 return UPDATE_OK;
}
