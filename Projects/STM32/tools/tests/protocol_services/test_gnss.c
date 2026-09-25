#include "test_common.h"
#include "GNSS_Service.h"
static GnssService service;
static GnssSnapshot snapshot;
static GnssFix phone;
static char sentence[220];

/* 데이터 열만 받아 checksum을 붙인다. 표준 예시의 고정 checksum도 별도로 시험한다. */
static size_t Nmea(const char *body,char *out)
{
    static const char hex[]="0123456789ABCDEF";
    size_t i,n=strlen(body); uint8_t sum=0;
    out[0]='$';
    for(i=0;i<n;++i) {out[i+1U]=body[i];sum^=(uint8_t)body[i];}
    out[n+1U]='*';out[n+2U]=hex[sum>>4];out[n+3U]=hex[sum&15U];
    out[n+4U]='\r';out[n+5U]='\n';out[n+6U]='\0';return n+6U;
}
/* checksum 있는 한 sentence를 연결된 외부 stream에 전달한다. */
static void Feed(const char *body,uint32_t now)
{ size_t n=Nmea(body,sentence); GnssService_FeedExternal(&service,(const uint8_t *)sentence,n,now); }

/* 선택 정책, fixed-point 단위, 구조/체크섬 실패, field별 staleness를 함께 검증한다. */
int TestGnss(void)
{
    static const char rmc[]="$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A\r\n";
    static const char gga[]="$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    uint32_t split,okay,before; size_t n;
    memset(&phone,0,sizeof(phone));
    phone.fields=GNSS_VALID_POSITION|GNSS_VALID_SPEED;
    phone.latitude_e7=370000000;phone.longitude_e7=1270000000;phone.speed_mm_s=10000;
    GnssService_Init(&service,3000U);
    CHECK(GnssService_UpdatePhone(&service,&phone,100U));
    CHECK(GnssService_GetSnapshot(&service,100U,&snapshot));
    CHECK(snapshot.source==GNSS_SOURCE_PHONE && snapshot.valid);
    GnssService_SetExternalConnected(&service,1U);
    CHECK(GnssService_GetSnapshot(&service,100U,&snapshot));
    CHECK(snapshot.source==GNSS_SOURCE_EXTERNAL && !snapshot.valid && snapshot.stale);
    for(split=0;split<=strlen(rmc);++split) {
        GnssService_Init(&service,3000U);GnssService_SetExternalConnected(&service,1U);
        GnssService_FeedExternal(&service,(const uint8_t *)rmc,split,100U);
        GnssService_FeedExternal(&service,(const uint8_t *)rmc+split,strlen(rmc)-split,101U);
        CHECK(service.sentences_ok==1U);
        CHECK(GnssService_GetSnapshot(&service,102U,&snapshot));
        CHECK(snapshot.valid && snapshot.fix.latitude_e7==481173000 && snapshot.fix.longitude_e7==115166667);
        CHECK(snapshot.fix.quality==0U);
        CHECK(snapshot.fix.speed_mm_s==11524U && snapshot.fix.course_mdeg==84400U);
        CHECK(snapshot.fix.date_yyyymmdd==19940323U && snapshot.fix.utc_ms==45319000U);
    }
    GnssService_FeedExternal(&service,(const uint8_t *)gga,strlen(gga),200U);
    CHECK(service.external.altitude_mm==545400 && service.external.satellites==8U);
    CHECK(service.external.hdop_milli==900U && service.external.quality==1U);
    CHECK(service.external.speed_mm_s==11524U && service.external.field_ms[3]<=101U);
    /* 새 GGA의 position만 새로워지고 오래된 RMC speed는 invalid가 된다. */
    GnssService_FeedExternal(&service,(const uint8_t *)gga,strlen(gga),3102U);
    CHECK(GnssService_GetSnapshot(&service,3103U,&snapshot));
    CHECK(snapshot.valid && !(snapshot.fix.fields&GNSS_VALID_SPEED));
    CHECK(snapshot.fix.fields&GNSS_VALID_ALTITUDE);
    Feed("GNRMC,010203.456,A,3351.900,S,15112.000,W,1.0,359.999,290224,,,A",3200U);
    CHECK(service.external.latitude_e7==-338650000 && service.external.longitude_e7==-1512000000);
    CHECK(service.external.speed_mm_s==514U && service.external.utc_ms==3723456U);
    CHECK(service.external.date_yyyymmdd==20240229U);
    Feed("GNGGA,010204.0,3351.900,S,15112.000,W,1,12,1.2,-12.345,M,0,M,,",3201U);
    CHECK(service.external.altitude_mm==-12345);
    Feed("GNRMC,010205.0,V,,,,,,,290224,,,N",3202U);
    CHECK(!(service.external.fields&(GNSS_VALID_POSITION|GNSS_VALID_SPEED|GNSS_VALID_COURSE|GNSS_VALID_ALTITUDE)));
    CHECK(GnssService_UpdatePhone(&service,&phone,3202U));
    CHECK(GnssService_GetSnapshot(&service,3202U,&snapshot));
    CHECK(snapshot.source==GNSS_SOURCE_EXTERNAL && !snapshot.valid);
    GnssService_SetExternalConnected(&service,0U);
    CHECK(GnssService_GetSnapshot(&service,3203U,&snapshot));
    CHECK(snapshot.source==GNSS_SOURCE_PHONE && snapshot.valid);
    GnssService_SetExternalConnected(&service,1U);
    CHECK(!service.external.has_sample && !service.used);
    Feed("GNRMC,010203,A,9000.000,N,18000.000,E,0,0,010126,,,A",3300U);
    CHECK(service.external.latitude_e7==900000000 && service.external.longitude_e7==1800000000);
    okay=service.sentences_ok;before=service.format_errors;
    Feed("GNRMC,010203,A,9000.001,N,18000.000,E,0,0,010126,,,A",3301U);
    Feed("GNRMC,010203,A,1260.000,N,01200.000,E,0,0,010126,,,A",3302U);
    Feed("GNRMC,010203,A,1,N,01200.000,E,0,0,010126,,,A",3303U);
    Feed("GNRMC,010203,A,1200.000,E,01200.000,E,0,0,010126,,,A",3304U);
    CHECK(service.sentences_ok==okay && service.format_errors==before+4U);
    n=Nmea("GNRMC,010203,A,1200.000,N,01200.000,E,0,0,010126,,,A",sentence);
    sentence[n-4U]=sentence[n-4U]=='0'?'1':'0';
    GnssService_FeedExternal(&service,(const uint8_t *)sentence,n,3305U);
    CHECK(service.checksum_errors==1U && service.sentences_ok==okay);
    /* 유효 위치가 있어도 잘린 시간/불가능 날짜는 해당 field만 invalid다. */
    Feed("GNRMC,1,A,1200.000,N,01200.000,E,0,0,290223,,,A",3400U);
    CHECK(service.external.fields&GNSS_VALID_POSITION);
    CHECK(!(service.external.fields&(GNSS_VALID_TIME|GNSS_VALID_DATE)));
    Feed("GNGGA,120000,,,,,0,00,99.9,,M,,M,,",3401U);
    CHECK(!(service.external.fields&(GNSS_VALID_POSITION|GNSS_VALID_SPEED|GNSS_VALID_COURSE|GNSS_VALID_ALTITUDE)));
    Feed("GLRMC,010203,A,1200.000,N,01200.000,E,0,0,010126,,,A",3402U);
    CHECK(service.unsupported_sentences==1U);
    /* 긴 입력/중간 $/손상 byte 이후에도 다음 정상 sentence가 복구된다. */
    memset(sentence,'A',sizeof(sentence));sentence[0]='$';
    GnssService_FeedExternal(&service,(const uint8_t *)sentence,sizeof(sentence),3500U);
    CHECK(service.overflows==1U);
    GnssService_FeedExternal(&service,(const uint8_t *)"$GPRMC,bad",10U,3501U);
    GnssService_FeedExternal(&service,(const uint8_t *)rmc,strlen(rmc),3502U);
    CHECK(service.external.latitude_e7==481173000);
    GnssService_SetExternalConnected(&service,1U); CHECK(service.external.has_sample);
    CHECK(GnssService_GetSnapshot(&service,6503U,&snapshot));
    CHECK(snapshot.source==GNSS_SOURCE_EXTERNAL && snapshot.stale && !snapshot.valid);
    GnssService_Init(&service,50U); GnssService_SetExternalConnected(&service,1U);
    GnssService_FeedExternal(&service,(const uint8_t *)rmc,strlen(rmc),UINT32_MAX-20U);
    CHECK(GnssService_GetSnapshot(&service,10U,&snapshot));CHECK(snapshot.valid && snapshot.age_ms==31U);
    CHECK(GnssService_GetSnapshot(&service,31U,&snapshot));CHECK(!snapshot.valid && snapshot.stale);
    phone.latitude_e7=900000001;CHECK(!GnssService_UpdatePhone(&service,&phone,0U));
    CHECK(!GnssService_GetSnapshot(NULL,0U,&snapshot));
    return 0;
}
