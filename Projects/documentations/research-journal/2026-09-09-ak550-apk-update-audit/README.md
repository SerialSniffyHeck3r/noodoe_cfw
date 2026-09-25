# AK550 순정 APK 업데이트·부트 관련 재검증

작성: 2026-09-09 KST. 대상: 보존된 `com.noodoe.sunray 2.1.14` APK와 디컴파일 소스. 실물에 명령을 보내지 않았으며 앱이나 펌웨어를 수정하지 않았다.

## 핵심 결론

1. **순정 앱의 업데이트 기능은 휴대전화에 있는 파일을 Noodoe로 보내는 기능이다. 현재 조사에서 Noodoe의 내부 플래시를 주소·길이로 읽어 휴대전화에 저장하는 구현은 확인하지 못했다.** 따라서 이 APK의 OTA 기능을 부트로더 백업 수단으로 취급할 근거가 없다. 이는 모든 미공개 공장 명령의 부재를 증명한다는 뜻은 아니다.
2. `BASIC_FIRMWARE_UPGRADE (0x10)`와 `FILE_QUERY (0x03)`라는 이름은 실제 열거형에 존재하지만, 생산 앱의 현대식 송신·수신 처리기에서는 두 명령을 **invalid**로 처리한다. 이름만 보고 부트로더 진입이나 덤프 API로 해석하면 안 된다.
3. 과거 실차에서 얻었다는 `protocol 0.0 / HW 0 / FW 5.16`을 순정 앱 판별식에 넣으면 `VERSION_1_5`다. **2017년식 도너의 현재 버전 튜플은 아직 읽지 않았으므로, 같은 연식·모델명으로 이를 대입해서는 안 된다.** `protocol 0.0 / HW 0 / FW major < 4`는 `VERSION_1_0` 경로다.
4. 순정 앱은 차량 이름만 보고 `.bin`을 고르지 않는다. 장치의 세대와 language/motor/resource/dashboard 튜플로 서버에서 series를 얻고, 그 series로 펌웨어·리소스 버전을 조회한다. 여러 차종의 모듈 외관이 같다는 관찰은 이미지 교환 가능성의 증거가 아니다.
5. 보존된 별도 진단 APK의 UART 화면은 **외부 UART 테스트 도구를 연결해 속도 전달을 확인하라는 안내와 수동 합격/불합격 기록 화면**이다. 이 화면에서 MCU 부트로더 읽기 또는 UART 플래시 업로드를 수행하는 코드는 확인되지 않았다.

## 1. 실제 APK와 새 분석 산출물의 연결

이번에 직접 읽은 APK:

- `evidence/apk/com.noodoe.sunray_2.1.14/base.apk`
- 크기: 95,475,801 bytes
- SHA-256: `6c9c82aebfcbce221c68530c731f9c469db6d7fdba30439b5549a0e239c79e80`
- DEX 6개 전부에 대해 내장 Adler-32와 SHA-1 필드를 재계산하여 일치 확인.
- 관련 클래스가 들어 있는 `classes6.dex`: 10,043,596 bytes, SHA-256 `c5b45470fd203ab93f5d55e1c5aa670d89838549bdb5a0d85b34c6bfc76dc9f2`.

`audit_apk.py`는 실제 ZIP의 DEX를 읽고 CommonStruct, OutputCommandProcessor, InputCommandParser, OTAClient, FWTransmitModel의 클래스 descriptor를 확인했다. 다섯 클래스는 이 DEX에서 JADX 1.5.2 CLI로 새로 디컴파일했다. 원래 소스와 새 소스의 전체 텍스트 hash는 같지 않다. 단일 DEX 입력, 메서드·내부 클래스 배치, 변수명, 상수 치환, 분기 표현 때문에 결과가 달라지며 **이번 검증은 텍스트 동일성 주장이 아닌 핵심 분기·필드 처리의 재확인**이다.

기본 모드의 `InputCommandParser.handleReceivedCommand`는 큰 메서드 디컴파일 실패로 본문을 생략한다. `--show-bad-code`를 추가한 `fresh-jadx/InputCommandParser-show-bad-code.java`에 경고를 유지한 본문이 있다. 해당 입력 분기는 기존 소스 및 enum switch mapping과 함께 확인했으며, 디컴파일 경고를 원본 프로그램의 예외 발생으로 해석하지 않는다. 출력 거부 분기는 일반 모드에서 재확인됐다.

생산 디컴파일 소스는 Java 파일 21,130개이며, 그중 `com/noodoe` 3,208개에 대한 한정된 UART/read-memory/read-flash/boot-mode 이름 검색은 일치 행 0개였다. 이 검색은 보조 근거다. 이름 검색만으로 간접 호출·수치 명령·네이티브 코드·펌웨어 전용 명령의 부재를 보장하지 않는다. APK 서명 인증서의 제작사 신뢰성은 이번에 새로 검증하지 않았다.

재현 데이터: `apk-provenance.json`, `fresh-jadx-results.json`, `source-scan-summary.json`, `source-evidence.md`, `source-evidence-index.json`.

## 2. AK550 세대와 이미지 선택

새 출력 `fresh-jadx/CommonStruct.java:395–427`의 분기:

```text
protocol=0.0 AND HW=0 AND FW.major<4 -> VERSION_1_0
그 외 protocol=0.0                 -> VERSION_1_5
protocol.major=2, minor>=0          -> VERSION_2_0
protocol.major>2                   -> VERSION_2_0
그 외                              -> INVALID
```

소스에는 `FW.major == 4 && FW.minor < 0`도 있다. 해당 version 값의 정상 unsigned 파싱 범위에서 의미 있는 분기는 아니다. 기존 9월 1일 문서의 '2.x 허용'이라는 요약은 **major > 2도 VERSION_2_0으로 수용**한다는 점을 생략했다. AK 0.0/5.16 분류 결과에는 영향이 없다.

`fresh-jadx/OTAClient.java:41–69`에서 v2.0 계열만 `OTASeriesQueryRequestV20`/`FWQueryRequestV20`을 사용한다. v1.0과 v1.5는 v3 request DTO를 사용한다. 원래 소스 `ota/OTAInterface.java:19–33`은 다음 URL suffix를 선언한다.

| 세대 | series | firmware | resource |
| --- | --- | --- | --- |
| 1.0/1.5 | v3/querySeries | v3/queryFirmware | v3/queryResource |
| 2.0 이상 분류 | v4/querySeries | v4/queryFirmware | v4/queryResource |

기본 URL은 `ServerConstant.java:8`의 `http://nars.noodoe.com/`이다. 이번 조사에서는 서버에 요청하지 않았다. `OTAManager.java:504–511`은 querySeries 결과의 firmware_series_id/resource_series_id를 queryFirmware로 넘긴다. 그 응답의 `downloadUrl`을 다운로드하며, 앱에 특정 AK550 `.bin` 한 개가 고정된 구조가 아니다.

v3 series 입력: `language_packet, motor_series, resource_id, default_dashboard_id`. v4는 `hardware_id`도 포함한다. 도너 정상 부팅 후 먼저 확보할 앱 측 자료는 protocol/HW/FW/resource/bootloader version과 OQC의 이 튜플이다. 기존 실차 값과 도너 값을 별도로 보관해야 한다.

## 3. 생산 앱의 실제 업데이트 경로

원래 생산 소스 상대 기준 경로는 `analysis/jadx/com.noodoe.sunray_2.1.14/sources/com/noodoe/sunray/`이다. 필요한 모든 발췌는 `source-evidence.md`에 파일 hash와 함께 묶었다.

```text
settings/firmware/FirmwareUpdatePresenter.java:25
  ACTION_UPDATE_FIRMWARE
communication/TransmissionService.java:178
  VERSION_1_0 -> backward.device_1_0.Transmitter
  VERSION_1_5/2_0 -> Transmitter_1_5
communication/transmitter/Transmitter_1_5.java:422
  FIRMWARE, RESOURCE, CLOCK, WEATHER, SPEEDOMETER,
  POI, GALLERY, GROUP (+ VERSION_2_0의 AUDIO)
communication/transmitter/model/FWTransmitModel.java
  새 버전에 맞는 휴대전화 파일을 준비 -> 파일 1개 전송
cmu/library/version_1_5/BTOtaApiHandler.java:36
  startInstallTask(FIRMWARE,...)
cmu/service/task/SunrayTransferFileTaskHandler.java
  negotiate/control/data/terminate/done
```

현대식 transfer의 location `FIRMWARE=2048 (0x0800)`은 **프로토콜상의 논리적인 위치 ID**다. 이것을 MCU 플래시 주소 `0x08000000` 또는 실제 플래시 offset으로 바꾸어 해석하지 않는다. 논리적인 위치를 Noodoe 펌웨어가 저장 영역으로 매핑하는 부분은 별도의 바이너리 분석 대상이다.

새 `FWTransmitModel.java:68–107,207–238`은 파일 존재 확인, 버전 기반 content ID, 파일 하나의 byte length, 전송 API 호출을 재확인한다. `FWOTAInfo.isResourceReady`를 계산·보관하지만 **현대식 startTask 자체는 이 필드를 조건으로 검사하지 않는다.** 리소스가 관련되어 있다는 사실과 해당 필드가 모든 경로를 차단하는 안전장치라는 주장은 구분해야 한다. legacy OtaQueryState/OtaFileTransferState에는 명시적인 `isResourceReadyForFWUpgrade` gate가 있고, TransmissionService에도 별도 리소스 확인 루틴이 있다.

기존 분석과 일치하는 현대식 상태기계 세부:

- `SunrayTransferFileTaskHandler.java:42–49`: CONTINUE 10초, negotiate timeout 20초, 최대 data 단위 상수 11818 및 4-byte 정렬 처리.
- `:689–699`: control reply `receivedLength`가 부분 수신 범위이면 파일 스트림을 해당 위치만큼 skip하여 재개.
- `:997–1002`: 진행 중 CONTINUE를 주기적으로 예약.
- `:1013–1040`: `java.util.zip.CRC32`, 매 read chunk를 필요 시 4-byte 배수로 0-padding하여 checksum. `totalSize`는 padding 전 실제 파일 길이.
- `:1198` 이후: FILE_TRANSFER 송신 성공 콜백이 다음 데이터 단계로 진행. 앱이 0x0D command reply를 진행 조건으로 소비하지 않는다는 사실은 Noodoe 펌웨어가 해당 reply를 생성하지 않는다는 뜻이 아니다.

**여기의 CRC는 전송용 checksum이다.** 부모 분석에서 새로 재현한 SR1.x 이미지 끝의 STM32 방식 CRC와 계산 방식·대상·역할이 다르다. APK의 파일 전송 완료나 DONE 처리는 부트로더의 설치·이미지 검증·재부팅 성공을 독립적으로 증명하지 않는다.

legacy 경로는 `SunrayCMUConstants.java:53,69–70`의 NOTICE_FILE_CHANGE=0x0A, REQUEST_INDEXED_CONFIG_FILE=0xC4/응답0x44, REQUEST_FILE=0xC5/응답0x45이다. OtaQueryState는 휴대전화의 후보 firmware filename/size를 반환하고, OtaFileTransferState는 **Noodoe가 요청한 파일명과 수신 길이를 보고 휴대전화 파일의 남은 부분을 송신**한다. `REQUEST_FILE`이라는 이름이 Noodoe 플래시를 휴대전화로 가져온다는 뜻이 아니다.

## 4. 부트로더 백업과 혼동하기 쉬운 두 명령

| 명령 | 열거형 정의 | 생산 앱 실제 처리 |
| --- | --- | --- |
| BASIC_FIRMWARE_UPGRADE | 0x10, WRITE/REPLY | 출력·입력 처리기에서 invalid |
| FILE_QUERY | 0x03, READ/REPLY | 출력·입력 처리기에서 invalid |

원래 enum: `cmu/service/task/utils/Sunray_1_5_Commands.java:42,68`.

새 APK 출력에서 직접 확인:

- `fresh-jadx/OutputCommandProcessor.java:1284,1288`에서 두 enum이 switch case 45/46에 매핑된다.
- 같은 파일 `:1082–1092`는 두 case를 `invalid output commandCode from mobile`로 종료한다.
- `fresh-jadx/InputCommandParser-show-bad-code.java:1233,1237`에서 case 48/49 매핑.
- 같은 파일 `:1023–1032`는 `invalid input commandCode from device`를 기록한다.

반면 DEVICE_INFO의 bootloader major/minor는 새 입력 파서 `:234–235`에서 실제로 파싱한다. 이는 **버전 숫자를 읽는 것**이며 부트로더 byte 내용을 읽는 API가 아니다.

따라서 현 시점의 백업 전략은 이 APK OTA를 재활용하기보다, 별도 하드웨어 경로(SWD 또는 실제 칩에 맞는 ROM 부트로더의 읽기 기능)를 확인하는 편이 근거가 있다. 실제 칩·읽기 보호·BOOT 핀·회로 조건은 APK에서 알 수 없다. 그 하드웨어 절차와 보존할 내부 주소 범위는 부모의 통합 연구 문서를 따른다.

## 5. 별도 진단 APK의 UART 화면

다음은 생산 APK가 아닌 `noodoe.com.navigations_1.0`에 대한 보조 증거다. SHA-256은 `f08c30b0cfab34aefab26427d4a08834246e922d997fdd044bf9bc0beea75af2`, 크기는 10,914,615 bytes다. 이번에는 이 진단 APK의 클래스를 새로 디컴파일하지 않았다.

- `analysis/jadx/noodoe.com.navigations_1.0/resources/res/values/strings.xml:114`:
  `Connect the UART test tool and check the speed transsmition.`
- `.../sources/noodoe/com/navigation/uart/UartFragment.java:42–96`: pass/fail 체크박스와 다음 단계 처리.
- `.../sources/noodoe/com/navigation/uart/UartPresenter.java:25–26`: `appSettings.setUart(...)`에 검사 결과 저장.

이는 UART 검사에 외부 도구가 사용됐다는 정황을 보탠다. 속도 전달 시험은 계기판과 Noodoe 사이의 정상 동작 통신과 관련될 수 있지만, 해당 리소스만으로 **물리 커넥터 핀아웃·전압·baud rate·STM32 ROM 부트로더 포트**를 확정할 수 없다.

## 6. 기존 문서와의 대조 및 남은 경계

9월 1일 `firmware-update-architecture`와 9월 2일 `firmware-updater-assembly-validation`의 현대식 SPP 업데이트 골격과 legacy 파일 요청 방향은 이번 APK 재검증과 일치한다. 정밀하게 고쳐 읽을 부분은 다음이다.

- 세대 판정의 '2.x' 요약은 protocol major>2 수용을 빠뜨린다.
- 리소스 관련 정보가 존재한다고 해서 현대식 FWOTAInfo.isResourceReady가 startTask를 직접 막는다고 해석하면 안 된다.
- `0x0800` location ID, 부트로더 버전 metadata, legacy REQUEST_FILE, enum에만 남은 BASIC_FIRMWARE_UPGRADE/FILE_QUERY를 플래시 readback 능력으로 해석할 수 없다.
- 앞선 자료의 실차 5.16 tuple은 새로 구입한 2017 도너에서 얻은 값이 아니다.
- 앱은 APK 개발 시점의 서버 정책·전송 형태를 알려준다. MCU의 부트로더 install/rollback/서명 검증 정책, SWD 보호 상태, USB-B 배선은 여전히 바이너리·실물 확인 문제다.

## 재현

PowerShell에서 다음과 같이 실행한다. 파일을 읽고 이 분석 폴더 안에만 산출물을 생성한다. 디바이스 접속이나 서버 요청은 없다.

```powershell
& '<local-user>\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' '<workspace>\analysis\2026-09-09-ak550-apk-update-audit\audit_apk.py'
& 'C:\Program Files\Android\Android Studio\jbr\bin\java.exe' -Xmx2g -cp '<workspace>\.tools\jadx-1.5.2\lib\jadx-1.5.2-all.jar' jadx.cli.JadxCLI -r --show-bad-code --single-class 'com.noodoe.sunray.cmu.service.task.InputCommandParser' --single-class-output '<workspace>\analysis\2026-09-09-ak550-apk-update-audit\fresh-jadx\InputCommandParser-show-bad-code.java' '<workspace>\analysis\2026-09-09-ak550-apk-update-audit\classes6.dex'
& '<local-user>\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' '<workspace>\analysis\2026-09-09-ak550-apk-update-audit\collect_evidence.py'
```
