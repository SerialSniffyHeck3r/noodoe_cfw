#include "test_common.h"
#include "OBD_Service.h"
static ObdService service;
static ObdSnapshot snapshot;
static char command[OBD_COMMAND_MAX];
static uint32_t now;

/* adapter stream의 문자열을 실제 서비스에 전달한다. 시각은 테스트가 명시한다. */
static void Feed(const char *text)
{ ObdService_Feed(&service,(const uint8_t *)text,strlen(text),now); }

/* 여덟 초기 command를 public API로 진행한다. 어느 명령도 ECU 변경 명령이
 * 아닌지 정확한 byte 문자열로 검사하고 echo/fragmented prompt도 끼워 넣는다. */
static int Initialize(const char *support)
{
    static const char *const commands[]={"ATI\r","ATE0\r","ATL0\r","ATS0\r","ATH0\r","ATM0\r","ATTP0\r","0100\r"};
    uint32_t i;
    ObdService_Init(&service,1000U,5000U,10U);
    ObdService_SetConnected(&service,1U,now);
    for(i=0;i<8U;++i) {
        CHECK(ObdService_TakeCommand(&service,command,1U,now)==0U);
        CHECK(service.phase==OBD_INITIALIZING);
        CHECK(ObdService_TakeCommand(&service,command,sizeof(command),now)==strlen(commands[i]));
        CHECK(strcmp(command,commands[i])==0);
        CHECK(ObdService_TakeCommand(&service,command,sizeof(command),now)==0U);
        Feed(commands[i]);Feed("\r");
        if(i==0U) {Feed("ELM327 v1.5\r");Feed(">");}
        else if(i==7U) Feed(support);
        else {Feed("O");Feed("K\r");Feed(">");}
        CHECK(service.latest.last_result==(i==7U && strcmp(support,"NO DATA\r>")==0 ? OBD_RESULT_NO_DATA : OBD_RESULT_OK));
        now+=20U;
    }
    CHECK(service.phase==OBD_READY);
    return 0;
}

/* poll 주기를 지나 요청한 PID의 query가 생성되는지 검증하고 응답을 넣는다. */
static int Query(const char *expected,const char *reply)
{
    now+=20U;
    CHECK(ObdService_TakeCommand(&service,command,sizeof(command),now)==5U);
    CHECK(strcmp(command,expected)==0);
    Feed(reply);
    return 0;
}

/* 정상 PID 단위/지원 filtering과 one-outstanding/timeouts/late prompt를
 * 검증한다. 실패 뒤 다음 정상 command가 복구되는지까지 포함한다. */
int TestObd(void)
{
    uint32_t i; int result; char huge[600];
    static const char *const queries[]={"010C\r","010D\r","0105\r","010F\r","0111\r","0104\r","010B\r","0110\r"};
    static const char *const replies[]={"01 0C\r41 0c 1a f8\r>","410D64\r>","41055A\r>","410F1E\r>","411180\r>","4104FF\r>","410B64\r>","411001F4\r>"};
    static const int32_t expected[]={6904,100,50,-10,502,1000,100,5000};
    now=100U; result=Initialize("SEARCHING...\r4100FFFFFFFF\r>"); if(result) return result;
    CHECK(service.latest.support_known && service.latest.supported_01_20==UINT32_MAX);
    for(i=0;i<8U;++i) {
        result=Query(queries[i],replies[i]); if(result) return result;
        CHECK(service.latest.last_result==OBD_RESULT_OK);
        CHECK(service.latest.values[i]==expected[i]);
        CHECK(service.latest.value_ms[i]==now);
    }
    CHECK(ObdService_GetSnapshot(&service,now,&snapshot));CHECK(snapshot.valid_fields==255U);
    CHECK(ObdService_GetSnapshot(&service,now+5001U,&snapshot));
    CHECK(!snapshot.valid_fields && snapshot.stale_fields==255U && snapshot.values[0]==6904);
    result=Query("010C\r","NO DATA\r>"); if(result) return result;
    CHECK(service.latest.last_result==OBD_RESULT_NO_DATA && !(service.latest.valid_fields&1U));
    result=Query("010D\r","7F0112\r>"); if(result) return result;
    CHECK(service.latest.last_result==OBD_RESULT_NEGATIVE_RESPONSE);
    result=Query("0105\r","UNABLE TO CONNECT\r>"); if(result) return result;
    CHECK(service.latest.last_result==OBD_RESULT_ADAPTER_ERROR);
    result=Query("010F\r","410F\r>"); if(result) return result;
    CHECK(service.latest.last_result==OBD_RESULT_MALFORMED);
    result=Query("0111\r","7E803411180\r>"); if(result) return result;
    CHECK(service.latest.last_result==OBD_RESULT_MALFORMED);
    result=Query("0104\r","4104FF\rgarbage\r>"); if(result) return result;
    CHECK(service.latest.last_result==OBD_RESULT_MALFORMED);
    result=Query("010B\r","410D20\r>"); if(result) return result;
    CHECK(service.latest.last_result==OBD_RESULT_MALFORMED);
    result=Query("0110\r","411001F4\r411003E8\r>"); if(result) return result;
    CHECK(service.latest.last_result==OBD_RESULT_OK && service.latest.values[7]==5000);
    CHECK(service.latest.multiple_replies==1U);
    /* 임의 fragment 분리로 네 byte positive reply를 전달한다. */
    now+=20U;CHECK(ObdService_TakeCommand(&service,command,sizeof(command),now)==5U);
    Feed("4");Feed("10");Feed("C1AF");CHECK(service.phase==OBD_WAITING);Feed("8\r");Feed(">");
    CHECK(service.latest.values[0]==6904 && service.latest.last_result==OBD_RESULT_OK);
    now+=20U;CHECK(ObdService_TakeCommand(&service,command,sizeof(command),now)==5U);
    memset(huge,'A',sizeof(huge));
    ObdService_Feed(&service,(const uint8_t *)huge,sizeof(huge),now);
    CHECK(service.phase==OBD_WAITING && service.overflow);
    Feed(">"); CHECK(service.latest.last_result==OBD_RESULT_OVERFLOW && service.latest.raw_length==511U);
    /* timeout 후 다음 PID를 발행하지 않는다. 늦은 성공처럼 보이는 bytes도 버린다. */
    now+=20U;CHECK(ObdService_TakeCommand(&service,command,sizeof(command),now)==5U);
    CHECK(strcmp(command,"0105\r")==0);Feed("4105");
    now+=1000U;ObdService_Poll(&service,now);
    CHECK(service.phase==OBD_DRAINING && service.latest.last_result==OBD_RESULT_TIMEOUT);
    CHECK(ObdService_TakeCommand(&service,command,sizeof(command),now)==0U);
    Feed("FF\r>");CHECK(service.phase==OBD_READY);
    CHECK(!(service.latest.valid_fields&(1U<<OBD_VALUE_COOLANT)));
    CHECK(ObdService_TakeCommand(&service,command,sizeof(command),now)==0U);
    result=Query("010F\r","410F50\r>"); if(result) return result;
    CHECK(service.latest.values[3]==40);
    /* 지원 bitmap이 RPM과 speed만 허용하면 나머지 여섯 PID는 생략한다. */
    result=Initialize("410000180000\r>"); if(result) return result;
    result=Query("010C\r","410C0004\r>"); if(result) return result;
    result=Query("010D\r","410D01\r>"); if(result) return result;
    result=Query("010C\r","410C0008\r>"); if(result) return result;
    result=Initialize("410000000000\r>"); if(result) return result;
    now+=20U;CHECK(ObdService_TakeCommand(&service,command,sizeof(command),now)==0U);
    result=Initialize("NO DATA\r>"); if(result) return result;
    CHECK(!service.latest.support_known);
    result=Query("010C\r","410C0004\r>"); if(result) return result;
    ObdService_SetConnected(&service,0U,now);
    CHECK(ObdService_GetSnapshot(&service,now,&snapshot));CHECK(!snapshot.connected && !snapshot.valid_fields);
    /* 초기 AT 실패는 reconnect까지 정지한다. */
    ObdService_SetConnected(&service,1U,now);
    CHECK(ObdService_TakeCommand(&service,command,sizeof(command),now)==4U);Feed("?\r>");
    CHECK(service.phase==OBD_FAILED);
    now+=5000U;CHECK(ObdService_TakeCommand(&service,command,sizeof(command),now)==0U);
    ObdService_SetConnected(&service,0U,now);ObdService_SetConnected(&service,1U,now);
    CHECK(ObdService_TakeCommand(&service,command,sizeof(command),now)==4U);
    now+=1000U;Feed("ELM327\r>"); CHECK(service.phase==OBD_FAILED && service.latest.last_result==OBD_RESULT_TIMEOUT);
    /* 32-bit tick wrap 동안도 deadline 계산은 unsigned elapsed를 사용한다. */
    ObdService_Init(&service,50U,50U,1U);now=UINT32_MAX-20U;
    ObdService_SetConnected(&service,1U,now);
    CHECK(ObdService_TakeCommand(&service,command,sizeof(command),now)==4U);
    ObdService_Poll(&service,10U);CHECK(service.phase==OBD_WAITING);
    ObdService_Poll(&service,29U);CHECK(service.phase==OBD_DRAINING);
    CHECK(!ObdService_GetSnapshot(NULL,0U,&snapshot));
    return 0;
}
