#include "Vehicle_Service.h"
#include <string.h>

/* buffer 앞쪽 n bytes를 소비한다. 손상/잡음 때문에 버린 경우에만 discarded를
 * 올리고, 남은 byte는 순서를 보존해 다음 F5 후보의 재동기화에 사용한다. */
static void Drop(VehicleService *s, uint32_t n, uint32_t discarded)
{
    if(n>s->buffered) n=s->buffered;
    if(discarded) s->discarded_bytes+=n;
    s->buffered-=n;
    if(s->buffered) memmove(s->buffer,s->buffer+n,s->buffered);
}

/* 21/22 payload의 공통9-byte prefix만 해석한다. 연료의0/1칸은payload[3]의
 * 50/51 실측에서만 확인됐으므로 payload[2]나상위5를연료퍼센트로환산하지 않는다. */
static void DecodeTelemetry(VehicleService *s, uint32_t now)
{
    VehicleSnapshot *v=&s->latest;
    const uint8_t *p=s->buffer+3;
    uint32_t command=s->buffer[1], length=s->buffer[2];
    if(command!=0x21U && command!=0x22U) return;
    if(length<9U || (command==0x22U && length<11U)) {
        ++s->payload_errors;
        return;
    }
    v->speed_kph=p[0];
    v->payload2_raw=p[2];
    v->status_raw=p[3];
    v->status_high=p[3]>>4;
    v->status_low=p[3]&15U;
    v->odometer_km=(uint32_t)p[4]|((uint32_t)p[5]<<8)|((uint32_t)p[6]<<16)|((uint32_t)p[7]<<24);
    v->temperature_candidate_c=(int32_t)p[8]-40;
    v->extended_present=command==0x22U;
    v->extended_raw=command==0x22U ? (uint32_t)p[9]|((uint32_t)p[10]<<8) : 0U;
    v->valid_fields=VEHICLE_VALID_SPEED|VEHICLE_VALID_ODOMETER;
    v->fuel_observed=v->status_low;
    if(p[3]>=0x50U&&p[3]<=0x55U)v->valid_fields|=VEHICLE_VALID_FUEL_OBSERVED;
    if(p[3]>=0x52U&&p[3]<=0x55U)v->valid_fields|=VEHICLE_FUEL_INFERRED;
    if(!p[3])v->valid_fields|=VEHICLE_FUEL_MEASUREMENT_ERROR;
    v->telemetry_ms=now;
    s->have_telemetry=1U;
}

/* 길이와 XOR가 맞는 전체 frame만 확정한다. XOR 오류에서는 F5 한 byte만
 * 버려 frame 내부의 다음 후보를 다시 찾는다. payload의 정상 F5는 분리하지 않는다. */
static void Drain(VehicleService *s, uint32_t now)
{
    while(s->buffered) {
        uint32_t i,total;
        uint8_t sum=0;
        for(i=0;i<s->buffered && s->buffer[i]!=0xF5U;++i) { }
        if(i) Drop(s,i,1U);
        if(s->buffered<3U) return;
        total=4U+s->buffer[2];
        if(s->buffered<total) return;
        for(i=0;i<total;++i) sum^=s->buffer[i];
        if(sum!=0U) {
            ++s->checksum_errors;
            Drop(s,1U,1U);
            continue;
        }
        ++s->frames_ok;
        if(s->buffer[2] && (s->buffer[1]==0x21U || s->buffer[1]==0x22U ||
                           s->buffer[1]==0x41U || s->buffer[1]==0x42U))++s->link_frames_ok;
        ++s->latest.sequence;
        s->latest.frame_ms=now;
        s->latest.frame_command=s->buffer[1];
        s->latest.frame_length=s->buffer[2];
        s->latest.raw_length=total;
        memcpy(s->latest.raw,s->buffer,total);
        DecodeTelemetry(s,now);
        Drop(s,total,0U);
    }
}

/* 서비스 한 개를 호출자 메모리에 초기화한다. stale_ms=0은 2000ms 기본값이다. */
void VehicleService_Init(VehicleService *s, uint32_t stale_ms)
{
    if(!s) return;
    memset(s,0,sizeof(*s));
    s->stale_ms=stale_ms ? stale_ms : 2000U;
}

/* 정상259-byte frame은115200 8N1에서약22.5ms다.100ms 무수신 뒤 남은
 * 불완전 후보는 폐기하고 그 뒤에 이미 들어온 정상 후보를 다시 살핀다. */
void VehicleService_Poll(VehicleService *s, uint32_t now)
{
    if(!s) return;
    if(s->buffered && now-s->last_byte_ms>=VEHICLE_PARTIAL_TIMEOUT_MS) {
        ++s->partial_timeouts;
        /* timeout 시각에 새 telemetry가 수신됐다고 기록하지 않는다. */
        do {
            Drop(s,1U,1U);
            Drain(s,s->last_byte_ms);
        } while(s->buffered);
    }
}

/* 링버퍼 경계나 DMA 조각 길이에 의존하지 않는 byte stream 입력이다. 하나의
 * 최대 frame 크기만 보관하므로 공격적인 길이/잡음에도 메모리 사용량이 증가하지 않는다. */
void VehicleService_Feed(VehicleService *s, const uint8_t *data, size_t length, uint32_t now)
{
    size_t i;
    if(!s || (!data && length)) return;
    VehicleService_Poll(s,now);
    for(i=0;i<length;++i) {
        if(s->buffered==VEHICLE_FRAME_MAX) Drop(s,1U,1U);
        s->buffer[s->buffered++]=data[i];
        s->last_byte_ms=now;
        Drain(s,now);
    }
}

/* 이전 정상 데이터를 지우지 않고 현재 freshness만 복사본에 적용한다. uint32
 * 차이 연산으로 일반적인 ms tick wrap을 처리하며 raw와후보값은stale에도남는다. */
uint32_t VehicleService_GetSnapshot(const VehicleService *s, uint32_t now, VehicleSnapshot *out)
{
    if(!s || !out) return 0U;
    *out=s->latest;
    out->age_ms=s->have_telemetry ? now-out->telemetry_ms : UINT32_MAX;
    out->stale=!s->have_telemetry || out->age_ms>s->stale_ms;
    if(out->stale) out->valid_fields=0U;
    return 1U;
}
