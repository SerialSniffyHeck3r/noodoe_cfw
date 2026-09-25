# Noodoe firmware update architecture and OpenNoodoe gap analysis

Date: 2026-09-01 KST

이 문서는 `com.noodoe.sunray 2.1.14` 순정 APK와 보존된 진단 APK를 정적
분석하고, 그 결과를 현재 `android/OpenNoodoe` 구현과 대조한 기록이다. 이
조사 중 바이크로 펌웨어, 부트로더, OQC 쓰기 명령을 보내지 않았다.

## 결론

1. 순정 앱의 Noodoe 장치 분류는 두 종류가 아니라 `INVALID`,
   `VERSION_1_0`, `VERSION_1_5`, `VERSION_2_0` 네 값이다.
2. 펌웨어 전송 구현이 근본적으로 갈라지는 경계는 `1.0` 대 `1.5 이상`이다.
   `1.5`와 `2.0`은 같은 현대식 SPP 파일 전송 상태기계를 사용한다.
3. 실차에서 읽은 `protocol 0.0 / HW 0 / firmware 5.16`은 순정 판별식상
   `VERSION_1_5`다. 따라서 현재 AK550은 Nordic DFU가 아니라 현대식 SPP
   펌웨어 위치 `0x0800` 경로가 정상 후보이다.
4. APK에 Nordic DFU와 ST OTA 구현이 실제로 들어 있지만, Sunray 생산 앱의
   펌웨어 UI에서 이 API를 부르는 호출 지점은 발견되지 않았다. 이는 포함된
   범용 BLE 라이브러리 기능이지 현재 Noodoe 계기판 업데이트 경로라는 증거가
   아니다.
5. 후속 실차 캡처와 V5.16 펌웨어 어셈블리 분석으로 `0x0D FILE_TRANSFER`의
   16-byte command reply가 실제로 존재함이 확인됐다. 순정 Android 앱이 이 응답을
   다음 청크의 진행 조건으로 사용하지 않는 것과 계기판이 응답을 만들지 않는 것은
   다른 사실이다. OpenNoodoe 0.4.0의 직접 원인은 응답 대기 자체가 아니라 offset
   8/12 진행 필드를 잘못 해석한 것이었고, 0.4.1에서 수정됐다.
6. OpenNoodoe 0.4.1의 일반 파일 전송은 실차에서 동작했지만 펌웨어 업데이트는
   resume, 장시간 상태기계, 패키지 호환성, 부트로더 설치 및 복구가 검증되지
   않았다. 펌웨어 설치 버튼은 별도 검증 전까지 사용하면 안 된다.

## 증거 등급

- **확정 - 정적**: APK 코드에서 직접 확인한 분기, 바이트 구성, 상태 전이.
- **확정 - 실차**: 현재 AK550과의 이전 통신에서 실제로 읽거나 동작을 확인한 값.
- **추정**: 파일명, 바이너리 모양 또는 포함 라이브러리로부터의 가설.
- **미확인**: 실제 플래시 설치, 재부팅, 전원 차단 복구처럼 희생 가능한 계기판이나
  공식 업데이트 캡처가 필요한 항목.

## 1. 장치 세대 판별

순정 APK의 열거형은 다음 순서다.

```text
INVALID, VERSION_1_0, VERSION_1_5, VERSION_2_0
```

`ScooterDeviceInfo.getDeviceType()`의 실질적인 판별은 다음과 같다.

```text
if deprecated:
    VERSION_1_0
else if protocol == 0.0:
    VERSION_1_5
else if protocol is not accepted as 2.x:
    INVALID
else:
    VERSION_2_0
```

`deprecated` 조건은 `protocol 0.0`, `HW 0`, `firmware major < 4`다. 디컴파일된
`major == 4 && minor < 0` 조건은 unsigned로 읽는 minor에 대해 실질적으로
도달할 수 없다.

### 현재 AK550

| 필드 | 실차 값 | 순정 앱 해석 |
| --- | ---: | --- |
| protocol | `0.0` | 1.5 후보 |
| hardware | `0` | deprecated 판별에 사용 |
| firmware | `5.16` | major가 4 이상이므로 deprecated 아님 |
| 최종 분류 | | `VERSION_1_5` |

이 결과는 차량 연식이나 마케팅 이름이 아니라 계기판이 보고한 버전 튜플로
판정한 것이다. 같은 2017 AK550이라도 펌웨어가 4 미만이면 `VERSION_1_0`으로
분기할 수 있다.

### OpenNoodoe와의 차이

현재 `DeviceInfo.supportsFramedProtocol()`은 다음 조건이다.

```java
protocolMajor >= 1
        || (protocolMajor == 0 && (hardwareVersion != 0 || firmwareMajor >= 4))
```

현재 AK550을 현대식으로 고르는 결과는 맞다. 그러나 다음 문제가 있다.

- 공식 앱이 `INVALID`로 거부할 protocol `1.x`를 현대식으로 허용한다.
- `1.0`, `1.5`, `2.0`을 보존하지 않고 boolean 하나로 축약한다.
- v2 OTA 조회에 필요한 hardware ID와 세대별 정책을 표현할 수 없다.
- 버전이 불완전하게 읽힌 경우 fail-closed가 아니라 잘못된 현대식 경로를 선택할
  가능성이 있다.

## 2. 생산 앱의 전체 업데이트 흐름

사용자가 순정 앱에서 펌웨어 업데이트를 누르면 대략 다음 호출 흐름을 탄다.

```text
FirmwareDownloadPresenter
  -> OTAManager.checkNewFWVersionOnServer / checkFWAndUpdate
  -> OTAClient query + firmware/resource download

FirmwareUpdatePresenter.triggerToSyncNewFirmware
  -> TransmissionService(ACTION_UPDATE_FIRMWARE)
  -> initTransmitter(device type)
  -> transmitter.triggerInstall()
```

`TransmissionService.initTransmitter()`는 다음처럼 구현을 선택한다.

```text
VERSION_1_0            -> backward.device_1_0.Transmitter
VERSION_1_5/2_0        -> Transmitter_1_5
INVALID                -> 연결 종료
```

현대식 `Transmitter_1_5.handleTriggerInstall()`은 펌웨어만 독립적으로 보내지 않는다.
다음 위치의 모델을 준비하고 태스크를 시작한 다음 위치별로 파일을 직렬 전송한다.

```text
FIRMWARE, RESOURCE, CLOCK, WEATHER, SPEEDOMETER,
POI, GALLERY, GROUP, 그리고 VERSION_2_0에서 AUDIO
```

즉, 순정 앱의 펌웨어 업데이트 화면은 다운로드한 펌웨어와 필수 리소스를 포함한
전체 동기화 파이프라인에 진입한다. `isResourceReadyForFWUpgrade`도 이 과정의
일부다. OpenNoodoe처럼 임의의 `.bin` 하나를 바로 보내는 흐름과는 안전 조건이
다르다.

## 3. VERSION_1_0: 구형 raw SPP OTA

이 경로도 APK 코드상 Classic Bluetooth SPP를 사용한다. BLE 전송이라고 확정할
근거는 없다. 차이는 무선 매체보다 상위 프로토콜과 주도권이다.

### 상태기계

```text
앱 -> 계기판: NOTICE_FILE_CHANGE (0x0A) + 4 zero bytes

계기판 -> 앱: REQUEST_INDEXED_CONFIG_FILE (0xC4), page 0
앱 -> 계기판: 0x44 page response
               page + 32-byte firmware filename + u32 size + type

계기판 -> 앱: REQUEST_FILE (0xC5)
               filename + u32 already_received_length
앱 -> 계기판: 0x45 file header + raw remaining bytes
```

중요한 특징은 계기판이 파일 목록과 파일을 요청한다는 점이다. 앱은
`OtaQueryState`와 `OtaFileTransferState`에서 응답하고, 계기판이 보낸
`already_received_length`만큼 파일 스트림을 건너뛰어 재개한다.

### 구현 의미

- 현대식 `0x0A/0x0B/0x0D` framed 명령을 보내는 것으로는 1.0을 업데이트할 수
  없다.
- 구형 장치를 강제로 최신화한 뒤 현대식 기능만 제공하려 해도, 최소한 이 legacy
  OTA 응답 상태기계는 별도 복구 도구에 구현해야 한다.
- 모든 구형 Noodoe 기능을 구현할 필요는 없지만, bootstrap 판별, 파일 조회 응답,
  resume, 오류 응답, 완료 추적은 생략할 수 없다.

## 4. VERSION_1_5와 VERSION_2_0: 현대식 SPP OTA

두 세대는 동일한 `Transmitter_1_5`, `FWTransmitModel`, `BTOtaApiHandler`,
`SunrayTransferTaskManager`, `SunrayTransferFileTaskHandler`를 사용한다.

### 펌웨어 태스크

- location: `FIRMWARE = 0x0800`
- transfer type: `FILE = 2`
- 파일 수: 1
- firmware content ID: major와 minor를 각각 u16 little-endian으로 만든 4바이트
  값을 hex 문자열화하고 negotiate의 16바이트 ID 영역에 zero-pad
- 실제 파일 identity: 16바이트 영역 첫 u16에 file ID, 펌웨어는 ID `1`
- 무결성 필드: 전체 파일 CRC32와 u32 파일 크기

### 명령 구조

#### `0x0A FILE_TRANSFER_NEGOTIATE`

```text
u16 task_id
u16 transfer_type
u16 location
u32 total_size
u8  attribute
u8  content_id[16]
```

attribute는 `BEGIN=1`, `CONTINUE=2`, `DONE=3`, `REMOVE=4`, `RESET=5`,
`CANCEL=6`이다.

#### `0x0B FILE_TRANSFER_CONTROL`

```text
u16 task_id
u16 operation              # UPDATE=1, UPDATE_TERMINATE=2
u16 transfer_id
u8  identity[16]           # firmware는 첫 u16=file ID, 일반 파일은 MD5
u32 crc32
u32 size
u16 path_length
u8  path[path_length]
```

START 성공 응답에는 `receivedLength`가 포함된다. 순정 앱은 0보다 크고 파일
크기보다 작으면 그 위치로 스트림을 이동한 뒤 전송을 재개한다.

#### `0x0D FILE_TRANSFER`

```text
u16 task_id
u16 transfer_id
u16 data_type              # 1
u8  data[]
```

순정 앱은 데이터 최대 길이를 `floor4(11818) = 11816`바이트로 잡는다. 마지막
청크만 더 짧을 수 있다.

### ACK와 응답의 구분

이 구분과 함께, 계기판이 두 종류의 완료 신호를 모두 낸다는 점이 중요하다.

- sequence ACK: 프레임이 SPP/sequence 계층에서 전달됐다는 확인.
- command reply: 계기판 명령 처리 결과와 상태를 담은 별도 명령 프레임.

순정 `InputCommandParser`는 들어온 `0x0D`를 다음 청크 진행에 사용하지 않고,
송신 성공 콜백 `onOutputDataSuccess(FILE_TRANSFER)`이 다음 데이터 청크를
준비시킨다. 그러나 이것은 계기판 응답의 부재를 뜻하지 않는다. V5.16의
`0x080290FE` 핸들러는 16-byte `0x0D` reply를 구성하며, 실차 캡처에서 offset 8은
이번에 수락한 청크 길이, offset 12는 누적 수신 길이로 확인됐다. 재개용
`receivedLength`는 별도로 `0x0B` START 응답에서 읽는다.

### 복구와 장시간 전송

순정 상태기계에는 다음이 더 있다.

- 20초 negotiate timeout
- 태스크가 STARTED인 동안 10초 주기 `CONTINUE`
- meter-reported `receivedLength` 기반 resume
- `RESET`, `CANCEL`, `REMOVE` 상태
- file exists, busy, not found, delete error 등 응답별 분기
- 파일별 retry queue와 위치별 직렬화
- 전송 중 disconnect와 stop 처리

이들은 장시간 걸리고 전원 손실 위험이 있는 펌웨어 업데이트에서 부가 기능이
아니라 필수 복구 메커니즘이다.

## 5. VERSION_2_0에서 실제로 달라지는 부분

정적 코드에서 확인된 차이는 다음과 같다.

- OTA 서버 조회가 v3 DTO 대신 v4/v2.0 DTO를 사용한다.
- v2.0 series 조회에는 hardware ID가 추가된다.
- full sync에 `AUDIO` location이 포함된다.
- protocol 2.x부터 허용되는 추가 명령과 상태 조회가 있다.
- 보존된 v2.0 펌웨어 파일은 917480바이트로, v1.x 계열 458748바이트와 형식과
  메모리 맵이 다르다.

그러나 펌웨어 전송 자체는 여전히 같은 `FIRMWARE 0x0800` 현대식 상태기계를
사용한다. 따라서 “v1과 v2는 업데이트 구현이 완전히 다르다”는 말은 다음처럼
정리해야 정확하다.

- `VERSION_1_0` 대 `VERSION_1_5/2.0`: 전송 프로토콜이 완전히 다름.
- `VERSION_1_5` 대 `VERSION_2_0`: 서버 조회와 이미지/호환성 정책은 다르지만
  SPP 파일 전송 상태기계는 같음.

## 6. Nordic DFU와 ST OTA

APK의 범용 `com.noodoe.fwk.ble.v02` 라이브러리는 다음 API를 포함한다.

```text
CMUOtaLibrary.triggerNordicDfu(...)
CMUOtaLibrary.triggerStOta(...)
CMUOtaLibrary.sendNordicDeviceImage(...)
CMUOtaLibrary.sendSTDeviceImage(...)
```

Nordic 타입은 다음 세 값이다.

```text
NOODOE_1_BLOCK, NOODOE_2_BLOCK, WATCH
```

이는 `DEVICE_TYPE.VERSION_1_0/1_5/2_0`과 다른 열거형이다. 이름이 비슷하다는
이유로 같은 세대 분류로 연결하면 안 된다.

### Nordic handler의 실제 동작

1. 범용 NDBT GATT 서비스로 대상 장치에 연결한다.
2. `TASK_TYPE_DFU_MODE`를 content provider task queue에 넣는다.
3. DFU mode 진입 성공 후 연결을 끊는다.
4. 5초 뒤 Nordic Android DFU service를 시작한다.
5. `NOODOE_1_BLOCK`은 같은 MAC에 raw application image를 보낸다.
6. `NOODOE_2_BLOCK`과 `WATCH`는 마지막 MAC byte가 `+1`인 주소에 ZIP package를
   보낸다.
7. 실패 시 제한된 재시도 로직을 탄다.

### 호출 가능성 판정

생산 APK와 진단 APK의 decompiled source 전체에서 위 네 public API의 직접
호출이나 문자열 기반 reflection 호출은 찾지 못했다. 확인된 참조는 API 정의,
handler 내부 재호출, task plumbing뿐이다.

따라서 현재 결론은 다음과 같다.

- **확정**: Nordic/ST 업데이트를 수행할 수 있는 범용 라이브러리가 APK에 포함됨.
- **확정**: 생산 firmware UI는 SPP `TransmissionService` 경로를 호출함.
- **미확인**: 해당 Nordic 경로가 다른 제품, 과거 제품, 개발 도구 중 어디에
  사용됐는지.
- **근거 없음**: 현재 AK550 계기판 펌웨어 업데이트가 Nordic DFU를 호출한다는
  주장.

실차 MCU를 Nordic이라고 결론 내릴 수도 없다. 보존된 AK/SR1.5 이미지 시작은
Cortex-M vector table과 맞지만 제조사/정확한 MCU는 아직 식별하지 않았다.

## 7. 보존 펌웨어 바이너리에서 확인한 것

현재 아카이브의 firmware는 두 크기 군으로 나뉜다.

| 계열 | 크기 | 예 |
| --- | ---: | --- |
| SR1 / NewAK / SR1.5 | 458748 | `SR1.5_ota_V516.bin` |
| SR2.0 | 917480 | `SR2.0_application_ota_*.bin` |

### v1.x 계열

- offset 0에 SRAM `0x200...` stack pointer와 Thumb reset vector가 있다.
- 파일 끝 4바이트가 버전별로 달라지는 trailer/checksum 후보다. 이 값은 표준
  `CRC32(file[:-4])`와 일치하지 않았으므로 알고리즘은 미확인이다.
- 현재 AK 서버가 돌려준 정확한 파일은 series 1, firmware 5.16,
  `1657088080998-s1-SR1.5_ota_V516.bin`이다.

### v2.0 계열

- offset 0 stack pointer가 `0x240...` 영역이다.
- 파일 끝 8바이트 중 앞 4바이트는 아카이브된 모든 SR2.0 이미지에서
  `CRC32(file[:-8])`와 정확히 일치한다. 뒤 4바이트는 little-endian 버전이다.
  예를 들어 0.49는 `31 00 00 00`, 0.53은 `35 00 00 00`, 1.24는
  `18 00 01 00`이다.
- 정확히 두 개의 동일한 절반으로 구성되지는 않는다. 크기가 v1.x의 약 두 배라는
  사실만으로 Nordic `NOODOE_2_BLOCK`과 연결할 수 없다.

SR2.0 CRC와 version trailer는 재현 검증됐다. SR1.x trailer와 bootloader가 실제로
어떤 검증을 수행하는지는 여전히 미확인이다.

재현 명령:

```powershell
python inspect_firmware.py ..\..\artifacts\ota-archive\2026-08-31-full\blobs\firmware
```

스크립트는 파일을 읽기만 하며 아카이브를 수정하지 않는다.

## 8. 현재 OpenNoodoe 비교

| 항목 | 순정 앱 | OpenNoodoe | 판정 |
| --- | --- | --- | --- |
| transport | Classic SPP/RFCOMM | Classic SPP/RFCOMM | 일치 |
| 세대 분류 | 1.0/1.5/2.0/invalid | modern boolean | 불충분 |
| AK 5.16 선택 | 1.5 | modern | 결과 일치 |
| firmware location | `0x0800` | `0x0800` | 일치 |
| negotiate layout | 27-byte payload | 동일 | 일치 |
| file control | ID/CRC32/size/path | 동일 구조 | 대체로 일치 |
| chunk data | 11816 bytes | 11816 bytes | 일치 |
| data completion | sequence ACK로 진행, meter는 16-byte reply도 송신 | 16-byte reply 검증 | 동작 확인, 순정과 진행 조건은 다름 |
| resume | START reply receivedLength | 무시하고 offset 0 | 누락 |
| keepalive | 10초 CONTINUE | 없음 | 누락 |
| timeout/state | 20초와 세부 상태 | 단순 6초 timeout | 불충분 |
| cancel/reset | 구현 | 없음 | 누락 |
| package source | OTA metadata와 series | 임의 파일 + 수동 버전 | 위험 |
| compatibility | device/resource/series gate | vector와 downgrade 검사 | 불충분 |
| v1.0 OTA | raw meter-driven state machine | 없음 | 미구현 |
| v2 image check | `0x240...` image 허용 | `0x200...`만 허용 | v2를 거부 |
| final install | 위치 DONE 후 meter 처리 | DONE을 accepted로 표시 | 설치 확인 없음 |

### 현재 코드의 구체적인 위험

1. `transferOne()`은 순정 앱과 달리 `0x0D` command reply를 다음 청크의 진행
   조건으로 사용한다. 응답 자체는 실재하며 0.4.1의 offset 8/12 해석은 실차와
   일치하지만, reply 유실 시 sequence ACK만으로 복구하는 정책은 없다.
2. START control reply의 `receivedLength`를 읽지 않는다. 중단 뒤 0부터 다시 보내
   meter 상태와 어긋날 수 있다.
3. protocol `1.x`를 현대식으로 허용한다. 공식 앱은 이를 INVALID로 분류한다.
4. 선택 파일을 공식 server response/manifest와 묶지 않는다. series, hardware,
   resource dependency가 빠져 있다.
5. v2.0 공식 이미지의 `0x240...` stack pointer를 잘못된 이미지로 거부한다.
6. transfer DONE은 meter가 실제 플래시 설치와 검증을 끝냈다는 뜻이 아니다.
7. 저전압, ignition off 시점, 재부팅, rollback/bootloader recovery가 검증되지 않았다.

## 9. 안전한 구현 순서

### Phase A: 즉시 할 일

1. 현재 firmware write를 non-operational로 표시하고 실제 전송을 막는다.
2. `DeviceGeneration`을 `INVALID/1_0/1_5/2_0`으로 구현하고 순정 판별식을 그대로
   unit test한다.
3. read-only package inspector에서 archive manifest, SHA-256, series, version,
   vector map, trailer 후보를 보여준다.
4. 실차 tuple과 package tuple이 완전히 일치하지 않으면 fail-closed한다.

### Phase B: 공통 현대식 전송기 수정

1. sequence ACK와 command reply future를 타입 수준에서 분리한다.
2. `0x0D` reply의 task/transfer/chunk/cumulative 필드를 검증하되, sequence ACK와
   reply 유실을 구분하는 timeout/retry 정책을 구현한다.
3. `0x0B` START reply의 task ID, transfer ID, receivedLength를 검증하고 resume한다.
4. negotiate 20초 timeout과 10초 CONTINUE를 구현한다.
5. CANCEL/RESET 및 disconnect cleanup을 구현한다.
6. 일반 gallery/resource 전송으로 먼저 회귀 테스트한다.

### Phase C: 현대식 firmware dry run

1. archived official file만 선택 가능하게 한다.
2. 전송할 모든 frame과 예상 상태 전이를 파일로 기록하되 실제 socket write는 하지
   않는 dry-run 모드를 만든다.
3. 순정 앱이 생성한 trace와 byte-for-byte 비교한다.
4. FIRMWARE BEGIN 직전 다시 device info/OQC read-only snapshot을 저장한다.

### Phase D: 실제 현대식 업데이트 검증

1. 주력 차량이 아닌 복구 가능한 계기판을 사용한다.
2. 공식 앱 업데이트 한 회를 HCI snoop과 앱 로그로 먼저 캡처한다.
3. 안정된 전원, ignition 절차, timeout, disconnect, 재연결, 설치 완료 신호를
   문서화한다.
4. 같은 version reinstall 또는 vendor-approved update로 한 번만 비교한다.
5. 플래시 후 device info, resource version, 기능, 재부팅을 독립 검증한다.

### Phase E: VERSION_1_0 전용 복구 도구

현대식 앱에 legacy를 섞기보다 별도 화면/별도 도구로 구현한다. meter-driven 파일
요청만 처리하고 일반 Noodoe 기능은 제공하지 않는다. old unit을 강제 업데이트할
정책은 이 도구와 복구 절차가 완성된 뒤에만 가능하다.

## 10. 다음 실차 기록

펌웨어를 보내지 않고도 다음을 수집할 수 있다.

1. 연결 직후 raw device-info payload와 분류 결과.
2. firmware/resource/protocol/hardware/bootloader 버전.
3. 순정 앱 OTA query request/response와 다운로드 파일 SHA-256.
4. 순정 앱의 firmware download 완료부터 install 시작 직전까지 logcat.
5. 가능하면 실제 공식 업데이트가 존재하는 복구 가능한 장치에서 HCI snoop.

실제 업데이트 캡처에서는 최소한 다음 이벤트를 타임라인으로 기록한다.

```text
0x0A BEGIN reply
0x0B START request/reply and receivedLength
each 0x0D sequence ACK timing
0x0B UPDATE_TERMINATE request/reply
0x0A CONTINUE and DONE
SPP disconnect/reconnect
meter reboot/install duration
post-install device info
```

## 11. 정적 근거 파일

- 장치 enum:
  `analysis/jadx/com.noodoe.sunray_2.1.14/sources/com/noodoe/sunray/cmu/service/connection/handler/listener/ISunrayBTConnectionManager.java`
- 장치 판별:
  `analysis/jadx/com.noodoe.sunray_2.1.14/sources/com/noodoe/sunray/cmu/service/task/utils/CommonStruct.java`
- transmitter 선택:
  `analysis/jadx/com.noodoe.sunray_2.1.14/sources/com/noodoe/sunray/communication/TransmissionService.java`
- 현대식 전체 sync:
  `analysis/jadx/com.noodoe.sunray_2.1.14/sources/com/noodoe/sunray/communication/transmitter/Transmitter_1_5.java`
- 현대식 firmware model:
  `analysis/jadx/com.noodoe.sunray_2.1.14/sources/com/noodoe/sunray/communication/transmitter/model/FWTransmitModel.java`
- 현대식 file state machine:
  `analysis/jadx/com.noodoe.sunray_2.1.14/sources/com/noodoe/sunray/cmu/service/task/SunrayTransferFileTaskHandler.java`
- command parser/serializer:
  `analysis/jadx/com.noodoe.sunray_2.1.14/sources/com/noodoe/sunray/cmu/service/task/InputCommandParser.java`
  `analysis/jadx/com.noodoe.sunray_2.1.14/sources/com/noodoe/sunray/cmu/service/task/OutputCommandProcessor.java`
- 구형 OTA:
  `analysis/jadx/com.noodoe.sunray_2.1.14/sources/com/noodoe/sunray/communication/transmitter/backward/device_1_0/OtaQueryState.java`
  `analysis/jadx/com.noodoe.sunray_2.1.14/sources/com/noodoe/sunray/communication/transmitter/backward/device_1_0/OtaFileTransferState.java`
- OTA v3/v4 selection:
  `analysis/jadx/com.noodoe.sunray_2.1.14/sources/com/noodoe/sunray/ota/OTAClient.java`
- Nordic/ST library:
  `analysis/jadx/com.noodoe.sunray_2.1.14/sources/com/noodoe/fwk/ble/v02/library/CMUOtaLibrary.java`
  `analysis/jadx/com.noodoe.sunray_2.1.14/sources/com/noodoe/fwk/ble/v02/library/handler/NordicDFUHandler.java`
- 현재 OpenNoodoe:
  `android/OpenNoodoe/app/src/main/java/io/opennoodoe/app/NoodoeService.java`
  `android/OpenNoodoe/app/src/main/java/io/opennoodoe/app/OtaPackageInspector.java`
  `android/OpenNoodoe/app/src/main/java/io/opennoodoe/app/protocol/DeviceInfo.java`
  `android/OpenNoodoe/app/src/main/java/io/opennoodoe/app/protocol/FileTransferPayloads.java`

## 현 시점 사용 금지 경계

다음은 아직 검증되지 않았으므로 현재 AK550에 실행하지 않는다.

- OpenNoodoe의 firmware install
- 임의 또는 다른 series firmware 전송
- Nordic DFU mode 진입
- ST OTA mode 진입
- legacy device에 modern firmware command 전송
- ignition/power-loss 복구가 없는 상태의 resource/firmware write

현재 안전하게 가능한 것은 read-only device/OQC 정보 수집, OTA metadata와 파일
아카이빙, dry-run frame 생성, 순정 앱 trace 비교까지다.
