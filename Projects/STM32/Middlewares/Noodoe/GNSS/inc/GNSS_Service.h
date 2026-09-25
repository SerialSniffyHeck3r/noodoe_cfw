#ifndef GNSS_SERVICE_H
#define GNSS_SERVICE_H
#include <stdint.h>
#include <stddef.h>

#define GNSS_SENTENCE_MAX 160U
#define GNSS_VALID_POSITION (1U<<0)
#define GNSS_VALID_TIME (1U<<1)
#define GNSS_VALID_DATE (1U<<2)
#define GNSS_VALID_SPEED (1U<<3)
#define GNSS_VALID_COURSE (1U<<4)
#define GNSS_VALID_ALTITUDE (1U<<5)
#define GNSS_VALID_SATELLITES (1U<<6)
#define GNSS_VALID_HDOP (1U<<7)
typedef enum { GNSS_SOURCE_PHONE=1, GNSS_SOURCE_EXTERNAL=2 } GnssSource;

/* 정수단위:lat/lon=degree*1e7,speed=mm/s,course=degree*1000,altitude=mm,
 * HDOP=ratio*1000,utc_ms=UTC자정후ms,date=YYYYMMDD. raw는마지막정상NMEA다. */
typedef struct {
    uint32_t fields, sample_ms, field_ms[8], has_sample;
    int32_t latitude_e7, longitude_e7;
    uint32_t speed_mm_s, course_mdeg, utc_ms, date_yyyymmdd;
    int32_t altitude_mm;
    uint32_t satellites, hdop_milli, quality, raw_length;
    char raw[GNSS_SENTENCE_MAX];
} GnssFix;
typedef struct {
    GnssFix fix;
    uint32_t source, external_connected, valid, stale, age_ms;
} GnssSnapshot;
typedef struct {
    GnssFix external, phone;
    uint32_t stale_ms, external_connected, session, collecting, used;
    uint32_t sentences_ok, checksum_errors, format_errors, unsupported_sentences, overflows;
    char sentence[GNSS_SENTENCE_MAX];
} GnssService;

/* 같은task에서호출한다. 외부SPP 연결상태만 source선택을결정하며 nofix/stale에서
 * phone으로조용히돌아가지않는다. 연결전환은외부파서/유효fix를새세션으로초기화한다. */
void GnssService_Init(GnssService *service, uint32_t stale_ms);
void GnssService_SetExternalConnected(GnssService *service, uint32_t connected);
void GnssService_FeedExternal(GnssService *service, const uint8_t *data, size_t length, uint32_t now_ms);
/* phone프로토콜계층이해석한값을전달한다. 지정fields만유효하며시각은now_ms로고정한다. */
uint32_t GnssService_UpdatePhone(GnssService *service, const GnssFix *fix, uint32_t now_ms);
uint32_t GnssService_GetSnapshot(const GnssService *service, uint32_t now_ms, GnssSnapshot *out);
#endif
