#include "test_common.h"
#include "Update_Service.h"
#include "Recovery_Core.h"
#include "update_fixture.h"
static UpdateService service;
static uint8_t nor[UPDATE_APP_BYTES],image_chunk[512],manifest[56],command_data[1024],reply[NDCP_FRAME_MAX];
static uint32_t meta[5],enabled,erases,programs,commits,resets,fail_erase,sequence,now;

/* fakeNOR는테스트전용RAM이며실제주소경계와pageprogram의1→0조건을검사한다. */
static uint32_t Read(void *ctx,uint32_t address,void *out,uint32_t n)
{ (void)ctx;if(address<UPDATE_STAGE_BASE||address>UPDATE_STAGE_BASE+UPDATE_APP_BYTES||n>UPDATE_STAGE_BASE+UPDATE_APP_BYTES-address)return 1U;memcpy(out,nor+address-UPDATE_STAGE_BASE,n);return 0U; }
/* writer권한을장치대신메모리상태로모델링한다. */
static uint32_t Enable(void *ctx,uint32_t tx){(void)ctx;enabled=tx;return tx?0U:1U;}
/* erase는APPstage의정확한4KiB경계만허용한다. */
static uint32_t Erase(void *ctx,uint32_t address)
{(void)ctx;if(fail_erase||!enabled||address<UPDATE_STAGE_BASE||address>=UPDATE_STAGE_BASE+UPDATE_APP_BYTES||(address&4095U))return 1U;memset(nor+address-UPDATE_STAGE_BASE,0xFF,4096U);++erases;return 0U;}
/* program은명시stage주소와동일page안의최대256byte만허용한다. */
static uint32_t Program(void *ctx,uint32_t address,const void *data,uint32_t n)
{const uint8_t *p=data;uint32_t i,off=address-UPDATE_STAGE_BASE;(void)ctx;if(!enabled||address<UPDATE_STAGE_BASE||!n||n>256U||(address&255U)+n>256U||off>UPDATE_APP_BYTES||n>UPDATE_APP_BYTES-off)return 1U;for(i=0;i<n;++i){if((nor[off+i]&p[i])!=p[i])return 1U;nor[off+i]=p[i];}++programs;return 0U;}
/* 모든종료경로에서권한이닫히는지관측한다. */
static void Disable(void *ctx){(void)ctx;enabled=0U;}
/* 순정메타읽기경계만대역이며실제writer는별도메타시험이맡는다. */
static uint32_t MetaRead(void *ctx,uint32_t out[5]){(void)ctx;memcpy(out,meta,sizeof(meta));return 0U;}
/* 명시COMT와기존pending없는경우에만가상메타를바꾼다. */
static uint32_t MetaCommit(void *ctx,uint32_t version,uint32_t crc,uint32_t token)
{(void)ctx;if(token!=UPDATE_COMMIT_ARM||meta[4])return 1U;meta[1]=version;meta[2]=0x7F90U;meta[3]=UPDATE_APP_BYTES;meta[4]=crc;++commits;return 0U;}
/* MCUreset을실행하지않고호출횟수로별도ACK후딜레이계약을검사한다. */
static void Reset(void *ctx){(void)ctx;++resets;}
/* fixture정수는little-endian으로쓴다. */
static void Put(uint8_t *p,uint32_t n){uint32_t i;for(i=0;i<4U;++i)p[i]=(uint8_t)(n>>(8U*i));}
/* 응답공통status를읽는다. */
static uint32_t Get(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
/* Python hashlib/zlib이동일정의로만든fixture의byte를만든다. */
static uint8_t ImageByte(uint32_t offset)
{static const uint8_t vectors[]={0,0,2,0x20,1,1,1,8};return offset<8U?vectors[offset]:(uint8_t)(offset*13U+7U);}
/* 테스트세션만초기화하며stage는A5로채워erase동작을검출한다. */
static void Initialize(void)
{
    UpdatePlatform p={.read=Read,.enable=Enable,.erase4k=Erase,.program=Program,.disable=Disable,
        .metadata_read=MetaRead,.metadata_commit=MetaCommit,.reset=Reset};
    memset(nor,0xA5,sizeof(nor));memset(meta,0,sizeof(meta));meta[0]=0x000E0000U;
    enabled=erases=programs=commits=resets=fail_erase=0U;now=100U;sequence=0U;
    UpdateService_Init(&service,&p);UpdateService_SetConnected(&service,1U);UpdateService_Process(&service,now);
    memset(manifest,0,sizeof(manifest));Put(manifest,77U);Put(manifest+4,0x00010001U);Put(manifest+8,UPDATE_APP_BYTES);Put(manifest+12,FIXTURE_CRC);memcpy(manifest+16,fixture_sha,32U);Put(manifest+48,UPDATE_STAGE_ARM);
}
/* 비동기Handle→StorageProcess→IOTaskreply를실제공개API로통과시킨다. */
static int Command(uint32_t opcode,const uint8_t *data,uint32_t n,uint32_t expected)
{
    NDCP_Frame f={opcode,0U,++sequence,n,data};size_t count;
    CHECK(UpdateService_Handle(&service,&f)==UPDATE_OK);UpdateService_Process(&service,++now);
    count=UpdateService_TakeReply(&service,reply,sizeof(reply));CHECK(count>=40U);
    CHECK(Get(reply+8)==sequence&&Get(reply+16)==expected);
    CHECK(NDCP_Crc32(reply,count-4U)==Get(reply+count-4U));return 0;
}
/* 고정크기전체이미지를512byte조각으로수신한다.특정변형은검증단계실패용이다. */
static int Stage(uint32_t variant)
{
    uint32_t offset,i,before;int result;
    UpdateService_Authorize(&service,UPDATE_STAGE_ARM);
    result=Command(UPDATE_OP_BEGIN,manifest,sizeof(manifest),UPDATE_OK);if(result)return result;
    CHECK(!erases&&!programs&&!commits);
    for(offset=0;offset<UPDATE_APP_BYTES;offset+=512U){
        for(i=0;i<512U;++i)image_chunk[i]=ImageByte(offset+i);
        if(variant==1U&&offset==0U)image_chunk[0]=1U;
        if(variant==2U&&offset==4096U)image_chunk[99]^=1U;
        Put(command_data,77U);Put(command_data+4,offset);memcpy(command_data+8,image_chunk,512U);
        result=Command(UPDATE_OP_DATA,command_data,520U,UPDATE_OK);if(result)return result;
        if(offset==0U){before=programs;result=Command(UPDATE_OP_DATA,command_data,520U,UPDATE_OK);if(result)return result;CHECK(programs==before);}
    }
    CHECK(service.received==UPDATE_APP_BYTES&&erases==112U&&programs==1792U&&!commits);
    return 0;
}
/* SHAknownvectors와전체448KiB전송,잠금,재전송,commit/reset분리,손상/연결종료/
 * queuefull을검증한다. 실제NOR/FLASH의전원차단시험이아니다. */
static int TestUpdateVersion(uint32_t resident)
{
    static const uint8_t abc[32]={0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad};
    static const uint8_t empty[32]={0xe3,0xb0,0xc4,0x42,0x98,0xfc,0x1c,0x14,0x9a,0xfb,0xf4,0xc8,0x99,0x6f,0xb9,0x24,0x27,0xae,0x41,0xe4,0x64,0x9b,0x93,0x4c,0xa4,0x95,0x99,0x1b,0x78,0x52,0xb8,0x55};
    UpdateSha256 sha;uint8_t digest[32];uint32_t i,variant;int result;size_t n;NDCP_Frame f;
    UpdateSha256_Init(&sha);UpdateSha256_Final(&sha,digest);for(i=0;i<32U;++i)CHECK(digest[i]==empty[i]);
    UpdateSha256_Init(&sha);UpdateSha256_Feed(&sha,(const uint8_t *)"a",1U);UpdateSha256_Feed(&sha,(const uint8_t *)"bc",2U);UpdateSha256_Final(&sha,digest);for(i=0;i<32U;++i)CHECK(digest[i]==abc[i]);
    /* A typed stock target accepts only the pinned version/hash, before any
     * write capability opens. CFW retains the original target0 behavior. */
    Initialize();UpdateService_Authorize(&service,UPDATE_STAGE_ARM);Put(manifest+52,UPDATE_TARGET_STOCK);
    result=Command(UPDATE_OP_BEGIN,manifest,56U,UPDATE_HASH);if(result)return result;CHECK(!enabled&&!erases&&!programs);
    Initialize();UpdateService_Authorize(&service,UPDATE_STAGE_ARM);Put(manifest+52,2U);
    result=Command(UPDATE_OP_BEGIN,manifest,56U,UPDATE_ARGUMENT);if(result)return result;CHECK(!enabled&&!erases);
    Initialize();UpdateService_Authorize(&service,UPDATE_STAGE_ARM);Put(manifest+52,UPDATE_TARGET_STOCK);
    Put(manifest+4,RECOVERY_STOCK_VERSION);memcpy(manifest+16,recovery_stock_sha256,32);
    result=Command(UPDATE_OP_BEGIN,manifest,56U,UPDATE_OK);if(result)return result;
    CHECK(service.target==UPDATE_TARGET_STOCK&&enabled&&!erases&&!programs);
    Initialize();meta[0]=resident;result=Command(UPDATE_OP_BEGIN,manifest,56U,UPDATE_LOCKED);if(result)return result;CHECK(!erases);
    result=Stage(0U);if(result)return result;
    Put(command_data,77U);f=(NDCP_Frame){UPDATE_OP_FINISH,0U,++sequence,4U,command_data};
    CHECK(UpdateService_Handle(&service,&f)==UPDATE_OK);
    for(i=0;i<112U;++i)UpdateService_Process(&service,++now);
    CHECK(service.state==UPDATE_VERIFIED&&service.verified==UPDATE_APP_BYTES&&!enabled&&!commits);
    n=UpdateService_TakeReply(&service,reply,sizeof(reply));CHECK(n==72U&&Get(reply+16)==UPDATE_OK);
    for(i=0;i<32U;++i)CHECK(service.actual_sha[i]==fixture_sha[i]);
    UpdateService_SetConnected(&service,0U);UpdateService_Process(&service,++now);
    CHECK(service.state==UPDATE_VERIFIED&&!service.authorization);
    UpdateService_SetConnected(&service,1U);UpdateService_Process(&service,++now);
    Put(command_data+4,0U);memcpy(command_data+8,fixture_sha,32U);
    result=Command(UPDATE_OP_COMMIT,command_data,40U,UPDATE_ARGUMENT);if(result)return result;CHECK(!commits);
    Put(command_data+4,UPDATE_COMMIT_ARM);result=Command(UPDATE_OP_COMMIT,command_data,40U,UPDATE_LOCKED);if(result)return result;
    UpdateService_Authorize(&service,UPDATE_STAGE_ARM);
    result=Command(UPDATE_OP_COMMIT,command_data,40U,UPDATE_OK);if(result)return result;
    CHECK(commits==1U&&meta[0]==resident&&meta[4]==FIXTURE_CRC&&service.state==UPDATE_COMMITTED&&!resets);
    result=Command(UPDATE_OP_COMMIT,command_data,40U,UPDATE_OK);if(result)return result;CHECK(commits==1U);
    Put(command_data+4,UPDATE_RESET_ARM);result=Command(UPDATE_OP_RESET,command_data,8U,UPDATE_OK);if(result)return result;
    UpdateService_Process(&service,now+5000U);CHECK(!resets);
    UpdateService_NotifyReplyTransmitted(&service,sequence,now);UpdateService_Process(&service,now+1499U);CHECK(!resets);
    result=Command(UPDATE_OP_ABORT,command_data,4U,UPDATE_PENDING);if(result)return result;
    UpdateService_Process(&service,now+5000U);CHECK(resets==1U&&service.state==UPDATE_FAILED&&service.commit_uncertain);
    result=Command(UPDATE_OP_RESET,command_data,8U,UPDATE_STATE);if(result)return result;
    UpdateService_NotifyReplyTransmitted(&service,sequence,now);UpdateService_Process(&service,now+1500U);CHECK(resets==1U);
    /* 잘못된vector와수신후데이터손상은metadata호출전거절된다. */
    for(variant=1U;variant<=2U;++variant){
        Initialize();result=Stage(variant);if(result)return result;Put(command_data,77U);
        f=(NDCP_Frame){UPDATE_OP_FINISH,0U,++sequence,4U,command_data};CHECK(UpdateService_Handle(&service,&f)==UPDATE_OK);
        for(i=0;i<112U;++i)UpdateService_Process(&service,++now);
        CHECK(service.state==UPDATE_FAILED&&!commits&&!enabled&&!service.authorization);
        n=UpdateService_TakeReply(&service,reply,sizeof(reply));CHECK(n==72U);
        CHECK(Get(reply+16)==(variant==1U?UPDATE_VECTOR:UPDATE_HASH));
    }
    Initialize();meta[4]=1U;UpdateService_Authorize(&service,UPDATE_STAGE_ARM);
    result=Command(UPDATE_OP_BEGIN,manifest,56U,UPDATE_PENDING);if(result)return result;CHECK(!erases);
    Initialize();UpdateService_Authorize(&service,UPDATE_STAGE_ARM);result=Command(UPDATE_OP_BEGIN,manifest,56U,UPDATE_OK);if(result)return result;
    fail_erase=1U;Put(command_data,77U);Put(command_data+4,0U);memset(command_data+8,0,16U);
    result=Command(UPDATE_OP_DATA,command_data,24U,UPDATE_IO);if(result)return result;CHECK(service.state==UPDATE_FAILED&&!commits&&!programs);
    result=Command(UPDATE_OP_BEGIN,manifest,56U,UPDATE_LOCKED);if(result)return result;
    Initialize();UpdateService_Authorize(&service,UPDATE_STAGE_ARM);result=Command(UPDATE_OP_BEGIN,manifest,56U,UPDATE_OK);if(result)return result;
    UpdateService_SetConnected(&service,0U);UpdateService_Process(&service,++now);CHECK(!enabled&&!service.authorization&&service.state==UPDATE_FAILED);
    UpdateService_SetConnected(&service,1U);UpdateService_Process(&service,++now);result=Command(UPDATE_OP_BEGIN,manifest,56U,UPDATE_LOCKED);if(result)return result;
    f=(NDCP_Frame){UPDATE_OP_STATUS,0U,++sequence,0U,NULL};CHECK(UpdateService_Handle(&service,&f)==UPDATE_OK);CHECK(UpdateService_Handle(&service,&f)==UPDATE_OK);CHECK(UpdateService_Handle(&service,&f)==UPDATE_BUSY);
    return 0;
}
/* Both resident versions preserve word0 through the full APP transaction. */
int TestUpdate(void){int r=TestUpdateVersion(0x000e0000U);return r?r:TestUpdateVersion(0x000f0000U);}
