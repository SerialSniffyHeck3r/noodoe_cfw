# Companion 6.9.6 · 연결·전화·알림 수정

APK와 대응 설치 ZIP을 함께 적용한다. 기존 최신 Gate 사용자는 주행 연동을 수동 중지한 뒤 **CFW 업데이트**에서 이 ZIP을 선택한다. 이번 수정 때문에 순정으로 돌아가거나 Gate를 교체할 필요는 없다. 구 Gate가 실제로 다른 경우의 호환성 검사는 그대로 유지한다.

- 순정 → Bootstrap 자동 연결은 즉시 실패 5회로 끝나지 않고 최대 4분의 준비 시간을 사용한다. 페어링 거절은 반복 요청하지 않는다.
- ‘전화 받기·종료 · 동반 기기 등록’의 누락된 기능 선언과 예외 처리를 수정했다. 기존 전화 앱은 유지한다. 설치 후 해당 등록 메뉴에서 Android 승인을 완료한다.
- 전화 제어 권한을 실제 Android Telecom 승인 상태로 확인한다. 연락처/최근 목록과 통화 제어의 권한을 구분한다.
- 수신 전화 이름을 실제 번호로 조회하고, 번호가 바뀌면 이름 이미지도 교체한다. 통화 중에는 이름 아래에 번호를 표시한다.
- 전화 목록 제목은 고정하고 내부 항목만 세로 이동한다.
- 알림의 앱 아이콘·앱 이름·제목·본문 간격을 분리했다. 기존 GPU 버퍼를 사용하면서 표시 여백만 넓힌다.
- 알림 이미지를 무손실 압축하고 최신 알림부터 전송한다. 시험 알림은 18,432 → 7,778B(57.8% 감소). 실제 시간은 무선 환경에 따라 다르다.
- 음악 텍스트 타일 때문에 이미 전송 중인 앨범아트를 처음부터 재전송하던 경로를 제거했다.
- 저장 큐 대기를 저장 실패로 표시하던 경로와 긴 키 OFF 후 체크포인트 시간을 잘못 판단하던 경로를 수정했다. 실제 NOR 오류 표시는 유지한다.

검증: Android 270개 시험, lint 오류 0, Cortex-M4 코드 회귀시험, Product Release/Debug 빌드·메모리 예산, 기존 서명과 새/구 ZIP importer 통과. 이번에는 ST-LINK·ADB·실차에 접근하지 않았다. 정상 무선 재연결, Samsung의 실제 등록/통화, 실기 화면/FPS와 전송 시간은 아직 확인하지 않았다.

Bootstrap·Gate·순정 복구본·자산·삭제/진단 이미지의 바이트는 이전 배포본과 동일하다. Product와 APK가 바뀐다. 정상 업데이트 확정 후 설정/CFW 사진 초기화 정책은 기존과 같다.

## 원인과 구현 근거

### 전화 등록·권한·신원

AndroidManifest에 `android.software.companion_device_setup` 기능 선언이 없었고 CDM.associate의 SecurityException이 Activity 밖으로 나갔다. 선언 및 등록/선택기 예외 처리를 추가하고 실패 시 안내를 남긴다. [Android CompanionDeviceManager](https://developer.android.com/reference/android/companion/CompanionDeviceManager).

MANAGE_ONGOING_CALLS를 일반 runtime permission으로 조회하면 동반 기기 등록을 마쳐도 false가 될 수 있다. API31+의 `TelecomManager.hasManageOngoingCallsPermission()`과 선택 주소의 실제 association으로 확인한다. [Android TelecomManager](https://developer.android.com/reference/android/telecom/TelecomManager).

이름 이미지 키에 전화번호가 빠져 있었다. 번호·이름·통화 ID·세대·상태를 모두 반영한다. 최근 통화의 CACHED_NAME 대신 실제 번호로 PhoneLookup을 조회한다. 조회 불가 시 다른 통화의 캐시 이름을 재사용하지 않는다. 통화 상세가 아직 null인 콜백도 허용한다. 실제 사용자 연락처/통화 내용은 시험이나 일반 로그에 수집하지 않았다.

### 연결·전송

BootstrapConnectSession은 기존에도 240초 마감값이 있었지만 외부 5회 반복이 먼저 끝났다. 마감 기반 읽기 전용 연결 재시도로 수정했다. createBond가 BOND_NONE 상태에서 즉시 거절되면 준비 안 됨으로 재시도하되, BONDING 이후 사용자 거절/취소에는 재요청하지 않는다. 역할·SHA·UID 확인, 보안 RFCOMM, COMMIT/RESET 무단 재전송 금지는 유지한다.

VisualTransfer의 pauseForTile/pauseForMusic가 진행 중인 RAM 업로드까지 처음으로 돌렸다. 이제 BEGIN 전 작업만 재정렬하고 수락된 전송은 CRC·완료 확인까지 끝낸다. 음악/알림은 완료 단위로 번갈아 보내고 통화 패널은 우선한다. 알림 메타데이터는 과거 이미지 전송이 끝나기를 기다리지 않는다. 알림별 큐를 최신 한 세대로 합친다.

### 알림 형식과 UI

새 capability 0x2000, visual kind8: [128, layout] 뒤에 PackBits형 A4 바이트열. layout1은 기존 288×128, layout2는 같은 18,432B 안에 status24/app32/title32/body40행을 넣는다. UI는 원본 픽셀을 확대하지 않고 각각 y=0/34/84/132에 그려 간격을 되살린다. 앱 아이콘은 알림을 만든 패키지 context에서 SmallIcon을 읽고 application icon/envelope로 대체한다.

디코더는 길이·버전·CRC·run 경계·최종 크기를 검사한다. 실패한 패널은 공개하지 않는다. 고정 SDRAM 버퍼, 이미지 키 불변, 두 페이지 bank와 display-list fence를 유지한다. 구 CFW에는 기존 kind6 288×128 데이터를 전송한다.

Android Canvas/Skia로 일본어·번체 혼합 알림 PNG를 만들고, 투명 여백 제거 전후 픽셀 동일성을 확인했다. Android 출력 RLE를 실제 ARM C 디코더에 입력해 바이트 동일성을 확인했다. LVGL draw 호출 경계 시험에서 전화 제목의 루트 Y 고정, 행 이동, clip 복원, 알림 4개 strip 위치를 검사했다. 이 결과는 실기 캡처가 아니다.

### 저장 상태

ConfigStore의 CFW_BUSY/CFW_PENDING은 아직 쓰기가 수락되지 않은 정상 대기이다. dirty 상태를 유지하고 재시도하며 실패 횟수/결과를 오염시키지 않는다. Ride 저장도 같은 처리다. 체크포인트의 기준은 직전 성공과 새 dirty 시작 중 최신 시각으로 잡아, 오래 깨끗했던 대기를 저장 지연으로 오인하지 않는다. 실제 readback/CRC/NOR 실패는 그대로 노출한다. 사용자 실차 로그가 없어 다른 저장 장애까지 모두 배제한 것은 아니다.

## 자원과 호환성

| 빌드 | APP 플래시 여유 | SRAM 여유 | CCM 여유 |
|---|---:|---:|---:|
| Release | 65,704 B | 53,272 B | 16,320 B |
| Debug | 33,912 B | 54,192 B | 16,320 B |

GPU 캐시 보수적 상한 114,214/114,688B, 여유 474B. 제공 캐시 글리프 전체·아이콘 전체·두 18KiB 페이지를 포함한다. 160px 숫자는 기존 페이지에 합성하므로 별도 글리프 캐시로 중복 계산하지 않는다. 480×480 사진 두 장과 캡처 배치는 유지한다. 캐시 여유가 작으므로 향후 새 상주 자산 추가는 재산정이 필요하다.

LE32 scalar codec의 바이트별 루프를 little-endian compile guard + memcpy로 정리했다. 겹치는 알림 목록 이동은 memmove를 사용한다. 큰 dispatch/visual worker 경계를 유지해 LTO의 과도한 합성을 줄였다. 스택·큐·힙을 줄이거나 예산을 완화하지 않았다.

검증 상세는 verification-summary.json, GPU 계산은 gpu-budget.json, ARM 각 시험은 *-results.json 및 *-tests.log, 빌드 로그는 firmware-release.log / firmware-debug.log / android-build.log에 있다. 기존 컴파일 경고는 로그에 남기며 빌드 성공을 경고 0으로 표현하지 않는다. 전체 무선 경로·실차 통화·GPU 장시간 부하는 별도 검증이 필요하다.
