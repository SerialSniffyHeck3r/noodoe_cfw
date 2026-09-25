# BL 0.15 / SR0701 호환성 조사

2026-09-21. 순정 APK 2.1.14, 과거 실차 SPP 수신 원문, 벤치 전체 내부 플래시, 보관 OTA 9개, 공식 서버의 현재 응답, 현재 CFW/Bootstrap/Gate/Android 구현을 대조했다. 이번 작업은 조사와 오프라인 시험이다. 제품 코드 수정, MCU 접속, 플래시 쓰기는 하지 않았다.

## 결론

BL 0.15가 CFW와 본질적으로 호환되지 않는다는 증거는 찾지 못했다. 오히려 실차 정보로 공식 서버가 선택하는 APP는 벤치에서 덤프한 APP와 동일하다. 그러나 현재 구현에는 **실차에서 확인된 패널 값 4를 거부하는 코드**, **BL 버전 0.14를 고정 magic처럼 취급하는 코드**, **벤치 BL 해시에 묶인 복구 경로**가 있다. 설치 허용 플래그만 지워서는 해결되지 않는다.

BL 0.15 실행 파일 자체는 확보하지 못했다. 따라서 두 BL의 명령어 차이, 초기 주변장치 상태, 설치 중 전원 차단 및 실패 처리 차이는 아직 직접 비교할 수 없다. 아래에서 확인된 제품 차이와 미확인 BL 구현 차이를 분리한다.

## 1. 실제로 확인된 두 기기의 차이

실차 DeviceInfo는 `../2026-09-20-bootstrap-recovery/stock-identity-observed-boot015/reply.bin`을 재파싱했다. OQC는 `../../captures/opennoodoe-live-2026-08-31-0339/20260831-033459-580/protocol.log:115`의 원시 수신 프레임에서 다시 추출했다. 외부 프레임 길이, 순정 APK 방식의 payload checksum, 명령 ID, 내부 길이, 성공 status를 확인했다.

| 항목 | 벤치 | 정상 실차 | 의미 |
|---|---|---|---|
| BL 표시 버전 | 0.14 | 0.15 | 메타데이터 값의 차이. BL 실행 파일의 해시 차이를 직접 측정한 것은 아님 |
| PCBA 문자열 | sr0601 | SR0701 | 제조 식별값. GPIO strap 숫자와 동일시할 수 없음 |
| 부품번호 | 0037150-lgc6-e00 | 0037150-LGC6-E01 | 대소문자 외 E00/E01 차이도 있음 |
| 패널 selector, factory offset128 | 3 | 4 | 현재 CFW/Gate가 실차 값을 거부함 |
| 공장 OQC FW 필드 | 1.22 | 1.62 | 현재 실행 FW가 아님 |
| 현재 순정 FW | 5.16 | 5.16 | 아래 공식 배포 이미지와 별도로 구분 |
| 조립번호 | 0037130-LGC6-B80 | 동일 | 동일 |
| 모델 | SAA1AA(KR) | 동일 | 동일 |
| 최고속도 / language | 200 / 12 | 동일 | 동일 |
| unit / type / language pack | 1 / 1 / 2 | 동일 | 동일 |
| motor / resource / dashboard ID | 1 / 1 / 1 | 동일 | 동일 |
| 밝기 threshold 10개 | 8103,2981,1097,403,148,55,20,7,3,0 | 동일 | 조도 보정표의 차이는 없음 |
| GPIO strap revision | 실측 6 | 미확보 | SR0701이라는 이름으로 7이라고 추정하면 안 됨 |

개체별 serial/MAC는 당연히 별개이며 벤치 값을 실차에 이식할 대상이 아니다. 파싱 결과는 `evidence.json`, `panel-results.json`에 있다. 실차의 `0x0800C080` 값은 해당 주소를 SWD로 읽은 것이 아니라 순정의 OQC 복사 경로를 통해 읽힌 값이다.

## 2. 공통 APP를 사용하는 근거: 버전 문자열보다 강함

공식 APK `ota/OTASeriesQueryRequest.java`의 요청 필드는 language_packet, motor_series, resource_id, default_dashboard_id 네 개다. BL 버전이나 PCBA 필드는 없다. `FWQueryRequest.java`도 series/resource series, 채널, APP/FW 버전을 사용하며 BL0.14/0.15별 분기는 없다.

2026-09-21 공식 서버에 실차의 실제 tuple `(2,1,1,1)`로 읽기 목적 조회했다.

1. `http://nars.noodoe.com/v3/querySeries` → firmware_series_id=1, resource_series_id=4.
2. `http://nars.noodoe.com/v3/queryFirmware` stable, FW5.16, 위 series → `SR1.5_ota_V516.bin`.
3. 지정된 공식 CDN 파일을 다시 받아 보관 파일 및 벤치 APP와 비교했다.

공식 파일: https://sunray-cdn.noodoe.com/firmwares/1657088080998-s1-SR1.5_ota_V516.bin

- OTA 길이: 458,748B.
- OTA SHA256: `3b64673054ca84b9cf504f4cc7ebcb2e6615b9fbf59df79a37a92dcf14f037ca`.
- 벤치 덤프 `0x08010000`부터 OTA 길이만큼 비교한 차이: **0 byte**.
- 끝의 FF 4B를 포함하여 448KiB로 비교한 SHA256: `162faeecc64bf56d828570168785e66d1a7da0152ef912bf83f9a19094d91edf`, 양쪽 동일.
- 서버 응답/시각/요청/해시: `official-queries.json`. 다운로드 사본: `official_v516.bin`.

따라서 실차용으로 공식 서버가 선택한 실행 APP는 벤치 APP와 동일하다. 실차의 현재 APP 전체를 직접 덤프한 것은 아니므로 그 실행 중인 바이트까지 직접 동일성을 측정했다고 주장하지 않는다. 같은 APP가 두 BL에 설치 요청을 전달하도록 배포된다는 것은 공통 업데이트 계약의 강한 근거이나, BL0.15의 전원 차단 처리까지 증명하지는 않는다.

## 3. 새로 특정한 실제 화면 호환 문제

순정 `InputCommandParser.java:765`는 OQC panel 값을 u16으로 파싱한다. 순정 MCU APP `0x08030E38` 부근은 factory `0x0800C000`에서 137B를 응답에 복사한다. panel 필드는 factory+128이므로 실차 응답의 4는 `0x0800C080`의 4에 대응한다. 버전 보정(+1 등)은 없다.

현재 코드:

- `../../STM32CubeProjects/FuckNudo_Noodoe_CFW_Project/Drivers/BSP/src/bsp_lcd_panel.c:10,169`: PANEL_PROTOCOL=3이고 다른 값이면 실패.
- `../../STM32CubeProjects/FuckNudo_Noodoe_CFW_Project/RecoveryGate/src/gate_display.c:39`: `*(u16*)0x0800C080 != 3`이면 실패.
- Product와 Bootstrap의 BSP뿐 아니라 독립 복구 및 early-rescue 표시도 함께 수정해야 한다. Gate의 화면 초기화 실패가 복구 버튼 처리까지 모두 중단시킨다는 뜻은 아니다.

순정 APP의 패널 초기화 `0x0806AB0A`는 selector가 **10 이상이고 255가 아닐 때** 다른 명령 계열을 쓴다. 3과 4는 모두 같은 legacy 명령 계열이다.

이를 문서 추정에서 멈추지 않고 원본 ARM 명령어를 Unicorn에서 실행했다. selector3/4 각각에 모의 strap2/3/6/7을 주입했다. 같은 strap끼리는 초기화 callback 순서·명령·delay·반환값이 모두 같았다. status=0x009C일 때 성공하고 status=0일 때 오류5로 종료하는 것도 확인했다. **9개 시험 통과**. 스크립트 `panel_probe.py`, 결과 `panel-results.json`.

공통 명령은 SleepOut(0x1100) →120ms→DisplayOn(0x2900)→PowerMode(0x0A00) 읽기/0x009C 검사다. 16bit wire word는 각각 `[2011,0000,4000]`, `[2029,0000,4000]`, `[200A,0000,C000]`이다.

한편 reset GPIO 경로는 selector가 아니라 **별도 GPIO strap**에 의해 `<3`/`>=3`으로 갈린다. `<3`이면 EVE GPIO7, `>=3`이면 MCU PC13을 사용한다. strap 입력은 PA3/PH3/PH2/PB10이며 HIGH를 순서대로 bit0..3에 조합한다. 현재 BSP는 실측한 벤치6의 경로에 고정되어 있다. 정상 기기의 실제 strap을 읽어 적절한 profile을 선택해야 한다. PCBA SR0701에서 7을 유추하지 않는다.

이 시험은 순정 분기 및 명령의 동일성 확인이다. GPIO/SPI 응답은 모의값이며 실제 실차 패널 성공 시험이 아니다.

## 4. BL 연동에서 고칠 부분

순정 V5.16의 부트 버전 reader `0x08031BB4`는 `0x08008000`의 두 halfword를 읽어서 반환한다. 버전0.14는 0x000E0000, 실차가 보고한0.15는 이 읽기 계약상 0x000F0000이다.

순정의 업데이트 요청 writer `0x08031BEE`는 **0x08008004부터 16B**를 쓰도록 하위 저장 함수를 호출한다. 선행 4B 부트 버전은 보존한다. 하위 구현은 sector shadow/erase/rewrite이므로 물리적으로16B만 직접 덮는다는 뜻은 아니다.

| 주소/값 | 공통 순정 APP에서 확인된 의미 |
|---|---|
| 0x08010000 | APP 시작 |
| 448KiB | 기존 APP envelope |
| 외장0x07F90000 | APP 업데이트 staging |
| 0x08008000 | 유지할 기존 부트 버전 |
| 0x08008004 | 새 APP major/minor |
| 0x08008008 | staging block 0x7F90 |
| 0x0800800C | 완료 길이 |
| 0x08008010 | CRC/설치 요청 표시 |

현재 우리는 부트 버전 필드를 고정 magic처럼 검사한다. 아래 의존성은 하드웨어 비호환의 증거가 아니라 구현을 일반화해야 할 지점이다.

| 계층 | 수정 대상 |
|---|---|
| Update_Metadata | 실제 타깃의 부트 버전 보존. 비교만 풀고 word0=0xE 대입을 남기면 버전 필드를 훼손함 |
| Recovery_Core / Update_Service / RuntimeUpdate / NoodoeControl | 요청·commit·재부팅 상태 검사에서0xE만 허용하는 부분 제거 및 대상 profile 사용 |
| RecoveryResident_Verify | 벤치 32KiB BL SHA 고정 대신 대상의 검증·보존한 BL 식별에 바인딩 |
| recovery_store 컨테이너 | header에0xE 고정한 생산자/소비자와 형식·검증을 함께 변경 |
| Android NdcpSession / BootstrapProvisioner / RoutineUpdateSession | 같은 버전·해시 고정 의존성 일반화 |
| recovery_bundle.py / Android bundle builder | 실제 target 정보와 요구 기능으로 패키지 호환성 산정 |

직전 조사에서 현 production C를 O0/Os로 컴파일한 뒤 모의 플래시로 실행했다. 0xF에서는 RecoveryCore_Request 및 UpdateMetadata_Commit이 erase/program 전에 거부되었다. 0xE는 같은 조건에서 성공했다. 근거와 재현 스크립트는 `../2026-09-21-bl015-dependency-audit/REPORT.md`, `results.json`, `probe.py`.

검사한 업데이트/복구 경로에서 BL0.14의 비공개 함수 주소로 직접 호출하는 의존성은 발견하지 못했다. 주된 인계는 staging+metadata+reset이다. APP 전체의 모든 간접 호출에 대한 전수 부재 증명은 아니다.

## 5. 순정 APK와 보관 파일에서 못 얻은 것

- `com.noodoe.sunray` Java 파일2,568개에서 BL/PCBA getter 참조를 조사했다. BL 값은 저장/설정 화면 표시용 참조가 확인되며, OTA 선택 경로에서0.14/0.15 구분을 찾지 못했다.
- `CommonStruct.getDeviceType`은 protocol과 FW로 SR1.5 경로를 선택한다. BL 버전으로 세대를 구분하지 않는다.
- APK 안 OTAInterface에는 firmware/resource query와 URL 다운로드가 있으며, 검토한 경로에 BL별 다운로드 endpoint는 없다.
- BASIC_FIRMWARE_UPGRADE enum이 존재한다는 것만으로 현재 정상 기기에서 BL을 읽거나 교체할 수 있다는 뜻은 아니다. 정상 stock 명령으로 BL0.15 전체를 읽는 경로는 확보하지 못했다.
- APK/split 10개 entry-name 인벤토리에서 BL0.15 이미지 후보를 찾지 못했다. `DebugProbesKt.bin`은 MCU 펌웨어가 아니다. 이름 검색만으로 native library 내부 압축 payload의 절대 부재를 보장하지 않는다.
- 보관 OTA9개는 APP 계열 파일이다. V5.16의 공식 release note는 `Fix issues`뿐이며 BL0.14→0.15 변경 내역이 아니다.
- 로컬의 기존 BL 이미지 탐색 결과와 이번 OTA/APK 점검에서 비교 가능한0.15 실행 파일은 확보하지 못했다.

따라서 아직 알 수 없는 것은 BL0.15 실행 코드/해시, 초기 GPIO·clock·watchdog 처리, CRC/길이/소거/재시도 처리의 변경, 설치 중 전원 손실 후 정확한 진행이다. 0.14에서 발견한 한계가0.15에서도 그대로 있거나 수정되었다고 말할 근거는 없다.

BT 칩 revision·실제 HCI 응답·NOR ID·SDRAM 동작 역시 PCBA/BL 문자열만으로 판정할 수 없다. 순정에서 Bluetooth SPP가 정상인 사실과 현재 손상된 벤치에서 CFW BT가 검증되지 않은 사실을 분리한다. 동일 APP와 동일 설정은 드라이버 공유의 근거이며 CFW 무선 동작 완료 시험을 대신하지 않는다.

## 6. 0.15 지원을 만드는 순서

1. 부트 버전/BL hash/패널 selector/GPIO strap을 분리한 target profile을 만든다. 알려진 selector3/4를 순정과 동일한 legacy profile로 지원하고 Bootstrap, Product, Gate 표시를 함께 맞춘다.
2. 부트 메타데이터의 원래 버전을 보존하도록 모든 producer/consumer를 일괄 수정한다. 부트 번호를0.14로 바꾸거나 BL 자체를 낮출 필요가 있다고 입증된 것은 없다.
3. Bootstrap에 제한된 읽기 전용 타깃 정보·BL 보존 export를 추가한다. 현재 stock 앱에 BL 전체 read가 있는 것으로 가정하지 않는다. 현행 hash 조회만으로 전체덤프를 받았다고 취급하지 않는다.
4. 최초 Bootstrap 실행 후 해당 기기의 BL32KiB와 하위64KiB의 metadata/factory 영역을 별도로 반복 읽기/비교하여 폰에 보관한다. UID와 원래 boot metadata, 실제 resident hash를 연결한다. 벤치의 공장 데이터로 덮어쓰지 않는다. 이 단계는 첫 Bootstrap 설치 이후에 가능하므로 최초 순정BL 인계의 사전 검증을 대체할 수 없다.
5. SRAM/SDRAM/NOR, 디스플레이, IGN·O 버튼, 실제 HCI/SPP 송수신 및 저장소 구조를 진단한다. GPIO strap에 따른 power/reset 경로도 확인한다.
6. 같은 metadata/복구 profile로 순정 복원·후보 실패·watchdog·버튼 복구를 검증한다. 정상 진입과 설치 중 전원 상실 복구는 별개의 시험이다. 일상 CFW APP 설치를 수행하는 Gate 경로와 최초 설치/순정 복원에서 순정BL에 인계하는 경로도 분리한다.

첫 작업은 BL 다운그레이드가 아니라 **확인된 동일 APP 계약을 유지하며 벤치 전용 가정을 제거하는 작업**이다. 새 패널 값 지원과 version-preserving 저장 처리가 빠진 패키지는 단순히 허용 플래그만 바꿔 배포해서는 원하는 UI/복원 기능을 제공하지 못한다.

## 재현 산출물

- `collect.py` / `evidence.json`: DeviceInfo, APK 참조, archive 인벤토리, APP 바이트 비교.
- `query_official.py` / `official-queries.json` / `official_v516.bin`: 공식 서버 조회와 원본 다운로드.
- `panel_probe.py` / `panel-results.json` / `vehicle-oqc-reply.bin`: OQC 재파싱, 두 기기의 공장 필드 비교, 원본 ARM 패널 분기9건 시험.

파일 내 개인 식별자는 로컬 연구 증거로만 유지한다. 전송한 공식 서버 조회는 모델 선택용 공통 필드만 포함하며 MAC/serial/UID는 보내지 않았다.
