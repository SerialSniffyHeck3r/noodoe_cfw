#include "OBD_Service.h"
#include <string.h>

/* ATSP는 protocol을 비휘발성 저장할 수 있어 사용하지 않는다. ATM0로 성공한
 * protocol의 자동 저장도 끈 뒤 ATTP0를 시도한다.0100 뒤 지원 PID만 순환한다. */
static const char *const init_commands[]={"ATI\r","ATE0\r","ATL0\r","ATS0\r","ATH0\r","ATM0\r","ATTP0\r","0100\r"};
static const uint8_t pids[OBD_PID_COUNT]={0x0C,0x0D,0x05,0x0F,0x11,0x04,0x0B,0x10};
#define INIT_COUNT ((uint32_t)(sizeof(init_commands)/sizeof(init_commands[0])))

/* 한 byte의 hex digit을 변환한다. 외부 입력을 부분적으로 숫자로 받아들이지 않는다. */
static int Hex(char c)
{
    if(c>='0' && c<='9') return c-'0';
    if(c>='A' && c<='F') return c-'A'+10;
    return -1;
}

/* 현재 command의 PID를 snapshot index로 찾는다.0100 및 AT 명령은 값 index가 없다. */
static int PidIndex(uint32_t pid)
{
    uint32_t i;
    for(i=0;i<OBD_PID_COUNT;++i) if(pids[i]==pid) return (int)i;
    return -1;
}

/* 실패한 PID의 최신 유효 표시만 내린다. 직전 raw와 값은 진단/표시용으로 보존한다. */
static void InvalidateCurrent(ObdService *s)
{
    int index=PidIndex(s->current_pid);
    if(index>=0) s->latest.valid_fields&=~(1UL<<(uint32_t)index);
}

/* 알려진 positive reply의 raw bytes를 공개 정수 단위로 바꾼다. Mode01 공식을
 * 사용하며 RPM은 정수 반올림 손실 없이 rawAB=RPM*4로 보관한다. */
static void CommitValue(ObdService *s, uint32_t raw, uint32_t now)
{
    int index=PidIndex(s->current_pid);
    int32_t value=(int32_t)raw;
    if(s->current_pid==0U) {
        s->latest.supported_01_20=raw;
        s->latest.support_known=1U;
        return;
    }
    if(index<0) return;
    if(index==OBD_VALUE_COOLANT || index==OBD_VALUE_INTAKE_TEMP) value-=40;
    if(index==OBD_VALUE_THROTTLE || index==OBD_VALUE_LOAD) value=(int32_t)((raw*1000U+127U)/255U);
    if(index==OBD_VALUE_MAF) value=(int32_t)(raw*10U);
    s->latest.raw_values[index]=raw;
    s->latest.values[index]=value;
    s->latest.value_ms[index]=now;
    s->latest.valid_fields|=1UL<<(uint32_t)index;
}

/* echo 판별과 hex parsing을 위해 한 줄의 공백을 제거하고 ASCII 대문자로 바꾼다.
 * 원문은 별도 raw에 그대로 보존한다. 출력 overflow는 거절한다. */
static uint32_t Compact(const char *data, uint32_t length, char *out, uint32_t capacity)
{
    uint32_t i,n=0U;
    for(i=0;i<length;++i) {
        char c=data[i];
        if(c==' ' || c=='\t' || c=='\r' || c=='\n') continue;
        if(c>='a' && c<='z') c=(char)(c-'a'+'A');
        if((uint8_t)c<32U || (uint8_t)c>126U || n+1U>=capacity) return 0U;
        out[n++]=c;
    }
    out[n]='\0';
    return 1U;
}

/* 한 응답 줄은 ATH0에서 기대하는 headers-off hex bytes여야 한다. ECU header,
 * ISO-TP multi-frame 번호 또는 후행 garbage 안에서41을 검색해 성공으로 만들지 않는다. */
static uint32_t HexLine(const char *line, uint8_t *bytes, uint32_t *count)
{
    uint32_t n=(uint32_t)strlen(line),i;
    if(!n || (n&1U) || n>32U) return 0U;
    for(i=0;i<n;i+=2U) {
        int hi=Hex(line[i]),lo=Hex(line[i+1U]);
        if(hi<0 || lo<0) return 0U;
        bytes[i/2U]=(uint8_t)((hi<<4)|lo);
    }
    *count=n/2U;
    return 1U;
}

/* '>'까지 모은 response를 줄 단위로 해석한다. echo/banner/SEARCHING/NO DATA/
 * 오류를 분리하고 요청한 Mode01 PID의 정확한 byte 수만 채택한다. 복수 ECU의
 * 정상 reply가 있으면 첫 값을 쓰되 multiple_replies로 모호함을 남긴다. */
static ObdResult ParseResponse(ObdService *s, uint32_t *raw_out)
{
    uint32_t start=0U,i,had_ok=0U,had_banner=0U,no_data=0U,replies=0U,bad=0U;
    ObdResult error=OBD_RESULT_NONE;
    char echo[OBD_COMMAND_MAX];
    if(!Compact(s->latest.command,(uint32_t)strlen(s->latest.command),echo,sizeof(echo))) return OBD_RESULT_MALFORMED;
    for(i=0U;i<=s->used;++i) {
        char line[128];
        uint8_t bytes[16];
        uint32_t count=0U,j,expected,raw=0U;
        if(i<s->used && s->response[i]!='\r' && s->response[i]!='\n') continue;
        if(i==start) { start=i+1U; continue; }
        if(!Compact(s->response+start,i-start,line,sizeof(line))) { bad=1U; start=i+1U; continue; }
        start=i+1U;
        if(!line[0] || strcmp(line,echo)==0) continue;
        if(strcmp(line,"OK")==0) { had_ok=1U; continue; }
        if(strcmp(line,"NODATA")==0) { no_data=1U; continue; }
        if(strcmp(line,"SEARCHING...")==0 || strcmp(line,"BUSINIT...OK")==0) continue;
        if(strcmp(line,"?")==0 || strstr(line,"ERROR") || strstr(line,"UNABLETOCONNECT") ||
           strstr(line,"STOPPED") || strstr(line,"BUFFERFULL")) { error=OBD_RESULT_ADAPTER_ERROR; continue; }
        if(HexLine(line,bytes,&count)) {
            if(count==3U && bytes[0]==0x7FU && bytes[1]==0x01U) {
                error=OBD_RESULT_NEGATIVE_RESPONSE; continue;
            }
            expected=s->current_pid==0U ? 6U :
                     (s->current_pid==0x0CU || s->current_pid==0x10U ? 4U : 3U);
            if((!s->is_init || s->init_index==INIT_COUNT-1U) && count==expected &&
               bytes[0]==0x41U && bytes[1]==s->current_pid) {
                for(j=2U;j<count;++j) raw=(raw<<8)|bytes[j];
                if(!replies) *raw_out=raw;
                ++replies;
            } else bad=1U;
        } else if(s->is_init && s->init_index==0U) had_banner=1U;
        else bad=1U;
    }
    if(replies>1U) s->latest.multiple_replies+=replies-1U;
    if(error!=OBD_RESULT_NONE) return error;
    /* 정상 줄과 garbage/다른 PID가 섞여도 부분 성공으로 감추지 않는다. */
    if(bad || (no_data && replies)) return OBD_RESULT_MALFORMED;
    if(s->is_init && s->init_index<INIT_COUNT-1U) {
        return (s->init_index==0U ? had_banner || had_ok : had_ok) ? OBD_RESULT_OK : OBD_RESULT_MALFORMED;
    }
    if(replies) return OBD_RESULT_OK;
    if(no_data) return OBD_RESULT_NO_DATA;
    return OBD_RESULT_MALFORMED;
}

/* 결과를 한 번 기록하고 다음 command의 phase를 결정한다. 초기화 AT 실패는
 * reconnect 전까지 FAILED로 멈추며,0100 NO DATA는 지원정보 미확인으로 계속한다. */
static void Finish(ObdService *s, uint32_t now)
{
    uint32_t raw=0U;
    ObdResult result=s->overflow ? OBD_RESULT_OVERFLOW : ParseResponse(s,&raw);
    s->latest.raw_length=s->used;
    memcpy(s->latest.raw,s->response,s->used);
    s->latest.raw[s->used]='\0';
    s->latest.last_result=result;
    s->latest.last_result_ms=now;
    s->latest.last_pid=s->current_pid;
    ++s->latest.completed;
    if(result==OBD_RESULT_OK) {
        if(!s->is_init || s->init_index==INIT_COUNT-1U) CommitValue(s,raw,now);
    } else {
        InvalidateCurrent(s);
        if(result==OBD_RESULT_OVERFLOW) ++s->latest.overflows;
        else if(result==OBD_RESULT_MALFORMED) ++s->latest.malformed;
        else if(result!=OBD_RESULT_NO_DATA) ++s->latest.adapter_errors;
    }
    if(s->is_init) {
        if(result!=OBD_RESULT_OK && !(s->init_index==INIT_COUNT-1U && result==OBD_RESULT_NO_DATA)) s->phase=OBD_FAILED;
        else {
            ++s->init_index;
            s->phase=s->init_index<INIT_COUNT ? OBD_INITIALIZING : OBD_READY;
        }
    } else s->phase=OBD_READY;
    s->ready_ms=now;
    s->used=s->overflow=0U;
}

/* timeout/staleness/poll 주기는 caller 정책이다.0은 각각2000/5000/200ms로 대체한다. */
void ObdService_Init(ObdService *s, uint32_t timeout_ms, uint32_t stale_ms, uint32_t poll_interval_ms)
{
    if(!s) return;
    memset(s,0,sizeof(*s));
    s->timeout_ms=timeout_ms ? timeout_ms : 2000U;
    s->stale_ms=stale_ms ? stale_ms : 5000U;
    s->poll_interval_ms=poll_interval_ms ? poll_interval_ms : 200U;
}

/* 연결 세션 전환 시 pending response와 이전 유효 값/지원정보를 해제한다.
 * raw 진단 누계는 보존하며 새 세션은 adapter ID부터 다시 읽는다. */
void ObdService_SetConnected(ObdService *s, uint32_t connected, uint32_t now)
{
    if(!s) return;
    connected=connected ? 1U : 0U;
    if(s->connected==connected) return;
    s->connected=connected;
    s->phase=connected ? OBD_INITIALIZING : OBD_DISCONNECTED;
    s->init_index=s->poll_index=s->used=s->overflow=0U;
    s->latest.valid_fields=s->latest.support_known=0U;
    s->latest.supported_01_20=0U;
    s->ready_ms=now;
}

/* timeout 후 즉시 다음 command를 보내면 늦은 reply가 새 PID에 섞일 수 있다.
 * DRAINING 상태로 보내고 실제 prompt가 다시 올 때까지 발행을 막는다. */
void ObdService_Poll(ObdService *s, uint32_t now)
{
    if(!s || s->phase!=OBD_WAITING || now-s->started_ms<s->timeout_ms) return;
    ++s->latest.timeouts;
    s->latest.last_result=OBD_RESULT_TIMEOUT;
    s->latest.last_result_ms=now;
    s->latest.last_pid=s->current_pid;
    s->latest.raw_length=s->used;
    memcpy(s->latest.raw,s->response,s->used);
    s->latest.raw[s->used]='\0';
    InvalidateCurrent(s);
    s->phase=OBD_DRAINING;
    s->used=s->overflow=0U;
}

/* command를 실제 transport에 전달할 때 타임아웃 시계를 시작한다. 단 하나만
 * 진행하며 caller buffer 부족/지원 PID 없음/아직 주기 미도달은 상태를 바꾸지 않는다. */
size_t ObdService_TakeCommand(ObdService *s, char *out, size_t capacity, uint32_t now)
{
    const char *command;
    char query[6];
    uint32_t pid=0U,index=0U,checked=0U;
    size_t length;
    if(!s || !out || !s->connected) return 0U;
    ObdService_Poll(s,now);
    if(s->phase!=OBD_INITIALIZING && s->phase!=OBD_READY) return 0U;
    if(s->phase==OBD_INITIALIZING) command=init_commands[s->init_index];
    else {
        static const char hex[]="0123456789ABCDEF";
        if(now-s->ready_ms<s->poll_interval_ms) return 0U;
        index=s->poll_index;
        while(checked<OBD_PID_COUNT) {
            pid=pids[index];
            if(!s->latest.support_known || (s->latest.supported_01_20&(1UL<<(32U-pid)))) break;
            index=(index+1U)%OBD_PID_COUNT; ++checked;
        }
        if(checked==OBD_PID_COUNT) return 0U;
        query[0]='0'; query[1]='1'; query[2]=hex[pid>>4]; query[3]=hex[pid&15U]; query[4]='\r'; query[5]='\0';
        command=query;
    }
    length=strlen(command);
    if(capacity<=length) return 0U;
    memcpy(out,command,length+1U);
    memcpy(s->latest.command,command,length+1U);
    s->is_init=s->phase==OBD_INITIALIZING;
    s->current_pid=pid;
    if(!s->is_init) s->poll_index=(index+1U)%OBD_PID_COUNT;
    s->started_ms=now; s->phase=OBD_WAITING; s->used=s->overflow=0U;
    return length;
}

/* transport 조각을 prompt까지 모은다. overflow는 남은 응답을 버리고 prompt에서
 * 실패로 확정한다. timeout 이후 늦은 응답은 새 값으로 commit하지 않는다. */
void ObdService_Feed(ObdService *s, const uint8_t *data, size_t length, uint32_t now)
{
    size_t i;
    if(!s || !s->connected || (!data && length)) return;
    ObdService_Poll(s,now);
    for(i=0;i<length;++i) {
        uint8_t c=data[i];
        if(c=='>') {
            if(s->phase==OBD_WAITING) Finish(s,now);
            else if(s->phase==OBD_DRAINING) {
                s->phase=s->is_init ? OBD_FAILED : OBD_READY;
                s->ready_ms=now;
            }
            continue;
        }
        if(s->phase!=OBD_WAITING || s->overflow) continue;
        if(s->used>=OBD_RESPONSE_MAX-1U) { s->overflow=1U; continue; }
        s->response[s->used++]=(char)c;
    }
}

/* 현재 연결/phase와 field별 신선도를 복사한다. stale로 invalid가 되어도 raw와
 * 마지막 값은 지우지 않아 UI가 '-'와마지막관측값을구분할 수 있다. */
uint32_t ObdService_GetSnapshot(const ObdService *s, uint32_t now, ObdSnapshot *out)
{
    uint32_t i;
    if(!s || !out) return 0U;
    *out=s->latest;
    out->connected=s->connected; out->phase=s->phase; out->stale_fields=0U;
    for(i=0U;i<OBD_PID_COUNT;++i) {
        uint32_t bit=1UL<<i;
        if((out->valid_fields&bit) && now-out->value_ms[i]>s->stale_ms) {
            out->valid_fields&=~bit; out->stale_fields|=bit;
        }
    }
    if(!s->connected) out->valid_fields=0U;
    return 1U;
}
