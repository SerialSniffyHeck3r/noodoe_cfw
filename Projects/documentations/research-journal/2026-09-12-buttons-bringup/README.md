# 세 버튼 bring-up 및 표시 API

순정 GPIO/이벤트 분석은 [stock-buttons.md](stock-buttons.md), 사용 API와 입력 정책은 [BSP/BUTTONS.md](../../STM32CubeProjects/FuckNudo_Noodoe_CFW_Project/Drivers/BSP/BUTTONS.md)에 정리했다.

## 구현

- `BSP_Buttons.c/h`: UP=PD12, ENTER=PA15, DOWN=PI6. LOW 눌림, NOPULL, 양에지 EXTI. ISR은 원시 변화 진단, 태스크는 안정화/시간/이벤트를 처리한다. Init이 소유 입력과 공유 IRQ만 초기화한다.
- 공개 상태/기간/이벤트 API: PRESS, RELEASE, SHORT, LONG(2001ms), VERY_LONG(3000ms); 21ms debounce, short 결정 기준80ms. 현재/직전 기간, 분류, 마스크, overflow 조회.
- 순정의 주요 설정을 대조했으나 완전 복제는 아니다. press 안정화, release에서 hold 동결, 처리 지연에 독립적인 최소 판정, very-long은 보완/확장이다. PH9/IGN gate와 IGN 전환 ENTER+DOWN 조합은 상위 정책으로 제외했다.
- `BSP_Display.c/h`: 검증된 EVE/패널/백라이트 구현 앞에 초기화, 프레임, 사각형/선, 밝기, 진단 API를 제공한다. 256-word 고정 DL이며 framebuffer/그래픽 프레임워크는 없다.
- `LCDTest()`가 위/엔터/아래 순서의 40×40 칸을 표시한다. long/very-long은 아래에 추가 칸이다. 첫 행은 해제 시 어두워지고 추가 칸은 사라진다. 최대152 words. 입력 서비스 약5ms, 상태 변화 시 redraw, 기존25%/OFF 500ms 깜빡임 유지.

## 검증 자료

| 대상 | 결과/근거 |
|---|---|
| 순정 핀/역할 | GPIO descriptor→short event→OQC packet→APK 상수 주소 교차검증 |
| Debug | 최종 빌드0 errors/0 warnings, APP49,148bytes. [빌드 로그](debug-final-build.log) |
| Release | 최종 빌드0 errors/0 warnings, APP28,572bytes. [빌드 로그](release-final-build.log) |
| 실제 버튼 C 로직 | Cortex-M4 ARM 명령을 Unicorn으로 실행, O0/O2 각각10 시나리오94 assertions 통과. [결과](../2026-09-12-buttons/host-tests/results.json) |
| 생성 코드 경계 | Core34개 C/H의 USER CODE 밖 동일. [검사 결과](regeneration-boundary-check.json). 이번 턴의 실제 GUI 재생성은 수행하지 않음 |
| 장치 기록/부팅 | [자동 실행 자료](../bringup-runs/2026-09-12-051528-270-Debug/summary.json): PASS. 전체 백업 A/B 일치, APP 읽기 일치, 하위64KiB 보존, 순정 BL을 통한 두 번의 reset 통과 |
| 실제 EXTI 경로 | [SWIER 시험](software-exti-check-halted/result.json): UP/DOWN/ENTER ISR 각각 +1, 서비스 진척 확인. 물리 버튼 누름은 아님 |

Debug APP SHA-256: `916ee8632a2f4f8584a72d302112334db04d81a4d44c04da625a9d8744431ec5`.

Release APP SHA-256: `b1ac9aeb2d8f587f2f33b6a03edea48aca85cd6f357765642732a61e25faf675`.

호스트 시험은 실제 GPIO 전기 신호나 EVE를 에뮬레이션한 결과가 아니다. HAL 입력/시각 fixture로 실제 BSP C의 상태 전이를 실행한다. 포함 범위: 59/60ms short 경계, 2000/2001/3000ms, release 동결, 지연된 잡음, bounce/IRQ 반복, 동시 눌림, boot-held, tick wrap/기간 포화, 이벤트 큐 overflow, 잘못된 인수, Init의 소유 GPIO/EXTI 보존.

실물 버튼 및 화면 관측은 아래 결과 기록에 별도로 남긴다. SWIER를 이용한 시험은 GPIO를 누르지 않고 IRQ 경로만 확인하는 것이므로 물리 버튼 통과로 간주하지 않는다.

## 장치 검증과 관측 한계

168MHz, APP VTOR, FreeRTOS/HAL TIM6 진척, fault 없음, EVE ID0x7C/CPU reset0/프레임 증가, panel0x9C, 백라이트500Hz/CCR4=0 또는25, 버튼 EXTI routing 및 양에지 mask0x9040을 확인했다. 두 번째 reset 후 stack high-water887words, 최소 잔여 heap61,312bytes였다. Release는 빌드/이미지 검증만 수행했고 장치에는 Debug를 기록했다.

순정 BL/메타/기기 데이터가 포함된 하위64KiB SHA-256은 계속 `f38ed839c4fbce74d4d8227fcf13764194f405c4860cda75c628fce66064b574`이다. 전체 erase/옵션 바이트/외장 저장소 쓰기는 수행하지 않았다.

첫 번째 실행은 전체 백업 중 한 구간의 읽기 불일치로 쓰기 전에 멈췄다. 원본/재읽기로 word-shift 오류를 분리했고, 기존 검사를 유지한 전체 재시도가 통과한 뒤 기록했다. 실패 파일도 보존했다. [오류 분석](backup-read-glitch.md).

첫 SWIER 시험은 실행 중 ISR이 SWIER를 즉시 지워 CLI의 쓰기 확인이 실패했다. 해당 시도에서도 실제 세 IRQ 카운터는 증가했다. IRQ가 처리되기 전 레지스터 확인을 끝내도록 halt→SWIER 쓰기→run으로 시험 도구만 수정했고 재시험이 통과했다. 펌웨어는 바꾸지 않았다.

자동 시험 중 UP(PD12)는 계속 LOW/눌림으로 관측됐고, ENTER/ DOWN은 HIGH였다. 따라서 boot-held와 long/very-long 화면 상태는 관측됐지만 이것을 정상적인 물리 버튼 동작으로 확정하지 않는다. 사용자에게 해제 상태와 각 버튼의 실제 화면 반응을 확인 요청했다.
