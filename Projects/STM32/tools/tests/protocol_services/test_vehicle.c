#include "test_common.h"
#include "Vehicle_Service.h"
static VehicleService service;
static VehicleSnapshot snapshot;
static const uint8_t fuel0[]={0xF5,0x21,9,0,0,0,0x50,0x7B,0x8E,0,0,0,0x78};
static const uint8_t fuel1[]={0xF5,0x21,9,0,0,0,0x51,0x7B,0x8E,0,0,0,0x79};

/* fixture checksum은 서비스 함수를 호출하지 않고 알려진 XOR wire 규칙으로 만든다. */
static size_t Frame(uint8_t *out,uint8_t command,const uint8_t *payload,uint8_t n)
{
    uint32_t i; uint8_t x=0;
    out[0]=0xF5;out[1]=command;out[2]=n;
    for(i=0;i<n;++i) out[3U+i]=payload[i];
    for(i=0;i<(uint32_t)n+3U;++i) x^=out[i];
    out[3U+n]=x; return (size_t)n+4U;
}

/* 실측 0/1칸, 모든 fragment 경계, 손상 후 재동기화, 최대 길이, 시간 wrap을
 * 실제 서비스에 입력한다. 후속 정상 frame이 살아나는지까지 확인한다. */
int TestVehicle(void)
{
    uint32_t split,i; uint8_t data[300],payload[255]; size_t n;
    VehicleService_Init(&service,2000U);
    CHECK(VehicleService_GetSnapshot(&service,0U,&snapshot));
    CHECK(snapshot.stale && !snapshot.valid_fields);
    for(split=0;split<=sizeof(fuel0);++split) {
        VehicleService_Init(&service,2000U);
        VehicleService_Feed(&service,fuel0,split,10U);
        VehicleService_Feed(&service,fuel0+split,sizeof(fuel0)-split,11U);
        CHECK(service.frames_ok==1U && service.link_frames_ok==1U);
        CHECK(VehicleService_GetSnapshot(&service,12U,&snapshot));
        CHECK(snapshot.odometer_km==36475U && snapshot.fuel_observed==0U);
        CHECK(snapshot.valid_fields==7U && snapshot.raw_length==13U);
    }
    VehicleService_Feed(&service,fuel1,sizeof(fuel1),20U);
    CHECK(VehicleService_GetSnapshot(&service,21U,&snapshot));
    CHECK(snapshot.fuel_observed==1U && snapshot.status_raw==0x51U);
    CHECK(snapshot.speed_kph==0U && snapshot.payload2_raw==0U);
    CHECK(VehicleService_GetSnapshot(&service,2021U,&snapshot));
    CHECK(snapshot.stale && !snapshot.valid_fields && snapshot.odometer_km==36475U);
    /* 부서진 checksum 바로 뒤의 정상 frame을 동일 feed에서 복구한다. */
    VehicleService_Init(&service,0U);
    data[0]=0x11; data[1]=0x55;
    memcpy(data+2,fuel0,sizeof(fuel0)); data[14]^=1U;
    memcpy(data+15,fuel1,sizeof(fuel1));
    VehicleService_Feed(&service,data,28U,100U);
    CHECK(service.frames_ok==1U && service.checksum_errors==1U);
    CHECK(service.latest.status_raw==0x51U && service.discarded_bytes==15U);
    /* 실제 분석 저장소의 회귀 vector와 후보/확정 필드 구분을 대조한다. */
    { static const uint8_t vector[]={0xF5,0x21,9,6,0x11,0x55,0x9A,0x78,0x56,0x34,0x12,0x41,0x4C};
      VehicleService_Feed(&service,vector,sizeof(vector),110U); }
    CHECK(service.latest.speed_kph==6U && service.latest.odometer_km==0x12345678U);
    CHECK(service.latest.temperature_candidate_c==25 && service.latest.valid_fields==3U);
    { static const uint8_t p[]={0,1,0x64,0x10,0x39,0x30,0,0,0x28,0x34,0x12};
      n=Frame(data,0x22,p,sizeof(p)); VehicleService_Feed(&service,data,n,120U); }
    CHECK(service.latest.extended_present && service.latest.extended_raw==0x1234U);
    CHECK(service.latest.odometer_km==12345U && service.latest.temperature_candidate_c==0);
    /* 정상 payload의 F5를 delimiter로 쪼개지 않는다. */
    memset(payload,0xF5,sizeof(payload)); n=Frame(data,0x42,payload,255U);
    for(i=0;i<n;++i) VehicleService_Feed(&service,data+i,1U,130U);
    CHECK(service.latest.frame_length==255U && service.latest.raw_length==259U);
    CHECK(service.latest.odometer_km==12345U && service.latest.telemetry_ms==120U);
    /* checksum은 맞지만 잘린21 payload는 원래 telemetry를 덮어쓰지 않는다. */
    n=Frame(data,0x21,payload,2U); VehicleService_Feed(&service,data,n,140U);
    CHECK(service.payload_errors==1U && service.latest.telemetry_ms==120U);
    VehicleService_Init(&service,0U);
    data[0]=0xF5;data[1]=0x21;data[2]=255U;
    memcpy(data+3,fuel0,sizeof(fuel0));
    VehicleService_Feed(&service,data,16U,100U);
    CHECK(service.frames_ok==0U && service.buffered==16U);
    VehicleService_Poll(&service,199U); CHECK(service.frames_ok==0U);
    VehicleService_Poll(&service,200U);
    CHECK(service.frames_ok==1U && service.partial_timeouts==1U && !service.buffered);
    CHECK(service.latest.telemetry_ms==100U);
    VehicleService_Init(&service,50U);
    VehicleService_Feed(&service,fuel0,sizeof(fuel0),UINT32_MAX-20U);
    CHECK(VehicleService_GetSnapshot(&service,10U,&snapshot));
    CHECK(!snapshot.stale && snapshot.age_ms==31U);
    CHECK(VehicleService_GetSnapshot(&service,31U,&snapshot)); CHECK(snapshot.stale);
    VehicleService_Feed(&service,NULL,1U,31U);
    CHECK(!VehicleService_GetSnapshot(NULL,0U,&snapshot));
    CHECK(!VehicleService_GetSnapshot(&service,0U,NULL));
    /* Only stock-admitted, nonempty IDs maintain the transmit handshake.
     * Unknown and empty frames remain available as raw diagnostic evidence. */
    VehicleService_Init(&service,50U);
    for(i=0;i<256U;++i){
        uint32_t before=service.link_frames_ok;
        n=Frame(data,(uint8_t)i,payload,0U);VehicleService_Feed(&service,data,n,100U);
        CHECK(service.link_frames_ok==before);
        n=Frame(data,(uint8_t)i,payload,1U);VehicleService_Feed(&service,data,n,101U);
        CHECK(service.link_frames_ok==before+((i==0x21U||i==0x22U||i==0x41U||i==0x42U)?1U:0U));
    }
    return 0;
}
