# 순정 V5.16의 PH9 버튼 허용 조건과 CFW 비교

2026-09-23. 원본 BIN 정적 분석, 원본 Thumb 코드의 오프라인 실행, 현재 CFW 소스 대조. 장치에 접근하거나 펌웨어를 수정·설치하지 않았다.

## 결론과 사용자 관찰

사용자는 같은 버튼으로 계기판과 누도를 조작하며 별도 선택 스위치로 대상을 바꾼다고 설명했다. CFW에서는 계기판 선택 상태에서도 누도가 조작된다.

순정 앱은 **PH9 HIGH && IGN ON 캐시 != 0**일 때만 UP/ENTER/DOWN의 입력 에지를 기록한다. PH9 LOW이면 세 버튼 모두의 누름/해제 기록을 생략한다. 현재 CFW에는 이 PH9 조건이 없다. 사용자 관찰과 코드 차이가 일치하므로 PH9는 선택 스위치 신호의 유력한 대응 핀이다. 다만 실물 스위치 양 위치에서 PH9가 어떻게 바뀌는지 이번에 측정하지 않았으므로 하네스 연결 및 스위치 방향은 아직 실측 확인 대상이다.

이 조건은 부트로더가 아니라 **순정 V5.16 애플리케이션**에서 발견했다. 속도나 주행 상태를 판독하는 분기가 아니다.

## 원본과 주소 해석

- 전체 512KiB 백업: `../../VERY_IMPORTANT_ORIGINAL_NOODOE_BOOTLOARDER/noodoe_full_flash_dump_A.bin`, base `0x08000000`.
- SHA-256: `38207bed741a8fef9e6d2ed24b86c8cb8b376ba5e4c957163f5edd83fbeb8037`.
- 전체 백업의 `0x10000` 오프셋부터 V5.16 OTA 이미지가 byte 단위로 일치함을 다시 확인했다.
- 아래 `0x080...` 주소는 MCU FLASH 실행 주소다. 전체 백업 파일의 오프셋은 `주소 - 0x08000000`, OTA 파일에서는 `주소 - 0x08010000`이다. 예: 핵심 분기 `0x08043A26`은 전체 백업 오프셋 `0x43A26`이다.
- `0x200...`는 RAM 주소이며 패키지 핀 번호가 아니다. PH9는 GPIO 포트 H의 9번 비트다.

## 원본 기계어 증거

[exti-button-gate.asm.txt](exti-button-gate.asm.txt)의 콜백 진입은 `0x080439F4`다. EXTI mask가 12, 15, 6 중 하나인지 먼저 확인한다.

| 실행 주소 | 명령 / 의미 |
|---|---|
| `0x08043A18` | `ldrh r1,[r0,#0x12]`: RAM `0x20000A20`의 핀 테이블에서 pin9 mask `0x0200` 취득 |
| `0x08043A1E` | `ldr r0,[r0,#0x1C]`: RAM `0x20000998`의 포트 테이블에서 GPIOH `0x40021C00` 취득 |
| `0x08043A20` | `bl 0x080355F2`: IDR `0x40021C10` bit9 읽기 |
| `0x08043A24` | `cmp r0,#0`: LOW인지 비교 |
| **`0x08043A26`** | **`beq.w 0x08043BBC`: LOW이면 콜백 종료** |
| `0x08043A2A–0x08043A30` | RAM byte `0x20023302` 읽고 0과 비교 |
| **`0x08043A32`** | **`beq.w 0x08043BBC`: IGN ON 캐시가 0이어도 종료** |

포트/핀 테이블은 원본 FLASH의 압축된 초기 RAM 데이터를 다시 복원해 확인했다. `HAL_GPIO_ReadPin`이라고 이름 붙인 함수 `0x080355F2`는 GPIO base+0x10을 읽어 mask를 검사하고 0/1을 반환한다. 심볼이 남아 있다는 뜻은 아니다.

[ph9-input-init.asm.txt](ph9-input-init.asm.txt)의 `0x08036EE8–0x08036EFC`는 PH9를 일반 GPIO 입력으로 초기화한다. Mode=0, 앞선 버튼 설정의 Pull=0을 유지한다. PH9 자체를 세 버튼의 EXTI처럼 설정하지 않는다.

의미를 정리한 의사 코드이며 원본 C 소스는 아니다:

```c
if (pin == PIN_12 || pin == PIN_15 || pin == PIN_6) {
    if (read_pin(GPIOH, PIN_9) == LOW) return;
    if (ign_on_cache == 0) return;
    record_button_press_or_release_time(pin);
}
```

일반 버튼 매핑은 UP=PD12, ENTER=PA15, DOWN=PI6이다. 이름까지의 연결 근거는 [기존 버튼 분석](../2026-09-12-buttons-bringup/stock-buttons.md)에 있다. IGN 캐시는 PG13 LOW→ON, HIGH→OFF를 11 tick 이상 지난 뒤 반영한다 (`0x08043CE0`의 전원 처리).

## 차단의 정확한 범위

| PH9 | IGN ON 캐시 | 일반 버튼 에지 기록 |
|---|---|---|
| LOW | 0 또는 1 | 무시 |
| HIGH | 0 | 무시 |
| HIGH | 1 | 기록 |

콜백 이전에 EXTI pending을 지우므로 차단된 에지는 나중에 자동 재생되지 않는다. PH9를 HIGH로 바꾸기만 해서는 이미 누른 버튼의 down 시각이 새로 생기지 않는다.

이 조건은 **새 에지 기록의 허용 조건**이다. 이미 기록된 down/up 타이머를 PH9 LOW로 바뀔 때 지우는 처리는 이 경로에 없다. [전체 주기 처리](button-and-power-poll.asm.txt)의 일반 버튼 루프는 PH9를 다시 검사하지 않는다. 따라서 허용 상태에서 누른 뒤 PH9가 LOW가 되어 해제 에지가 무시되면, 원본 입력 생성기는 최초 누름 후 2001 tick에 long 콜백을 호출할 수 있다. 이 결과를 상위 UI가 실제 어떤 기능으로 처리하는지까지 실행한 시험은 아니다.

IGN OFF→ON 순간 ENTER+DOWN이 함께 LOW이면 시작하는 index3 특수 조합은 별도 경로이고 PH9 검사를 거치지 않는다. 따라서 PH9 LOW를 모든 버튼 관련 동작의 절대 차단이라고 일반화하지 않는다.

## 현재 CFW와의 차이

현재 프로젝트 `../../STM32CubeProjects/FuckNudo_Noodoe_CFW_Project`의 다음 파일을 읽었다:

- `Drivers/BSP/src/BSP_Buttons.c:10`: 주석에 **PH9/IGN 정책 제외** 명시. `ReadPressed()`(68행)는 세 버튼 GPIO만 읽고, `BSP_Buttons_Process()`(207행)는 이 값을 polling하여 이벤트를 만든다.
- `Core/Src/gpio.c:156`: PH9를 INPUT/NOPULL로 초기화하지만 이것만으로 버튼 선택 정책이 구현되는 것은 아니다.
- `Middlewares/Noodoe/Input/src/ButtonEvents.c:23`: BSP 이벤트를 받아 구독자에 전달하며 PH9를 검사하지 않는다.
- `App_Logic/UI/src/product_ui.c:131,190`: 이벤트를 ProductUI_Button에 연결한다. 준비 상태/설치/결과 확인/화면 처리 조건은 있지만 PH9 선택 조건은 없다.
- `App_Logic/UI/src/product_input.c`: 버튼 종류를 UI 이벤트로 변환하고 Ui_Dispatch를 호출한다.

따라서 **현재 소스에서는 선택 신호에 따른 누도 입력 억제가 빠져 있다.** 사용자 장치에 설치된 CFW의 바이너리 해시를 이번에 읽은 것은 아니므로 현재 소스와 설치본의 byte 일치까지 주장하지 않는다.

수정 시에는 일반 주행 UI의 선택 상태를 명시적으로 관리하고, 선택 해제 시 진행 중인 누름/길게 누름/대기 이벤트를 취소하며, 재선택 시 이미 눌린 버튼은 해제 후 새 누름부터 받아들이는 정책을 검토할 수 있다. 이는 순정 에지 분기를 그대로 복제하는 것과는 다른 개선안이다. 진단·복구·설치 승인 버튼까지 동일하게 차단할지는 경로별로 정해야 한다. 이번에는 분석만 수행했다.

## 오프라인 검증

[verify.py](verify.py)는 원본 FLASH와 복원한 초기 RAM을 Unicorn에 올려 원본 Thumb 명령을 실행한다. 실제 GPIO 판독 함수와 HAL tick 판독 함수도 원본을 실행하고, GPIO IDR 및 tick 값만 모의 입력으로 제공한다. short/long 및 전원 이벤트 래퍼 진입을 기록한 뒤 반환하므로 하위 UI/큐/실제 하드웨어는 실행하지 않는다.

- 버튼3개 × PH9 두 상태 × IGN 캐시 두 상태 × 누름/해제 = **24개 조합 모두 예상 결과 일치**.
- 세 버튼의 정상/차단 누름-해제, 누름 중 선택 변경, 차단된 누름의 재선택, IGN ON 특수 조합 = **9개 시나리오 모두 통과**.
- 재현 결과: [verification.json](verification.json).
- 실차 전압, 하네스 net, 설치 CFW 해시, 물리 스위치 전환 시 PH9 실측은 이번 검증에 포함하지 않았다.
