# 상위 애플리케이션용 API 안내

Middlewares/Noodoe는 보드 드라이버 위의 통신·파싱·저장·업데이트 계층이다. 상위 화면은 Runtime 스냅샷과 공개 API를 사용하고 HAL handle, parser 내부, BTstack, FatFs 내부 객체를 직접 공유하지 않는다. 앱 정책·상태·실행 조정은 [App_Logic](../../App_Logic/README.md)에 둔다. 현재 구현과 실기 증거를 구분하며, 이 문서는 전체 브링업 완료 선언이 아니다.

## 모듈과 진입점

| 모듈 | 상위에서 사용할 기능 | 계약 |
|---|---|---|
|Runtime|`Start`, `GetVehicle/GetGnss/GetObd`, 그래픽 heartbeat|[NoodoeRuntime.h](../../App_Logic/Runtime/inc/NoodoeRuntime.h); 태스크 생성·snapshot 복사|
|Vehicle|`VehicleService_Init/Feed/Poll/GetSnapshot`|[헤더](Vehicle/inc/Vehicle_Service.h) · [프로토콜/필드](Vehicle/README.md); F5/XOR frame, 속도·ODO·raw|
|GNSS|외부 NMEA, 휴대폰 fix, 유효성·신선도·소스 선택|[헤더](GNSS/inc/GNSS_Service.h) · [정책](GNSS/README.md)|
|OBD|ELM 초기화·명령 생성·prompt 파싱·Mode01 값|[헤더](OBD/inc/OBD_Service.h) · [단위/제약](OBD/README.md)|
|Bluetooth|검색·pair·역할별 연결·진단·세션에 묶인 송수신|[NoodoeBluetooth.h](Bluetooth/inc/NoodoeBluetooth.h) · [README](Bluetooth/README.md); 큐 수락과 실제 연결 완료는 별개|
|Ambient|조도 probe/enable 요청·완료 조회·신선도 포함 snapshot|[AmbientService.h](Ambient/inc/AmbientService.h) · [README](Ambient/README.md); 버스 접근은 Storage worker만 수행|
|Control|PHONE NDCP 명령, GPS 입력, 진단, 업데이트 승인·전달|[헤더](../../App_Logic/Control/inc/NoodoeControl.h) · [wire/API](../../App_Logic/Control/README.md)|
|Storage|파일 read/write/stat/mkdir/remove, NVM blob, 명시적 format|[StorageService.h](Storage/inc/StorageService.h) · [배치/권한](Storage/README.md)|
|Settings|초기 marker 검증, BT 키 import/persist, 설정·역할 binding 비동기 저장|[SettingsService.h](../../App_Logic/Settings/inc/SettingsService.h) · [레코드/완료 판정](../../App_Logic/Settings/README.md)|
|USB|CDC ACM transport; 수신 링과 송신 완료|[USBBridge.h](USB/inc/USBBridge.h); 원본 USB Device는 Third_Party, 프로젝트 설정은 Middlewares/Noodoe|
|StorageBackup|USB CDC 전체 NOR 읽기, host unlock/format 요청|[StorageBackup.h](Storage/inc/StorageBackup.h) · [host/protocol](Storage/README.md)|
|StorageSWD|실행 중 SRAM mailbox→전용 SDRAM→SWD로 전체 NOR 읽기|[StorageSWD.h](StorageSWD/inc/StorageSWD.h) · [검증/도구](StorageSWD/README.md)|
|Update|APP staging·검증·명시적 commit/reset, 순정 BL 인계|[Update_Service.h](Update/inc/Update_Service.h) · [업데이트 계약](Update/README.md) · [Runtime 연결](../../App_Logic/Runtime/inc/RuntimeUpdate.h)|

일반 화면은 `NoodoeRuntime_Get*()`의 반환값, `valid_fields`, `stale`, `age_ms`를 확인한다. 초기값0을 실측0으로 표시하지 않는다. 차량 연료의 확정 근거는 UART status `0x50/0x51`의0/1칸이며 나머지 연료 단계·온도 후보를 검증된 센서값으로 확장하지 않는다. Settings request의 OK는 큐 수락이고, `completed_request/completed_result`가 저장 완료다.

추가한 [장치 API 계층 계약](DEVICE_API_CONTRACT.md)은 공개 요청과 BSP의 커맨드 사이에
서비스를 둔다. 조도는 이 경로를 실제 사용하며 HUD는 `AmbientService_GetSnapshot()`만
호출한다. BT는 기존 소유 태스크/큐에 연결·취소 제한을 추가했다. Storage의 동기 파일
API와 RTC·백라이트의 직접 BSP 호출까지 모두 이행했다고 해석하지 않는다.

요청별 완료 조회는 Ambient의 operation ID와 BT `Bluetooth_RequestStart()`의
요청 번호/접수 ACK/완료 결과에서 제공한다. BT의 기존 연결·해제·검색·페어링·키 삭제·정지
명령은 큐 접수 뒤 실행 단계에서 거부될 수도 있으며, 아직 요청별 완료 ID는 없다.
`command_rejected`는 전역 집계이고 링크 snapshot은 현재 상태이므로 특정 명령의 완료
응답으로 취급하지 않는다. BT 연결·해제부터 같은 완료 계약으로 확장하는 작업은
[다음 이행 범위](DEVICE_API_CONTRACT.md#현재-이행-우선순위)로 남아 있다.

## BSP·vendor 경계

[BSP_Display.h](../Drivers/BSP/inc/BSP_Display.h)와 [BSP_Buttons.h](../Drivers/BSP/inc/BSP_Buttons.h)는 화면·백라이트·버튼 공개 API다. LVGL 객체/화면 정책은 [Graphics](../Graphics/README.md)가 소유한다. EVE bus/panel 하위 파일을 상위 로직에서 직접 호출하지 않는다. 활성 원은 [Graphics_Viewport.h](../Graphics/Port/inc/Graphics_Viewport.h)의 중심(239.5,239.5), 반경239.5다. 이 원 밖에 UI를 배치하지 않는다. 실제 raster 주사는480×480으로 유지된다.

[BSP_Dash.h](../Drivers/BSP/inc/BSP_Dash.h)는 UART5 RX 링과 명시적 TX 활성화, [BSP_BT_HCI.h](../Drivers/BSP/inc/BSP_BT_HCI.h)는 HCI UART/DMA, [BSP_NOR.h](../Drivers/BSP/inc/BSP_NOR.h)는 SPI5 물리 읽기·범위 제한 쓰기, [BSP_RAM.h](../Drivers/BSP/inc/BSP_RAM.h)는 SDRAM 초기 시험·arena를 담당한다. [BSP_Ambient.h](../Drivers/BSP/inc/BSP_Ambient.h), [BSP_Clock.h](../Drivers/BSP/inc/BSP_Clock.h), [BSP_Power.h](../Drivers/BSP/inc/BSP_Power.h)는 각각 조도·RTC·IGN 관측/전원 출력을 담당한다. 현재 보드 revision이 미확정이므로 자동 전원 차단은 활성화하지 않는다.

HAL/CMSIS/FreeRTOS/LVGL/BTstack/USB Device/FatFs 원본은 Drivers 또는 Middlewares의 고정 vendor 패키지다. 프로젝트용 포트·프로토콜·설정을 vendor 파일에 넣지 않는다. [AGENTS.md](../AGENTS.md)의 Cube 재생성 계약과 각 `module.build.json`의 명시적 vendor source 목록을 유지한다.

## 시작 순서와 소유 태스크

strong main task는 계속 `LCDTest();`만 호출한다. Integrated에서는 표시 초기화 뒤 `NoodoeRuntime_Start()`가 DMA·HCI UART의 생성 초기화를 실행하고 I/O·Storage worker를 만든다.

| 소유자 | 실행·소유 내용 |
|---|---|
|Graphics/default task|LVGL/EVE 명령, 호 시험, 버튼/UI, CPU·FPS HUD, 실제 screenshot 처리 지점|
|I/O worker|Dashboard RX→Vehicle, GPS/ELM byte stream→각 parser, PHONE Control, IGN 관측; 대략2ms마다 반복,100ms마다 snapshot 게시|
|Storage worker|SPI5/USB/NOR→Settings 초기화→최초 BT 시작→ALS/RTC/SDRAM→StorageSWD/RuntimeUpdate; 이후 저장·백업·업데이트·설정·센서 poll|
|Bluetooth task|BTstack/HCI/SPP, 요청 큐, peer lifecycle; 최초 Start 전에 검증된 Settings 키를 import|
|IRQ|DMA/UART/USB 완료·오류 및 바이트 수집/통지. NOR DMA priority4에서는 RTOS API를 호출하지 않음|

Control은 RuntimeUpdate 객체가 완전히 초기화되었다는 release/acquire 게시 이후 연결된다. Update의 요청 수락·reply 송신은 I/O, 실제 staging/commit은 Storage가 담당한다. Metadata commit의 HCI 일시 정지는 BT owner에게 요청하고 응답을 기다리는 별도 계약이며 UART 레지스터를 Storage에서 직접 조작하지 않는다.

Buffer 수명도 API별로 구분한다. Bluetooth send와 Dash send는 caller 데이터를 복사하지만 USBBridge send는 완료까지 buffer를 빌린다. Snapshot은 복사본이다. Filesystem/NVM API는 mutex·물리 I/O 대기가 있으므로 그래픽 tick이나 ISR에서 직접 호출하지 않는다. 한 parser context를 여러 태스크가 동시에 Feed하지 않는다.

## PHONE·ELM·GPS 역할

- **PHONE:** incoming SPP 제어 링크. NDCP 조회, 휴대폰 GPS와 미래 APP OTA 경로다. MFi/iAP/BLE/오디오 프로파일은 현재 범위에 포함하지 않는다.
- **ELM:** outbound SPP. 세션 설정 후 지원 Mode01 PID만 질의한다. DTC 삭제·ECU 변경 기능은 제공하지 않는다. 명령은 실제 송신 가능 시점에 가져오며 보내지 못한 명령을 timeout 이후 뒤늦게 전송하지 않는다.
- **GPS:** outbound NMEA SPP. 실제 외부 링크가 UP이면 silent/no-fix/stale여도 외부 source를 유지한다. **외부 링크가 끊길 때만** 휴대폰 source로 바꾼다. 저장된 pairing/binding은 현재 연결 상태가 아니다.

`SendSession/ReceiveSession`은 관측한 UP/CID/주소/reconnects와 현재 링크가 일치할 때만 원자적으로 enqueue/dequeue한다. 재접속 시 parser·이전 응답·현재 연결의 OTA 승인을 폐기한다. Settings binding은 저장된 의도이며 설정 저장 자체가 connect를 실행하지 않는다.

## 저장 배치와 쓰기 권한

| 영역 | NOR byte 범위, 끝 주소 제외 | 권한 |
|---|---|---|
|FAT16 파일|`[0x00000000,0x07F70000)`|일반 runtime permit 또는 host gate|
|설정/NVM journal|`[0x07F70000,0x07F80000)`|동일;16개4KiB 순환 record|
|순정 BL staging|`[0x07F80000,0x07F90000)`|보존; 공개 쓰기 API에서 제외|
|APP staging|`[0x07F90000,0x08000000)`|별도의 명시적 OTA transaction|

최초 장치에서는 전체128MiB의 독립 A/B 백업이 동일한지 검증한 뒤 host unlock→명시적 format→명시적 provisioning을 수행한다. 현재 provisioning queue API는 있지만 transport opcode는 아직 연결하지 않았다. 파일시스템을 mount했다고 자동 format/provision하지 않는다. 정상 marker는 UID/schema/layout/CRC가 맞아야 하며, 다음 부팅에서는 이를 읽어 PC 없이 일반 파일·키 쓰기를 복원한다. **FORMAT은 매번 별도 host gate가 필요하다.** USB reset은 host gate만 닫고 정상 runtime permit은 보존하며, format/provision 기록 진행 중에는 host gate 없이 다음 물리 명령을 실행할 수 없다.

내부 FLASH APP는 `[0x08010000,0x08080000)`다. 일반 SWD APP 설치는 하위64KiB를 보존한다. 명시적 OTA commit만 순정 메타 sector2 `[0x08008000,0x0800C000)`를 사용한다. 순정 설치는 제자리 교체여서 A/B rollback이 없다. host 검증·stage·commit·reset을 분리하고 [Update 계약](Update/README.md)을 따른다.

## 빌드·콘솔·화면 캡처

아래 경로는 프로젝트 루트 기준이다. 빌드는 CubeIDE1.18.1 명령행 환경이며 장치 설치를 포함하지 않는다. Profile 선택은 저장되므로 의도한 profile을 명시한다. Integrated는 호/HUD와 서비스, Graphics는 보존한41개 UI 시험을 선택한다. 생성 후 `build.ps1`가 sync/check와 APP 전용 ELF/BIN/manifest 검증을 수행한다. GUI Generate Code는 사용자가 실행한다.

```powershell
.\tools\build.ps1 -Configuration Release -Profile Integrated
.\tools\build.ps1 -Configuration Debug -Profile Integrated
# 전체 그래픽 시험을 빌드할 때:
.\tools\build.ps1 -Configuration Release -Profile Graphics
```

실행 장치와 **동일한 ELF/manifest**를 선택해야 한다. 아래 STLINK_SN/COM8/새 출력 폴더는 해당 환경 값으로 지정한다. SWD 백업·캡처·다른 디버거를 동시에 실행하지 않는다. 현재 진행 중인 백업에 두 번째 요청자를 붙이지 않는다.

```powershell
# 표시 계획만 출력; --execute를 붙여야 실제 SWD screenshot 요청:
python tools/capture_display.py --manifest Release/app.manifest.json --output ../../analysis/new-capture --page 1

# USB 없는 상태에서도 가능한 읽기 전용 NOR 백업 경로:
python tools/storage_swd_backup.py status --serial STLINK_SN --elf Release/FuckNudo_Noodoe_CFW_Project.elf --directory ../../analysis/new-nor-status --read-khz 100
python tools/storage_swd_backup.py backup --serial STLINK_SN --elf Release/FuckNudo_Noodoe_CFW_Project.elf --directory ../../analysis/new-nor-backup --read-khz 100

# 연결 가능한 USB CDC/SPP host가 있을 때:
python tools/storage_backup.py ports
python tools/storage_backup.py backup --port COM8 --directory ../../analysis/new-usb-backup
python tools/noodoe_control.py --port COM8 info
python tools/noodoe_control.py --port COM8 bt
python tools/noodoe_control.py --port COM8 vehicle
```

[실제 screenshot 도구](../Graphics/CAPTURE.md)는 FT81x 출력480×480 RGB565를 예약 RAM_G에 snapshot하고, 세대/offset/CRC를 확인하며 SWD로 읽는다. CPU를 halt/reset하지 않는다. `--page 1/2/3`은 시험 결과 페이지를 잠시 선택하고 snapshot 직후 복원한다. 패널 유리·백라이트를 촬영한 사진이 아니다. 빌드용 Release 파일은 바뀔 수 있으므로 재현에는 [설치·실측 기록](../../../analysis/2026-09-12-integrated-bringup/README.md)에 보존한 실제 실행 이미지를 우선한다.

## 2026-09-12 문서 작성 시점의 검증 상태

구현·host model 검증과 실제 외부 장치 통신은 별개다. [실측 기록](../../../analysis/2026-09-12-integrated-bringup/README.md)에는 실제 호 fill/drain 및 결과 페이지 캡처, 약29.8–29.9FPS 관측, NOR 읽기 transport, SDRAM 제한 시험, RTC 읽기 진척을 보존했다. 실제 캡처의 원 밖 pixel은 검정으로 확인했다. 전체64MiB RAM 스트레스 시험이나 모든41개 UI 실기 합격을 뜻하지 않는다.

| 대상 | 현재 확인 범위·남은 조건 |
|---|---|
|Bluetooth|HCI 시작 timeout `0x0302`; controller ID/RX 및 실제 SPP peer 통신 미확인|
|조도 ALS|예상 I2C 주소에서 error2; 유효 ID/lux 미확인, 부품 고장으로 단정하지 않음|
|USB CDC|초기화 구현, 현재 host 연결/열거 없음|
|Dashboard/ELM/GPS|현재 외부 peer 입력 없음; parser·단위·세션 경계는 host fixture로 검증|
|NOR 전체 백업|SWD A/B 각128MiB 전체 읽기 완료, SHA256 `970af11e42f6c148c59f2ca99dec8fba1f9552a2def1405a43f50d3d1c98156f` 일치. 이후 순정 비교 실행 뒤 NOR 변화 여부는 아직 전 범위 재검사하지 않음|
|FS/Settings/OTA|실제 format/provision/APP staging/commit/reset 미수행; 초기 상태는 쓰기 잠김|

핵심 회귀 진입점은 `tools/tests/control_host/run.py`(31), `dash_irq_host/run.py`(23), `nor_gate_host/run.py`(35), `storage_journal_host/run.py`(155), `settings_host/run.py`(579)의 각 O0/Os 모델과 `tools/tests/protocol_services/README.md`의 프로토콜/업데이트 시험이다. 괄호는 현 버전 assertion 수이며 실측 성공 횟수가 아니다. 호스트 CLI 검증은 `test_noodoe_control.py`, `test_capture_display.py`, `test_storage_backup.py`를 참고한다. 미연결 입력을 합성 PASS로 바꾸거나 이 결과를 전체 통합 완료로 표현하지 않는다.
