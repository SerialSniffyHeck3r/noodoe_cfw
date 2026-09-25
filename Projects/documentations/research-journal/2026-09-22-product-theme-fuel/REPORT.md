# Product UI·Bootstrap 진단·단계별 절전 — 2026-09-22

## 구현 범위

- HOME: 배경 / 날짜 / 날짜+속도 3개 하위 화면. UP/DOWN 순환, O 길게는 하단 ODO/TRIP 모드 변경. 속도는 D-DIN 숫자 비트맵을 2.5배 확대하고 3자리 고정 셀·고정 기준선·리딩 제로 없음. 날짜 화면은 사진 전체를 어둡게 하지 않고 날짜 뒤에 둥근 반투명 판만 둔다. HOME 아이콘 스트립은 주 카테고리 변경 후 5초에 숨는다.
- 테마: Dark / Light / Auto. 자동은 조도 센서 또는 시간표를 선택한다. 센서는 히스테리시스와 3초 안정 조건, 오류 시 마지막 유효 테마 유지. 사진 RGB는 변환하지 않고 글자·구분선·아이콘·음영·화면 바탕을 함께 변환한다. 실기기 캡처에서 발견한 시계/ODO의 검정 고정 clear도 제거했다.
- 경고: `ScreenWarningOverlay(icon,color,seconds1,seconds2,message)`. 3초 1Hz 점멸 → 400ms 축소·상승 → 문구 3초, 문구 단계 버튼으로 닫기. 소비한 버튼의 RELEASE/SHORT가 원래 페이지로 새지 않는다. 1칸은 주황 Low Fuel, 0칸은 빨강 Fuel Level Critical 및 Reserve 고정. 2칸 이상 새 샘플이 3초 유지되어야 해제한다. 낡은 UART나 측정 오류를 0칸으로 위장하지 않는다.
- 하단: 위쪽 사다리꼴 유지, 아래쪽 구분선 제거. 오일 잔량 바는 X 중앙 136×9px.
- 상태 아이콘: 고정 위치 Bluetooth/휴대폰 알림 상태와 GPS 전송 활성 상태. Google Material Round 자산과 라이선스 유지. 연결 세대·알림 ID/revision·읽음/나이·GPS heartbeat를 폰과 주고받는다.
- Bootstrap: 기존 환영 화면 유지. 누도 본체의 Ambient 메뉴에 RAW/lux/센서 ID/오류/데이터 나이를 표시한다. BT 자가진단은 Bootstrap과 Product의 동일 `RadioSelfTest`에서 8KiB 패턴·CRC·epoch·시간 제한을 검사한다. 실제 RF 왕복 성공과 단순 controller 초기화 상태를 구별한다.
- Android APK6.5: 알림 메타데이터/GPS 상태 및 공용 BT self-test 명령 지원. 기존 앱의 연결 서비스/큐를 재사용한다. 원본 OpenNoodoe는 수정하지 않았다.

## 절전 단계와 실제 MCU 동작

`Settings → Power`에서 각 단계의 사용 여부와 아래 두 전환 시간을 별도로 설정한다. 기존 설정 파일의 필드 ID와 값을 보존한다. 전체 설정 기본값은 화면 60분, BT 전용 60분이며 기존 사용자 설정이 있으면 그것을 우선한다.

| 상태 | 표시/백라이트 | Bluetooth | MCU/전환 시간 |
|---|---|---|---|
| IGN OFF 1초 유예 | 마지막 화면 그대로 | 유지 | RUN. 빠른 키 재입력은 종료·누계 초기화를 만들지 않음 |
| 종료 애니메이션·Ride Summary | 표시 | 유지 | 기존 종료 상태기계, 저장 요청 처리 |
| `OFF_DISPLAY_HOLD` | 패널 ON, 백라이트 OFF, 시계 분 변경 시에만 새 프레임 | 유지 | Sleep/WFI, 화면 유지 **0~1,440분** |
| `OFF_BT_HOLD` | 패널/EVE sleep, 백라이트 OFF | 유지 | Sleep/WFI, BT 전용 유지 **0~1,440분** |
| `OFF_DEEP_SLEEP` | OFF | 종료 | STOP, IGN ON까지 유지 |

중간 단계 0분은 이후 활성 단계가 있으면 건너뛴다. 비활성 단계를 건너뛰며 마지막으로 활성화한 단계는 IGN ON까지 유지한다. 최종 STOP에서 시간이 지나면 다시 켜지는 타이머는 만들지 않았다.

BT 유지 단계에서 H4 UART/DMA는 살아 있어야 하므로 HSE/PLL을 끄는 STOP에 억지로 넣지 않는다. FreeRTOS idle WFI와 RTC wake 기반 tickless Sleep으로 CPU 일을 줄인다. 계기판 UART는 IGN OFF 동안 정지하고 조도 변환도 쉬게 한다. 센서의 오류/미장착 때문에 종료를 무한 대기하지 않는다.

마지막 단계는 IO·Storage·BT·Graphics 네 소유자의 정리 완료, 진행 중 저장 작업 없음, DMA 정지, 실제 PG13 IGN OFF를 재검사한다. SDRAM self-refresh → `LPDS | FPDS`, `PDDS=0`, `SLEEPDEEP`, WFI → HSE/PLL 168MHz 복원 → SDRAM 정상 모드 → 인터럽트/태스크 재개 순서다. 복원 실패는 정상 복귀로 보고하지 않고 watchdog/fault 경로에 남긴다. 실제 진입 모드와 RCC/PWR/SCR를 `g_bsp_lowpower` v2에 남긴다.

IWDG와 태스크 deadline은 유지한다. STOP을 1분 연속 수행한다고 주장하지 않는다. RTC 절전 구간은 최대 500ms 이하이고 건강 감시 등의 더 빠른 deadline이 우선한다. 설치 중에는 공용 InstallSession의 RUN 유지 정책이 우선한다.

### 순정과의 관계

순정 분석 자료 `Reversing/docs/2026-09-10-noodoe-power-state-and-sleep.md`의 `0x0804260C → 0x0804DB80`은 LPDS/WFI STOP을 사용한다. 이번 마지막 단계도 이 MCU 방식에 맞춘다. 그러나 순정의 정상 IGN OFF 종료 경로와 그 간접 STOP 호출의 연결, 외부 PD13/PG14/PI9 출력의 완전한 전원 회로는 미확정이다. 추측으로 전원 GPIO를 낮추거나 NOR deep-power-down 명령을 추가하지 않았다. 따라서 **완전 12V 차단 또는 순정과 동일한 소비전류를 보장하는 구현은 아니다**. ‘All off (STOP)’는 이 구별을 드러내는 설정 이름이다.

## 메모리와 패키지

| 대상 | 실행 이미지 | 플래시 여유 | 일반 SRAM 링크 여유 | CCM 여유 |
|---|---:|---:|---:|---:|
| Product Release | 327,216B | 66,000B | 56,368B | 16,320B |
| Product Debug | 359,524B | 33,692B | 54,904B | 16,320B |
| Bootstrap | 457,684B | 1,068B | 별도 빌드 기록 | 별도 빌드 기록 |
| Gate | 22,492B | 43,044B | 별도 빌드 기록 | 미사용 |

기존 메모리 예산, RTOS 힙/스택/큐 크기는 낮추지 않았다. Product는 0x08020000 시작 384KiB, Gate는 0x08010000 시작이다. 새로운 아이콘 4,448B는 외장 자산 entry19에 들어가며 기존 폰트/BT 패치 entry1~18은 유지한다. 요구 자산 SHA256은 `45b5308e2adf6c4333be46fc09ed8e7a9af039dcff895c27428fdc7680b984ce`. GPU 최악 정적 자산은 114,214/114,688B로 잔여 474B다. 다음 아이콘 추가 전에는 이 예산을 다시 검토해야 한다.

- 최종 빌드: `complete-release/`, `complete-debug/`
- Android: `NoodoeCompanion-6.5.apk`
- 실차 관측 ID 대상 패키지: `vehicle015-complete/installer.zip`, SHA256 `330e0de40c4a2c5904f60f11f7c2f89b3e17cba2314d24f664969e1ebe9f2a5a`
- ZIP은 Python 패키지 검증과 **실제 Android RecoveryBundle importer**를 모두 통과했다. 앱 표시 버전6.5와 별개로 펌웨어의 기존 transport 버전 필드는6.4를 유지하며, 실제 이미지 식별은 패키지/이미지 SHA를 사용한다.

## 검증과 한계

Product Release/Debug, Integrated Release, Graphics Release, Bootstrap, Gate 빌드 통과. Cube 원본/vendor 소스를 임의 수정하지 않았다. 이번 실제 Cube GUI 재생성은 수행하지 않았다.

확대 숫자 draw-task가 upstream의 tagged-task 제외 규칙으로 탈락하는 문제를 전용 evaluate 등록으로 수정했다. 실제 ARM renderer와 제공 D-DIN 메트릭으로 0~400, 미수신 표시, 일반 label과의 분리, 고정 셀 및 경계를 O0/Os 각각 2,993 assertions로 검사했다.

실제 ARM C 모의시험: 신규 테마/연료/경고/폰 상태/자가진단/절전 마스크 111 assertions씩 O0/Os, 저전력 port 449 assertions씩 O0/Os. 기존 UI·설정·프로토콜·사진 자산·캡처·조도·컴패니언 회귀 시험 통과. Android158시험, 실패0. `regression2-results.json`은 새 API용 시험 fixture를 갱신한 최종 결과이며 초기 실패 로그도 남겨 놓았다.

실물 EVE 캡처로 날짜의 둥근 반투명 배경, Bluetooth/GPS 아이콘, Fuel Level Critical 경고를 확인했다. 캡처용 경고는 **표시 상태만 일시 주입**했으며 실제 UART 연료 이벤트나 정상 무선 성공으로 해석하지 않는다. Arduino UART 장치는 이번 시점에 연결되어 있지 않았다.

현재 벤치에서는 BT가 controller 시작 timeout 0x302, ALS가 I2C 오류3이다. 따라서 새 코드의 정상 무선 연결·실제 조도 반응·BT 유지 중 소비전류를 이 기판으로 검증했다고 할 수 없다. 실제 IGN OFF/ON 단계 및 전류 측정은 사용자 전환을 기다리는 별도 시험이며 모의시험으로 대체하지 않는다.

최종 SWD 설치와 실행 결과는 `verification-summary.json`, `complete-install.log`, `bench-write-evidence-*` 및 최종 캡처에 기록한다. RAM 진단 도구에서 LTO의 동일 이름 static 변수 주소를 GDB가 잘못 선택한 문제는 ELF 객체 크기와 심볼을 함께 확인하도록 수정했다. HOME 상태기계의 이상으로 잘못 결론내리지 않았다.

## 최종 벤치 설치 결과

최종 Product SHA256 `529e13b097daff23ed5410ccdac3d3b748d56396cd44a11813950bad7331a2b9`. 최종 설치는 기존 Gate를 유지하고 Product만 교체했다. 최초 이 작업의 구형 Gate→현행 Gate 호환 이관은 별도 이전 증거에 보존했다. 순정 하위64KiB와 FAT 불변, 내부512KiB 독립 재읽기2회 일치. 이전 활성 APP은 NOR 반대 bank에 보존했다. 이는 명시적 로컬 SWD 정비 경로이며 무선 시험 부팅 확정으로 주장하지 않는다.

새 실행본 137초 표본: 29.9FPS, CPU 비유휴60.0%, FreeRTOS 최소여유23,752B, CCM guard 오류0, 그래픽/SPI/system error0, 네 건강 감시 소유자 모두 진척. 실제 IGN은 아직 ON이어서 이번 STOP 진입 실측값은 없다. 캡처 예외를 제외한 짧은 표본이며 8시간·전체 메뉴 최악부하 시험으로 확대하지 않는다.

`final-home-dark.png`의128은 draw-string만 잠깐 바꾼 표시 시험으로 실제 차량속도/거리에는 입력하지 않았다. `final-home-light.png`는 최종 흰 바탕·검정 글씨·청회색 링·날짜 판을 확인한 실물 EVE 캡처다. 모든 임시 표시/테마 변경은 복구하고 장치를 정상 Product 실행 상태로 남겼다.
