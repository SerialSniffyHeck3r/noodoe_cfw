# 계기판 UART 서비스

2026-09-12부터 통합 런타임에서는 `DashService`가 UART5와 `VehicleService`를
단독 소유한다. 아래의 순수 수신 파서는 서비스 내부 구성요소다. UI에서
Feed/Poll/BSP_Dash_Send를 직접 호출하지 않는다.

## 상위 API와 완료

```c
uint32_t operation;
Dash_Status accepted = DashService_RequestReconnect(&operation);
/* ACCEPTED 이후 GetSnapshot().completed_id == operation을 기다리지 않고
 * 다음 UI/service 주기에 조회한다. completed_result == 0이 전송 완료다. */
Dash_Snapshot link;
DashService_GetSnapshot(&link);
VehicleSnapshot vehicle;
DashService_GetVehicle(&vehicle);
```

- `RequestEnabled(0/1,&id)`: 정지/재개. 정지는 CMD01=00의 실제 UART TC 후
  TX 핀을 입력으로 바꾼다. RX 관측은 유지한다.
- `RequestReconnect(&id)`: CMD01=04 전송. 실제 TC에서 완료하며 계기판의
  명령 수용을 뜻하지 않는다. 정상 수신이 있어야 `link_up=1`이다.
- `RequestLight(0..9,&id)`: 진단용 조도 인덱스 override. RAM 정책 적용 완료를
  뜻하며 해당 순간의 전송 완료가 아니다. 다음 정상 A1 응답에 적용된다.
  `DASH_LIGHT_AUTO`로 실제 센서/캐시 정책으로 복귀한다.
- 큐는 한 슬롯이며 BUSY면 대기하지 않고 반환한다. ISR/interrupt-mask
  문맥은 거부한다. snapshot 조회는 임계구역 내 RAM 복사만 수행한다.
- `NoodoeRuntime_GetVehicle()`은 같은 캐시를 제공한다. 수신 중단1500ms 후
  stale/valid_fields를 갱신하고 마지막 ODO 등의 값은 보존한다.

## 순정 wire 정책

115200 8N1, PD2 RX/PC12 TX. 프로젝트 전용 코드는
`Dash_Protocol.c`(시간·wire), `DashService.c`(요청·장치·캐시),
`BSP_Dash.c`(GPIO/UART ISR/ring/DMA)로 분리한다.

- 초기3000ms를 **초과**하면 `F5 01 01 04 F1`을 보낸다.
- ACTIVE 상태에서 XOR-valid, nonempty CMD21/22/41/42 네 개를 받을 때마다
  `F5 A1 02 01 index XOR`를 보낸다. 무조건400ms 주기로 보내지 않는다.
- 마지막 허용 프레임 이후400ms 초과 시 GAP으로 이동하고 새800ms 대기가
  끝나면 다시 CMD01=04를 보낸다. 미허용/빈/깨진 프레임은 타이머를 갱신하지 않는다.
- 정상 센서 millilux를 정수 lux로 내린 뒤 보존 영역0x0800C040의10개
  threshold로0..9를 만든다. donor 표는 `[8103,2981,1097,403,148,55,20,7,3,0]`.
  인덱스는 lux나 백라이트 퍼센트가 아니다.
- 센서 오류/stale이면 마지막 성공 인덱스를 유지한다. 성공 이력이 없는
  초기0은 이 donor 순정의 실제 실패 상태와 일치한다. snapshot source는
  UNAVAILABLE/LIVE/CACHED/OVERRIDE를 별도로 알린다.
- 순정은 ALS 임계 레지스터 쓰기 실패도 이전 인덱스를 유지한다. 현재
  CFW의 정상 센서 경로는 polling sample 기반이며 센서 측 모든 interrupt/
  threshold-register 부수효과까지 복제한 것은 아니다. 고장난 donor에서
  정상 광센서 변화나 실제 계기판 밝기 변화는 아직 검증할 수 없다.

TX 버퍼는 BSP의 고정 private copy다. HAL 접수 후50ms 초과에는 bounded
abort를 요청하고 실패 시 명시 오류를 남긴다. DMA 정지가 불확실하면
buffer ownership을 유지하고 TX 재사용을 거절한다. 재부팅으로만 풀리는
poison 상태를 HAL READY 위조로 해제하지 않는다. 자동 응답은 최대8개
대기하며 초과는 counter로 보고한다. queue가 비면 무한 spin하지 않는다.

통합 화면에는 페이지와 무관하게 `UART UP/WAIT/RETRY/OFF/FAIL R… T…`를
표시한다. R은 순정이 허용하는 정상 수신 프레임 누계, T는 실제 TC 완료
프레임 누계다. 결과2페이지는 최신 ODO·속도·확인된 연료0/1칸도 보여준다.
현재 링크와 과거 누적 UART 오류를 혼동하여 영구 FAIL을 띄우지 않는다.

## 순수 파서

`VehicleService`를 한 태스크가 소유하고 `Init → Feed/Poll → GetSnapshot`으로 사용한다. Feed의 조각은 UART 링/DMA 경계와 무관하다. ISR은 byte 수집만 하고 서비스 호출은 태스크에서 직렬화한다. 함수 자체에 잠금·HAL·송신·동적 할당은 없다. `now_ms`는 단조 증가하는 uint32 ms이며 unsigned 차이로 wrap을 처리한다.

115200 8N1의 `F5 cmd length payload XOR`를 최대259바이트로 보관한다. 전체 frame XOR=0에서만 확정한다. 손상된 checksum은 첫 F5만 버리고 재검색한다. 길이 손상으로 미완료가 되면100ms 무수신 뒤 다음 후보를 살핀다. 이 정책은 입력 조각 사이에100ms가 넘게 걸리지 않는 실제 링크를 전제로 한다. XOR는 손상 탐지이며 인증은 아니다.

CMD21은9바이트, CMD22는11바이트 이상에서 속도(p0), ODO(LE p4..7), status(p3)와 raw를 제공한다. 연료의 확정값은 실제 `p3=50/51`의0/1칸뿐이다. p2·status 상위 nibble·온도 후보(p8−40)·CMD22 확장값은 raw/후보로 유지한다. 다른 command와 짧은 telemetry는 최신 raw frame만 갱신한다. raw frame 시각과 마지막 telemetry 시각을 구분해야 한다.

기본 freshness는2000ms다. snapshot의 stale/validity만 내리며 마지막 값/raw를 지우지 않는다. 구조체를 다른 태스크에서 동시에 읽으면 원자 snapshot을 보장하지 않으므로 caller가 메시지/잠금으로 복사해야 한다.

근거: [Python 원본 파서](../../../../src/noodoe_protocol/vehicle_uart.py), [기존 회귀 시험](../../../../tests/test_vehicle_uart.py), [AK550 payload 연구](../../../../docs/2026-09-09-ak550-uart-payload-map.md), [연료1칸 실측](../../../../docs/2026-09-10-fromdash-gas1.md). 실측 fixture의 ODO는36,475km다. [실제 C 시험](../../tools/tests/protocol_services/README.md)은 별도 파일을 본다.

2026-09-12 [실제 UART 재생 시험](../../../../analysis/2026-09-12-uart-bringup/README.md): COM11의 ATmega를 제거한 Uno로 누도에416바이트를 전송하고 MCU416바이트/수신 오류0을 확인했다. 정상31프레임의 ODO36475·연료0/1칸·합성 속도73이 실행 중 C 파서와 상위 공개 스냅샷에 반영됐고, 의도한 XOR 오류1개 거절 및 수신 중단 stale 처리가 통과했다. 실물 계기판은 분리돼 있어 실제 계기판 연결/송신 명령 수용 검증과 구분한다. 반대 방향은 레지스터 UART TX5바이트의 PC 수신만 확인했으며 BSP TX DMA·자동 조도 응답 서비스의 성공을 뜻하지 않는다.
