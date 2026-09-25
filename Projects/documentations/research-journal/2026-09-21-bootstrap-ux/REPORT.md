# Bootstrap UX · 별도 Android 설치 앱 구현

2026-09-21. 소스 구현, Android APK, Bootstrap 및 RecoveryGate 빌드와 오프라인 검증을 완료했다. **최종 Gate+Product 설치 ZIP은 Product Debug의 기존 메모리 여유 기준 미달로 생성하지 않았다. 이번 후보는 벤치나 실차에 설치하지 않았다.**

## 재생성 확인

사용자의 두 번째 Cube 재생성에서 HSE 25MHz → PLL/SYSCLK 168MHz, USB 비활성화, 공개 주변장치 초기화 함수와 main의 지연 초기화 계약을 확인했다. IOC에 빠져 있던 `RCC.PLLSourceVirtual=RCC_PLLSOURCE_HSE`를 명시했다. 생성 C를 수동 수정하지 않았다.

USB 비활성화 뒤 Cube가 제거한 LL_USB/PCD 소스를 빌드 도구의 파일별 최적화 목록에서도 제거했다. 존재하지 않는 드라이버나 빈 IRQ stub을 되살리지 않았다. `check_generated.py`와 Bootstrap 독립 빌드도 HSE 선택 누락을 거부한다. 생성 계약 시험 5개 및 프로젝트 빌드 정책 시험 7개가 통과했다.

## 구현된 설치 및 복구 흐름

- Bootstrap의 CHECK, READY, CONNECT, WORK, INSTALL_READY, INSTALL, PAUSED, RECOVERY, ERROR를 별도 앱 상태기계로 분리했다. 화면 출력은 별도 EVE ROM 렌더러다. LVGL·외장 폰트가 없어도 유지보수 화면을 구성한다.
- 연결 대기에서 O로 120초 페어링 창을 연다. 설치 확인 화면의 기본 선택은 Back이다. Install CFW 허용은 현재 전송 transaction과 암호화된 연결 epoch에만 유효하다. 새 전송·IGN OFF·연결 해제는 허용을 취소한다.
- 일반 취소는 미확정 작업만 취소한다. 이미 COMMIT된 상태나 COMMIT 결과가 불확실한 상태를 취소 성공으로 위장하지 않는다.
- 최초 묶음은 S4의 독립 Gate 64KiB와 S5–S7의 Product 384KiB다. 8개 고정 파일의 물리 검증, 자산 ID, 벡터 범위, 전송 Product와 CFWA/CFWB/부트 저널의 SHA 일치가 필요하다.
- `0x5A`는 읽기 전용 유지보수 상태 조회다. 기존 `0x58` identity 및 NDCP v1 framing/CRC/sequence 계약을 유지한다. 상태 조회나 전송 성공을 정상 부팅 확인으로 취급하지 않는다.
- `SYSTEM ERROR`는 붉은색이고 아래에 `aw shit :(`가 나온다. 사용 문구는 친근한 영어로 작성하고 오류 코드는 별도로 보존한다.

### BT가 안 될 때

키 OFF → 가운데 O를 약 1초 누름 → 누른 채 키 ON → 2초 더 유지하는 동일 제스처를 사용한다. 내부 정책은 IGN OFF 200ms 관찰, 버튼 debounce 80ms, ON 이전 500ms 및 ON 이후 2초 유지다. 중간에 떼거나 30초가 지나면 취소한다. 리셋 순간 ON+O라는 사실만으로 과거 IGN 전환을 추정하지 않는다.

Bootstrap은 소유 startup hook에서 정상 HSE/RTOS/BT/SDRAM 초기화보다 먼저 IWDG와 최소 복구 경로를 준비한다. 워치독/폴트/누른 버튼으로 진입하면 확인 대기이며, 새 제스처 없이 자동으로 순정을 덮어쓰지 않는다. 명시적으로 확인한 복구 intent만 저장소 drain 후 reset을 거쳐 복구를 시작한다.

최소 복구는 HSI, 고정 SRAM, polling SPI NOR, EVE ROM 글꼴 및 내장 raw-DEFLATE 순정 APP를 사용한다. 내장 이미지를 전체 검증하고 staging을 물리 readback한 다음 기존 순정 설치 메타데이터를 확정한다. 독립 Gate와 이 최소 복구 경로에는 BT가 없다. 원격 순정 복구 명령은 BT가 동작하는 Bootstrap 런타임의 기능이다.

## Android 포크

위치: `C:/shared/KYMCO/NoodoeInstaller/Android`, applicationId `io.noodoe.installer`.

원본 OpenNoodoe의 소스·설정 88개 파일 해시가 모두 유지됐다. 새 런처와 foreground service가 연결과 작업을 소유한다. 홈에는 순정 정보 읽기, 설치 ZIP 가져오기, Bootstrap 전송, 유지보수 상태, 백업/파일 준비/설치, 전송 이어가기, 순정 복구, 결과 재확인과 증거 내보내기가 있다.

`transport/SppTransport`와 `protocol/ndcp/NdcpClient`를 분리했다. 정상 Product의 음악·알림·GPS 통신은 새 서비스에 연결하지 않았다. 기존 일반 Product 업데이트 명령은 무선 연결 전에 거부한다. 최소 RecoveryGate에 SPP 서버가 있다고 표현하지 않는다.

설치 준비는 전체 NOR 독립 A/B 백업과 사후 A/B 대조를 수행하며 휴대폰 여유 공간 1GiB를 요구한다. 파일 생성 중 실패는 자동 재실행하지 않는다. 모든 파일의 생성이 끝난 작업만 새 전체 백업을 기존 계획과 대조한 뒤 staging 전송부터 다시 시작할 수 있다. COMMIT 불확실 상태는 별도 재확인이 필요하다.

APK: `NoodoeInstaller-debug.apk` (419,124B). 테스트용 debug 서명이며 v1/v2 서명 검증을 통과했다. 실물 휴대폰 설치·권한 UX·무선 연결 시험은 아직 수행하지 않았다. Bootstrap이 순정 bond key를 가져오지 않으므로 휴대폰에서 기존 등록 제거 후 다시 페어링이 필요할 수 있다.

## 빌드와 메모리

| 대상 | APP/이미지 크기 | 플래시 여유 | 판정 |
|---|---:|---:|---|
| Bootstrap | 435,760B | 22,992B / 448KiB | 빌드 및 내장 이미지 검증 통과 |
| 독립 Gate | 16,520B | 49,016B / 64KiB | 빌드 통과 |
| Product Release | 316,888B | 76,328B / 384KiB | 64KiB 여유 기준 통과 |
| Product Debug | 367,432B | 25,784B / 384KiB | 32KiB 기준에 **6,984B 부족** |

Product 일반 SRAM 여유는 Release 65,384B / Debug 63,856B, CCM 여유는 둘 다 16,320B다. FreeRTOS 힙 48KiB와 LVGL CCM 48KiB 및 보호값을 유지했다. Debug의 O0/g3 기본 정책·기존 파일별 예외·메모리 합격 기준을 변경하지 않았다.

같은 재생성 결과로 Integrated Release(395,156B, 여유 63,596B)와 Graphics Release(426,752B, 여유 32,000B)도 빌드·주소 검사를 통과했다. 다른 두 프로필의 Debug까지 새로 검증한 것은 아니다. 프로필 시험 후 개발 프로젝트는 Product로 복원한다.

Debug는 컴파일·링크 및 주소 배치 검사는 통과했지만 **최종 예산 검사와 APP export는 실패**했다. `product-debug`의 ELF는 분석 증거이며 설치 배포물이 아니다. 새 패키저로 실제 묶음 생성을 시도했으며 `bundle-validation/audit.json`에 같은 이유로 차단된 결과가 있다. ZIP은 만들지 않았다.

## 검증 근거

- Android unit test 66개: 실패 0, 오류 0. assembleDebug/lintDebug 통과. APK v1/v2 서명 확인.
- 실제 ARM UI/취소/제스처/설치 대상 검사: O0/Os 합계 24개 시나리오 통과.
- 실제 ARM 저장소 reader/provisioner: O0 7개/464 assertions, Os 29개/8,937 assertions 통과. Os에서 8종 파일의 설치·검사와 GateStore 경로를 검사했다. 모든 경우를 O0에서도 수행했다고 해석하지 않는다.
- Gate Product updater: 72개 ARM 실행 통과.
- Gate 제스처/정책: O0/Os 각각 1,155 assertions 통과.
- Bootstrap 순정 해제 이미지 458,752B가 승인된 순정 APP SHA256과 일치. 6개 seek, 5개 범위 거부 검사 통과.
- 기존 패키지 무결성/관측 identity 및 BL0.15 거부 시험 6개 통과. 최종 새 ZIP의 Android 실물 import 시험은 ZIP 차단 때문에 미수행.
- 상세 숫자·SHA·로그·재생성 후 프로필별 빌드 결과는 `verification-summary.json`과 같은 폴더의 로그에 남긴다.

## 남은 완료 조건

1. 기능·힙·스택·예약 기준을 임의로 줄이지 않고 Product Debug의 6,984B 부족을 해결한 뒤 두 Product ELF로 패키지 검사를 다시 통과시킨다.
2. 정상 기판에서 실제 SPP, 페어링 재연결, 버튼 제스처, IWDG 장애 진입, 설치 중 전원 차단 및 첫 정상 부팅을 검증한다. 현재 벤치 BT 고장은 ARM 모의시험으로 대체하지 않는다.
3. 승인 대상은 V5.16 / BL0.14 / sr0601이다. 실차에서 관측된 BL0.15 / SR0701은 별도 분석 전까지 거부한다. 동일 모델명만으로 이 후보를 실차에 설치하지 않는다.

이번 작업에서 MCU Flash·외장 NOR·옵션 바이트·휴대폰에 쓰는 작업은 수행하지 않았다. 과거 벤치 순정 왕복 성공은 이번 새 Gate/Bootstrap/Android 조합의 실증 결과가 아니다.
