#include "GNSS_Service.h"
#include <string.h>

/* ASCII hex 한 자리만 변환한다. checksum 이외의 문자를 숫자로 받아들이지 않는다. */
static int Hex(char c)
{
    if(c>='0' && c<='9') return c-'0';
    if(c>='A' && c<='F') return c-'A'+10;
    if(c>='a' && c<='f') return c-'a'+10;
    return -1;
}

/* 부호 없는 정수 필드용이다. 빈 값/소수/뒤따르는 문자를 거절하고 max 범위 안에서
 * 자리마다 검사하므로 overflow 후 범위 검사를 하는 방식이 아니다. */
static uint32_t UInt(const char *p, uint32_t max, uint32_t *out)
{
    uint32_t value=0U;
    if(!p || !*p) return 0U;
    while(*p) {
        uint32_t digit;
        if(*p<'0' || *p>'9') return 0U;
        digit=(uint32_t)(*p++-'0');
        if(value>max/10U || (value==max/10U && digit>max%10U)) return 0U;
        value=value*10U+digit;
    }
    *out=value;
    return 1U;
}

/* 정수 scale(10의 거듭제곱)로 decimal을 읽는다. 지수/공백/부분 문자열은
 * 허용하지 않는다. scale보다 세밀한 자리는 숫자인지 검사하고 절삭한다.
 * float/sscanf/동적 할당 없이 signed altitude와 unsigned 항법 값을 함께 처리한다. */
static uint32_t Decimal(const char *p, uint32_t scale, int64_t max_abs,
                        uint32_t signed_value, int64_t *out)
{
    uint64_t whole=0U,fraction=0U;
    uint32_t negative=0U,digits=0U,step=scale;
    if(!p || !*p) return 0U;
    if(*p=='-' || *p=='+') {
        if(!signed_value) return 0U;
        negative=*p=='-'; ++p;
    }
    while(*p>='0' && *p<='9') {
        uint32_t digit=(uint32_t)(*p++-'0');
        if(whole>(uint64_t)max_abs/scale/10U+1U) return 0U;
        whole=whole*10U+digit;
        if(whole>(uint64_t)max_abs/scale) return 0U;
        ++digits;
    }
    if(!digits) return 0U;
    if(*p=='.') {
        ++p; digits=0U;
        while(*p>='0' && *p<='9') {
            if(step>1U) { step/=10U; fraction+=(uint32_t)(*p-'0')*step; }
            ++p; ++digits;
        }
        if(!digits) return 0U;
    }
    if(*p || whole*scale+fraction>(uint64_t)max_abs) return 0U;
    *out=(int64_t)(whole*scale+fraction);
    if(negative) *out=-*out;
    return 1U;
}

/* ddmm.mmm/dddmm.mmm와 N/S/E/W를 degree*1e7로 변환한다.60분 이상,
 * 극점/날짜변경선 너머와 잘못된 hemisphere를 거절한다. */
static uint32_t Coordinate(const char *number, const char *hemisphere,
                           uint32_t latitude, int32_t *out)
{
    int64_t n;
    uint32_t degrees,minutes,digits=0U,max=latitude?90U:180U;
    int32_t result;
    char h;
    if(!hemisphere || !hemisphere[0] || hemisphere[1]) return 0U;
    h=hemisphere[0];
    if(latitude ? (h!='N' && h!='S') : (h!='E' && h!='W')) return 0U;
    if(!number) return 0U;
    while(number[digits]>='0' && number[digits]<='9') ++digits;
    /* 선행 0도 형식의 일부다. 잘린 좌표를 다른 위치로 해석하지 않는다. */
    if(digits!=(latitude?4U:5U)) return 0U;
    if(!Decimal(number,1000000U,(int64_t)max*100000000LL,0U,&n)) return 0U;
    degrees=(uint32_t)(n/100000000LL);
    minutes=(uint32_t)(n%100000000LL);
    if(minutes>=60000000U || (degrees==max && minutes!=0U)) return 0U;
    result=(int32_t)(degrees*10000000U+(uint32_t)(((uint64_t)minutes*10U+30U)/60U));
    *out=(h=='S' || h=='W') ? -result : result;
    return 1U;
}

/* HHMMSS.sss는 UTC 자정 이후 ms로 반환한다. 이 구현은23:59:60 leap-second
 * 표기를 시간 유효 값으로 받지 않으며 위치 등 독립 필드까지 버리지는 않는다. */
static uint32_t Time(const char *p, uint32_t *out)
{
    int64_t value;
    uint32_t h,m,s,integer,digits=0U;
    if(!p) return 0U;
    while(p[digits]>='0' && p[digits]<='9') ++digits;
    if(digits!=6U) return 0U;
    if(!Decimal(p,1000U,235959999LL,0U,&value)) return 0U;
    integer=(uint32_t)(value/1000LL);
    h=integer/10000U; m=(integer/100U)%100U; s=integer%100U;
    if(h>23U || m>59U || s>59U) return 0U;
    *out=((h*60U+m)*60U+s)*1000U+(uint32_t)(value%1000LL);
    return 1U;
}

/* Gregorian 날짜 범위/윤년을 검사한다. 전화 입력과 RMC date가 같은 검사를 쓴다. */
static uint32_t CalendarDate(uint32_t date)
{
    static const uint8_t days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    uint32_t y=date/10000U,m=(date/100U)%100U,d=date%100U,max;
    if(y<1980U || y>2079U || m<1U || m>12U) return 0U;
    max=days[m-1U];
    if(m==2U && (y%4U)==0U && ((y%100U)!=0U || (y%400U)==0U)) ++max;
    return d>=1U && d<=max;
}

/* DDMMYY의 century는80..99→1900년대,00..79→2000년대로 명시한다. */
static uint32_t Date(const char *p, uint32_t *out)
{
    uint32_t value,y,date;
    if(strlen(p)!=6U || !UInt(p,999999U,&value)) return 0U;
    y=value%100U; y+=y>=80U ? 1900U : 2000U;
    date=y*10000U+((value/100U)%100U)*100U+value/10000U;
    if(!CalendarDate(date)) return 0U;
    *out=date;
    return 1U;
}

/* 해당 필드만 현재 sentence 시각으로 새로 만든다. 다른 문장 종류의 altitude/
 * motion 정보는 각자의 시각을 유지하므로 새 GGA가 오래된 RMC speed를 되살리지 않는다. */
static void Mark(GnssFix *fix, uint32_t bit, uint32_t now)
{
    uint32_t index=0U;
    fix->fields|=bit;
    while((1UL<<index)!=bit) ++index;
    fix->field_ms[index]=now;
}

/* RMC 유효성 A/V와 optional mode N을 해석한다. no-fix도 정상 수신 문장이며
 * 기존 position/speed/course를 즉시 invalid로 만들고 raw를 남긴다. */
static uint32_t Rmc(GnssFix *v, char **f, uint32_t count, uint32_t now)
{
    int32_t lat=0,lon=0;
    int64_t n;
    uint32_t value,valid;
    if(count<10U || strlen(f[2])!=1U || (f[2][0]!='A' && f[2][0]!='V')) return 0U;
    valid=f[2][0]=='A' && !(count>12U && f[12][0]=='N');
    if(valid && (!Coordinate(f[3],f[4],1U,&lat) || !Coordinate(f[5],f[6],0U,&lon))) return 0U;
    v->fields&=~(GNSS_VALID_POSITION|GNSS_VALID_SPEED|GNSS_VALID_COURSE|GNSS_VALID_TIME|GNSS_VALID_DATE);
    if(Time(f[1],&value)) { v->utc_ms=value; Mark(v,GNSS_VALID_TIME,now); }
    if(Date(f[9],&value)) { v->date_yyyymmdd=value; Mark(v,GNSS_VALID_DATE,now); }
    if(!valid) { v->quality=0U; v->fields&=~GNSS_VALID_ALTITUDE; return 1U; }
    v->latitude_e7=lat; v->longitude_e7=lon;
    /* RMC의A로GGA quality=1을만들어내지않는다. position validity는별도bit다. */
    Mark(v,GNSS_VALID_POSITION,now);
    if(Decimal(f[7],1000U,2000000LL,0U,&n)) {
        /* knot*1000→mm/s: 1 knot=1852 m/3600 s이며 최종 정수 단위에서 반올림한다. */
        v->speed_mm_s=(uint32_t)(((uint64_t)n*1852000U+1800000U)/3600000U);
        Mark(v,GNSS_VALID_SPEED,now);
    }
    if(Decimal(f[8],1000U,359999LL,0U,&n)) { v->course_mdeg=(uint32_t)n; Mark(v,GNSS_VALID_COURSE,now); }
    return 1U;
}

/* GGA quality=0이면 위성 위치/고도 유효성을 제거한다. quality1..8은 종류를
 * raw로 남기며 caller가 RTK/추측항법 정확도 정책을 별도로 적용할 수 있다. */
static uint32_t Gga(GnssFix *v, char **f, uint32_t count, uint32_t now)
{
    uint32_t quality,value;
    int32_t lat=0,lon=0;
    int64_t n;
    if(count<11U || !UInt(f[6],8U,&quality)) return 0U;
    if(quality && (!Coordinate(f[2],f[3],1U,&lat) || !Coordinate(f[4],f[5],0U,&lon))) return 0U;
    v->quality=quality;
    v->fields&=~(GNSS_VALID_POSITION|GNSS_VALID_TIME|GNSS_VALID_ALTITUDE|GNSS_VALID_SATELLITES|GNSS_VALID_HDOP);
    if(Time(f[1],&value)) { v->utc_ms=value; Mark(v,GNSS_VALID_TIME,now); }
    if(UInt(f[7],255U,&value)) { v->satellites=value; Mark(v,GNSS_VALID_SATELLITES,now); }
    if(Decimal(f[8],1000U,9999999LL,0U,&n)) { v->hdop_milli=(uint32_t)n; Mark(v,GNSS_VALID_HDOP,now); }
    if(!quality) { v->fields&=~(GNSS_VALID_SPEED|GNSS_VALID_COURSE); return 1U; }
    v->latitude_e7=lat; v->longitude_e7=lon;
    Mark(v,GNSS_VALID_POSITION,now);
    if(f[10][0]=='M' && f[10][1]=='\0' && Decimal(f[9],1000U,100000000LL,1U,&n)) {
        v->altitude_mm=(int32_t)n; Mark(v,GNSS_VALID_ALTITUDE,now);
    }
    return 1U;
}

/* 완료된 ASCII sentence를 검증한 뒤 임시 fix에 파싱하고 성공 때만 commit한다.
 * checksum 오류/잘린 형식이 이전 정상 fix를 덮어쓰지 않는다. */
static void Sentence(GnssService *s, uint32_t now)
{
    char work[GNSS_SENTENCE_MAX],*field[20];
    uint32_t i,star=0U,count=1U,okay;
    uint8_t checksum=0U;
    GnssFix next;
    if(s->used<9U) { ++s->format_errors; return; }
    for(i=1U;i<s->used;++i) if(s->sentence[i]=='*') { star=i; break; }
    if(!star || star+3U!=s->used || Hex(s->sentence[star+1U])<0 || Hex(s->sentence[star+2U])<0) {
        ++s->format_errors; return;
    }
    for(i=1U;i<star;++i) checksum^=(uint8_t)s->sentence[i];
    if(checksum!=(uint8_t)((Hex(s->sentence[star+1U])<<4)|Hex(s->sentence[star+2U]))) {
        ++s->checksum_errors; return;
    }
    memcpy(work,s->sentence,star); work[star]='\0';
    field[0]=work+1;
    for(i=1U;i<star;++i) if(work[i]==',') {
        if(count==20U) { ++s->format_errors; return; }
        work[i]='\0'; field[count++]=work+i+1U;
    }
    if(strlen(field[0])!=5U || field[0][0]!='G' || (field[0][1]!='P' && field[0][1]!='N')) {
        ++s->unsupported_sentences; return;
    }
    next=s->external;
    if(strcmp(field[0]+2,"RMC")==0) okay=Rmc(&next,field,count,now);
    else if(strcmp(field[0]+2,"GGA")==0) okay=Gga(&next,field,count,now);
    else { ++s->unsupported_sentences; return; }
    if(!okay) { ++s->format_errors; return; }
    next.sample_ms=now; next.has_sample=1U;
    next.raw_length=s->used;
    memcpy(next.raw,s->sentence,s->used);
    next.raw[s->used]='\0';
    s->external=next;
    ++s->sentences_ok;
}

/* stale_ms=0이면3000ms를 사용한다. 구조체 전체가 caller 소유이고 할당은 없다. */
void GnssService_Init(GnssService *s, uint32_t stale_ms)
{
    if(!s) return;
    memset(s,0,sizeof(*s));
    s->stale_ms=stale_ms ? stale_ms : 3000U;
}

/* 실제 SPP 연결 이벤트에서만 호출한다. 같은 상태를 반복 보고하면 현재 fix를
 * 지우지 않는다. 전환할 때 외부 세션의 raw/부분문장/validity만 초기화한다. */
void GnssService_SetExternalConnected(GnssService *s, uint32_t connected)
{
    if(!s) return;
    connected=connected ? 1U : 0U;
    if(s->external_connected==connected) return;
    s->external_connected=connected;
    ++s->session;
    memset(&s->external,0,sizeof(s->external));
    s->collecting=s->used=0U;
}

/* '$'는 중간 문장의 재시작점이다. CR/LF로 종료하고 최대159문자, printable
 * ASCII만 받는다. 연결되지 않은 외부 입력은 phone을 덮어쓰지 않고 무시한다. */
void GnssService_FeedExternal(GnssService *s, const uint8_t *data, size_t length, uint32_t now)
{
    size_t i;
    if(!s || !s->external_connected || (!data && length)) return;
    for(i=0;i<length;++i) {
        uint8_t c=data[i];
        if(c=='$') { s->collecting=1U; s->used=0U; }
        if(!s->collecting) continue;
        if(c=='\r' || c=='\n') {
            s->sentence[s->used]='\0';
            Sentence(s,now); s->used=0U; s->collecting=0U; continue;
        }
        if(c<32U || c>126U) { ++s->format_errors; s->collecting=s->used=0U; continue; }
        if(s->used>=GNSS_SENTENCE_MAX-1U) { ++s->overflows; s->collecting=s->used=0U; continue; }
        s->sentence[s->used++]=(char)c;
    }
}

/* 전화 프로토콜에서 해석한 sample을 저장한다. 외부가 연결돼도 phone sample은
 * 보관하되 선택하지 않는다. 범위를 벗어난 유효 필드는 전체 update를 거절한다. */
uint32_t GnssService_UpdatePhone(GnssService *s, const GnssFix *fix, uint32_t now)
{
    uint32_t i;
    if(!s || !fix || (fix->fields&~255U)) return 0U;
    if((fix->fields&GNSS_VALID_POSITION) && (fix->latitude_e7<-900000000 || fix->latitude_e7>900000000 ||
        fix->longitude_e7<-1800000000 || fix->longitude_e7>1800000000)) return 0U;
    if((fix->fields&GNSS_VALID_TIME) && fix->utc_ms>=86400000U) return 0U;
    if((fix->fields&GNSS_VALID_DATE) && !CalendarDate(fix->date_yyyymmdd)) return 0U;
    if((fix->fields&GNSS_VALID_COURSE) && fix->course_mdeg>=360000U) return 0U;
    s->phone=*fix;
    s->phone.sample_ms=now; s->phone.has_sample=1U;
    for(i=0;i<8U;++i) s->phone.field_ms[i]=now;
    if(s->phone.raw_length>=GNSS_SENTENCE_MAX) s->phone.raw_length=GNSS_SENTENCE_MAX-1U;
    s->phone.raw[s->phone.raw_length]='\0';
    return 1U;
}

/* 선택 기준은 연결 상태 하나뿐이다. external no-fix/stale도 source=EXTERNAL로
 * 반환한다. 각 필드 timestamp로 오래된 것만 invalid 처리하고 원래 raw/값은 보존한다. */
uint32_t GnssService_GetSnapshot(const GnssService *s, uint32_t now, GnssSnapshot *out)
{
    uint32_t i;
    if(!s || !out) return 0U;
    out->external_connected=s->external_connected;
    out->source=s->external_connected ? GNSS_SOURCE_EXTERNAL : GNSS_SOURCE_PHONE;
    out->fix=s->external_connected ? s->external : s->phone;
    out->age_ms=out->fix.has_sample ? now-out->fix.sample_ms : UINT32_MAX;
    out->stale=!out->fix.has_sample || out->age_ms>s->stale_ms;
    for(i=0;i<8U;++i) if(now-out->fix.field_ms[i]>s->stale_ms) out->fix.fields&=~(1UL<<i);
    out->valid=(out->fix.fields&GNSS_VALID_POSITION)!=0U;
    return 1U;
}
