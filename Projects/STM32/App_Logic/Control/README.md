# PHONE SPP 제어 계층

`NoodoeControl`은 PHONE(role0) 하나의 RFCOMM 스트림을 소유한다. ELM/두 번째 폰/외장 GPS 연결은 제품에서 제거했고 **폰 GPS 궤적은 유지**한다. BTstack 태스크에 직접 접근하거나 화면·NOR·FatFs를 호출하지 않는다. Bluetooth 공개 API로 명령을 큐에 넣고, 원자적으로 복사한 Runtime 스냅샷을 응답한다.

## Runtime 연결 계약

1. 부모 Runtime이 `UpdateService_Init()`과 플랫폼 콜백을 구성한다.
2. I/O 태스크를 시작하기 전에 `NoodoeControl_Init(&update)`와 `NoodoeControl_SetPhoneGPSCallback(callback, context)`를 호출한다. 콜백은 `GnssService_UpdatePhone()`의 검증 결과를 반환한다. UpdateService 포인터를 NULL로 주면 일반 조회는 동작하지만 업데이트는 거부한다.
3. I/O 태스크 하나가 몇 ms마다 `NoodoeControl_Process(now_ms)`를 호출한다. 다른 곳에서 PHONE `Bluetooth_Receive`를 소비하지 않는다. Storage 태스크가 별도로 `UpdateService_Process()`를 호출한다.
4. Runtime 차량·GNSS getter는 자체 동기화를 유지한다. 폰 위치는 유일한 슬롯0에서 갱신한다. 외장 GPS 스트림은 연결하지 않는다.

생성 Core 파일에 본 로직을 넣지 않는다. App_Logic 재귀 소스/include 등록으로 빌드한다. `g_noodoe_control`은 버전2의22개u32(첫 폰 슬롯) 진단이며 장치 동작 검증 시 요청·응답·파서 오류·연결 세대를 읽을 수 있다.

## NDCP v1 프레임

모든 정수는 little endian이며 signed 값은 2의 보수다. 헤더16바이트는 Python `struct <4sBBHIHH`이다: `NDCP`, 버전1, opcode, flags, sequence, payload 길이, 예약0. 뒤에 payload(최대1024바이트), 마지막에 **헤더+payload 전체**의 표준 CRC32 LE가 온다. USB NOR 백업 프로토콜과 CRC 범위가 다르다. 요청 flags=0, 응답 flags=1, 실패 응답 flags=3이다. Parser는 분할 수신과 CRC 오류 후 재동기화, 2초 조립 제한을 처리한다.

일반 응답 payload의 첫4바이트는 int32 결과다. 0은 성공, Control의1..6은 argument/not-ready/busy/unsupported/denied/transport이다. Bluetooth의 음수 API 결과는 그대로 전달한다. 아래 응답 크기는 이 첫4바이트를 **제외**한다. 구조체 padding을 직렬화하지 않는다.

| opcode | 요청 payload | 성공 응답/동작 |
|---|---|---|
|00 PING|0..64바이트|그대로 echo|
|01 INFO|없음|68바이트: protocol u32, UID3개u32, 순정 메타5개u32, NUL 종료 build명32바이트|
|02 BT|없음|208바이트 BT 진단, 아래 레이아웃|
|03 DISCOVER|예약 번호|UNSUPPORTED(4); 검색/연결/장치 명령을 실행하지 않음|
|04 RESULTS|예약 번호|UNSUPPORTED(4); 검색/연결/장치 명령을 실행하지 않음|
|05 CONNECT|예약 번호|UNSUPPORTED(4); 검색/연결/장치 명령을 실행하지 않음|
|06 DISCONNECT|role u8, 0만 유효|현재 PHONE 응답의 로컬 송신 완료 후 disconnect|
|07 PAIR|예약 번호|UNSUPPORTED(4); 검색/연결/장치 명령을 실행하지 않음|
|08 PAIR WINDOW|seconds u32|0..120초; BT 태스크가 pairing 허용 시간을 적용|
|09 PHONE GPS|11개u32, 아래 레이아웃|Runtime 콜백 검증 후 휴대폰 fix 갱신|
|0A VEHICLE|없음|19개u32 + raw frame|
|0B GNSS|없음|27개u32 + raw NMEA|
|0C OBD|예약 번호|UNSUPPORTED(4); 검색/연결/장치 명령을 실행하지 않음|
|0D CAPABILITIES|없음|schema u32=1, max phones u32=1, feature bits u32=7: bit0 inbound SPP, bit1 phone GPS, bit2 SWD backup|
|1F AUTHORIZE UPDATE|UID3개u32 + 0x42414B32|현재 PHONE 연결만 OTA 승인; 자체적으로 erase/program/reset 하지 않음|
|40..47 UPDATE|Update_Service 계약|StorageTask에 전달; 업데이트 응답의 첫20바이트 결과/state/transaction/received/verified 계약 유지|

BT의 첫17개u32는 `magic version state last_error heartbeat manufacturer lmp_subversion hci_revision patch_bytes baud commands command_rejected hci_errors pairings key_generation key_persisted_generation stack_low_words`다. 그 뒤 local address6/discovered count u8/reserved0, PHONE·예약1·예약2 순서로44바이트 링크3개가 온다. 예약 링크는 0으로 유지한다. 링크는 address6/status u8/channel u8/CID u16/MTU u16와 `rx_bytes tx_bytes rx_overflow tx_rejected reconnects last_error rx_queued tx_queued` 8개u32다.

PHONE GPS는 `fields latitude_e7 longitude_e7 speed_mm_s course_mdeg altitude_mm utc_ms date_yyyymmdd satellites hdop_milli quality` 순서다. 위경도와 고도만 signed다. fields는 GNSS public header의 위치1/시각2/날짜4/속도8/방향16/고도32/위성수64/HDOP128을 사용한다. Runtime 콜백이 단위·범위·필드 유효성을 검증하고 수신 시각을 부여한다.

VEHICLE은 `sequence valid_fields stale telemetry_ms age_ms speed_kph odometer_km fuel_observed payload2_raw status_raw status_high status_low temperature_candidate_c extended_raw extended_present frame_ms frame_command frame_length raw_length`, 이어 raw 최대259바이트다. 온도 후보만 signed다. raw 필드를 실제 차량 의미로 확정했다고 해석하지 않는다.

GNSS는 `source external_connected valid stale age_ms fields sample_ms has_sample`, `field_ms[8]`, `latitude_e7 longitude_e7 speed_mm_s course_mdeg utc_ms date_yyyymmdd altitude_mm satellites hdop_milli quality raw_length`, 이어 raw 최대159바이트다. 문자열 raw에는 별도 NUL을 보내지 않는다.

## 연결 및 업데이트 승인

Parser와 응답 큐는 CID·주소·`reconnects` 세대가 바뀌거나 연결이 끊기면 폐기한다. 동일 CID가 재사용되어도 새 연결로 취급한다. Bluetooth 계층은 성공한 PHONE incoming open마다 reconnects를 증가시켜야 한다. OTA 승인은 새 연결에 승계되지 않는다.

수신·송신은 `Bluetooth_ReceiveSession/SendSession`에 이번 poll에서 관측한 링크를 넘긴다. BT 계층이 동일 critical section에서 UP/CID/주소/세대를 검사하고 바이트를 소비·enqueue하므로 조회 직후의 재접속도 차단한다. 불일치하면 바이트를 건드리지 않고 NOT_READY를 반환하며 Control은 즉시 이전 parser/응답/승인을 폐기한다. 다음 poll에서 새 연결을 설정한다.

일반 응답은2개 한정 큐로 보존한다. 가득 차면 수신 소비를 멈춰 side effect만 실행되고 응답이 사라지는 상황을 막는다. 한 번에 최대256개 RX 바이트를 처리하며, TX1044바이트 버퍼를 최대512바이트 단위로 보낸다. UPDATE reply는 전부 enqueue되었고 동일 연결의 `tx_queued==0`이 확인된 뒤에만 `UpdateService_NotifyReplyTransmitted()`를 호출한다. 이것은 **로컬 큐 drain**이며 상대 앱의 수신 ACK가 아니다. UpdateService의 후속 reset 지연 정책을 유지한다.

`1F`의 토큰은 실수 방지용이며 인증이나 암호학적 백업 증명이 아니다. MCU는 UID 일치와 `0x08008000` 메타의 version `0x000E0000`, pending CRC=0을 확인한다. 공식 host 도구는 먼저 INFO를 조회하고 `storage_backup.verify_unlock_manifest()`로 전체128MiB A/B 두 파일을 다시 읽어 해시·동일성·UID·pending을 검증한 후에만1F를 전송한다. 백업 파일 없이 임의 토큰으로 기능을 우회하는 것을 정상 절차로 제공하지 않는다. 펌웨어가 host 파일의 존재를 독자적으로 증명할 수 있다고 주장하지 않는다.

## PC 사용과 검증

`tools/noodoe_control.py --port COM8 info`로 paired PHONE SPP COM 포트를 사용한다. `bt`, `capabilities`, `vehicle`, `gnss`로 진단을 읽고 `gps --latitude 37.5 --longitude 127.0`으로 폰 GPS 입력을 제공한다. 역사적 CLI의 discover/results/connect/pair/obd는 이전 펌웨어 기록용이며 이번 Product에서는 UNSUPPORTED다. OTA 승인은 `authorize-update --backup-manifest <전체백업 manifest.json>`이다. PC에서 보여주는115200 COM 설정은 Bluetooth 무선 속도나 MCU HCI UART 설정을 바꾸지 않는다.

`tools/tests/control_host/run.py`는 실제 Control.c와 NDCP.c를 ARM O0/Os로 컴파일하고 Unicorn에서31개 검증을 실행한다. BT/Runtime/Update는 모의 구현이므로 무선 연결 검증을 대체하지 않는다. 최대 프레임 분할, 송신 완료 시점, 동일 CID 재접속, 조회와 dequeue/enqueue 사이의 강제 세대 변경, UID 거부, 업데이트 BUSY 형식, GPS 전달을 확인한다. `tools/tests/test_noodoe_control.py`의9개 테스트는 실제 host parser, CRC 오류, 조각 수신, signed 스냅샷, 변경된 백업과 UID에 대한 승인 거부를 확인한다. 디바이스는 접근하지 않는다.

## 2026-09-21 단일 세션

NDCP parser, 응답2개, TX 프레임, CID/주소/epoch를 슬롯0 하나가 소유한다.
매 Process에서 RX 최대256B, TX 최대512B를 처리한다. 재접속 시 parser,
미전송 응답과 업데이트 승인을 폐기한다. 이전 프레임의 ACK를 새 연결에
전달하지 않는다. USB CDC 프로토콜은 삭제했지만 host 백업 증명 검증은 유지한다.

09 폰 GPS, 음악/알림/리모컨 경로와 업데이트/복구 프로토콜 번호는 유지한다.
0D capability는 구성 지원을 알리며 현재 RF 연결 성공을 뜻하지 않는다.
기존 role1/2 요청은 거부하고 예전 설정 키/페이지 ID를 다른 기능으로 재사용하지 않는다.
