# BL 0.15 / SR0701 호환 구현

2026-09-21. 소스와 설치 패키지 구현 완료. **이번 수정본은 장치에 설치하지 않았고, 하드웨어에는 접근하지 않았다.**

## 결과

기존 BL0.14 고정 의존성을 제거하고, 관측한 실차 `BL0.15 / SR0701 / SAA1AA(KR) / HW0 / V5.16`에서 Bootstrap 진단 후 CFW 설치를 시도할 수 있게 했다. 순정 BL을 0.14로 낮추지 않는다. 과거 일괄 차단을 없애되 기기 식별·UID·이미지 해시·파일 범위·복구 파일 검사는 유지한다.

- [설치 앱 APK](NoodoeInstaller-BL015-debug.apk)
- [실차 0.15용 설치 ZIP](vehicle015/installer.zip)
- [패키지 감사 결과](vehicle015/audit.json)
- [기계 판독 검증 요약](verification-summary.json)
- [호환 구조와 프로토콜](../../STM32CubeProjects/FuckNudo_Noodoe_CFW_Project/Middlewares/Noodoe/Update/BL_COMPATIBILITY.md)
- [사전 리버싱 근거](../2026-09-21-bl015-compatibility-research/REPORT.md)

## 변경 내용

### 원래 BL 보존

업데이트·순정 복구·NOR 쓰기 승인·부팅 확인에서 0x000E0000만 허용하던 경로를 0x000F0000까지 확장했다. 메타데이터의 첫 워드는 트랜잭션 시작 때 읽은 원래 BL 버전을 그대로 쓴다. 진행 중 버전이 달라지면 실패한다. BL 코드/옵션바이트를 수정하는 명령은 추가하지 않았다.

0.14는 확보된 코드32KiB SHA를 확인한다. 0.15는 실제 순정 DeviceInfo/OQC에서 관측한 factory 필드와 벡터 범위를 확인한다. **0.15 BL 바이너리는 아직 없으므로 그 코드의 내부 동작이나 공식 서명을 인증하는 검사가 아니다.**

### 기기별 원본 결합

Bootstrap NDCP `0x60`은 하위64KiB만 읽는 명령이다. 한 요청은 최대480B, 암호화된 연결에서만 허용하며 설치·복구·저장 중에는 거절한다. 앱이 두 번 읽어 동일성과 BL 코드 SHA를 확인한 뒤 UID별 파일을 fsync/readback하여 보관한다.

`CFWREC.DAT` v2에 원래 BL 버전과 코드32KiB SHA를 포함했다. Gate/Bootstrap/Product 복구는 물리 BL과 이 값이 맞아야 진행한다. v1은 기존0.14만 지원한다. 앱은 다음 패키지에서도 동일 UID의 원본 백업을 재사용하며 Product에서 백업이 없으면 donor 해시를 대신 넣지 않는다.

백업은 **Bootstrap 설치 후** 하위64KiB 스냅샷이다. BL 코드와 공장 데이터는 원본이지만 APP 설치 메타데이터는 이미 바뀌었을 수 있다. 설치 전 전체 MCU 백업이라고 표시하지 않는다.

### 패널 및 하드웨어 진단

순정에서 동일 초기화 시퀀스를 사용하는 패널 selector3/4를 지원한다. PA3/PH3/PH2/PB10에서 실제 board strap을 읽는다. SR0701이라는 이름으로 strap7을 추정하지 않는다. 현재 초기화 경로는 revision>=3을 지원하며 다른 경우 실패로 남긴다.

Bootstrap 설치 허용 단계에서 BT READY, 보안 연결, 초기 화면/NOR/RAM 오류 여부와 기존 IGN/세션/사용자 확인 조건을 확인한다. 실제 무선 하드웨어 성공을 빌드 성공으로 대체하지 않는다.

### 앱 및 배포 정책

OpenNoodoe 원본은 수정하지 않고 NoodoeInstaller 포크만 변경했다. 신규 패키지는 `target.deployment=bootstrap-diagnostics`, `target.boot.binding=device-capture`다. `installable=true`는 진단 후 시도 허용이며 `wireless_verified=false`와 별개다. 최종 ZIP을 실제 Android production importer에 넣고 저장된 실차 DeviceInfo 응답과 대조하여 통과했다.

구형 PC 패키저는0.15에 donor BL을 잘못 결합하지 않도록 거절하고 신규 패키저 사용을 안내한다.

## 빌드와 메모리

| 빌드 | 실행 이미지 | 플래시 여유 | 일반 SRAM 여유 |
|---|---:|---:|---:|
| Product Release | 326,708B | 66,508B | 60,200B |
| Product Debug | 360,260B | 32,956B | 58,744B |
| RecoveryGate | 19,736B | 45,800B | 별도 Gate 구성 |
| Bootstrap | 440,420B | 18,332B | 별도 Bootstrap 구성 |

Product CCM 여유16,320B, FreeRTOS 힙49,152B 유지. Release64KiB/Debug32KiB 플래시 여유 기준을 통과했다. 이번에 실시간 FPS나 실제 힙 최소 여유를 새로 측정하지 않았다.

추가 코드로 부족해진 공간은 중복 CRC 루프 공통화와 고정 wire 직렬화 코드 개선으로 확보했다. BSP의 작은 CPU CRC 함수를 공유하고, 내부 플래시 쓰기 중 실행되는 SRAM metadata checksum은 독립적으로 유지한다. 직렬화는 모든 필드 크기/배치/little-endian을 정적 검사하고 기존 wire bytes를 회귀시험했다. 힙·태스크 스택 축소나 예산 완화는 하지 않았다.

## 검증

- Product Release/Debug 표준 Cube headless 빌드, 이미지 주소/벡터/자산/메모리 예산 검사 통과. Gate/Bootstrap 별도 빌드 통과.
- Android 단위시험86개, APK 빌드 및 최종 실제 ZIP 가져오기/실차 식별 대조 통과.
- ARM O0/Os: metadata 각8,386 assertion, protocol/OTA 각23,388, target profile 각19, RecoveryCore 각36, 제어 wire serialization 각121 통과.
- ARM runtime update, Storage SWD, CFW 설정/주행/사진 저널, Gate 정책·로그, display capture CRC 시험 통과. 상세 결과는 verification-summary의 스냅샷 참조.
- 자산 시험은 O0/Os 각각19개 항목 통과. 활성 A 슬롯을 보존하는514개 모의 전원 차단 경계, 손상/버전/잘린 파일, trial/reset 중 쓰기 거절을 포함한다.
- Bootstrap storage의 최종 공통 CRC 코드 재검증 결과도 검증 요약에 기록한다.

자산 시험의 기존 fixture가 확정된 Gate journal 없이 쓰기를 시도하여 error23이 발생했다. 실제 보호를 풀지 않고 fixture를 확정 상태로 맞추고, journal 없음/trial/reset에서 물리 쓰기0인 거절 시험을 추가했다.

**모의0.15는 donor 코드에 metadata/factory fixture를 구성한 것이다. 실제0.15 BL을 실행한 시험이 아니다.** NOR·BT·전원 등 대역을 사용한 ARM 시험은 실제 전원 차단/무선/RTOS 선점을 증명하지 않는다.

## 사용할 순서와 남은 확인

APK 설치 후 앱에서 이 ZIP을 가져오고 순정 식별을 읽어 대조한다. Bootstrap 전송 후에는 Bootstrap에 다시 연결하여 상태와 원본 백업을 확보한다. Bluetooth test가 정상이고 화면/NOR/RAM 진단을 통과하면 Install CFW 절차를 진행한다. Bootstrap 내부의 순정 V5.16 압축본과 Back to stock은 유지했다.

실차0.15의 실제 BL 코드 확보, 정상 BT에서 순정→Bootstrap→CFW→실패복구→순정 왕복, 순정 BL의 최초 설치/복원 중단 동작은 남아 있다. 진단 화면이 정상이라는 사실만으로 그 모든 복구 상황을 검증했다고 표시하지 않는다. 이전 전체 컴패니언 계획의 CJK/사진 전송 등 미완료 기능까지 이번 작업으로 완료됐다고 주장하지 않는다.
