# PD13 / PG14 / PI9와 BT 제어 GPIO 재감사

2026-09-15. 원본 전체 Flash A와 그 안에서 복원한 SRAM BL, V516 APP를 분석했다.
장치에는 접속하거나 GPIO를 쓰지 않았고, 현재 CFW도 변경하지 않았다.

## 후속 실물 정보: 떨어진 부품은 118번에 연결

사용자가 이후 떨어진0402가 MCU **118번에 연결된다**고 보고했다. 이는 기존의
"118번 근방"이라는 위치 정보보다 구체적이며, 아래의 PA8 근접 후보는 이를 반영해
수정한다. LQFP176의118번은 **PC9**이고 순정에서는 **조도 센서용 I2C3 SDA**다.
따라서 그 부품은 BT PA8 reset보다 조도 데이터선의 풀업/직렬 연결/필터 부품 후보로
우선 분류한다. 사용자 보고를 기록한 것이며 이번 턴에 직접 도통을 측정하지는 않았다.
정식 핀맵, 기존 실측과 다음 확인 항목은 [PC9 부품 기록](pc9-missing-component.md).

사용자가 이어서 **119번에 연결된 부품도 떨어져 있음**을 추가 보고했다.
따라서 현재는118번 PC9 관련 부품과119번 PA8 관련 부품의 **두 누락 위치**로 기록한다.
첫 부품을 PA8이라고 추정했던 것과 달리, 이번에는 별도의 PA8 연결 부품 누락 보고다.
119번 PA8은 순정 BT open/close가 직접 제어하는 출력이므로 BT 모듈측 reset/enable
전달 단절 가설을 우선 검토한다. 각 부품의 종류·반대 패드 연결·컨트롤러측 실제
파형은 아직 확인되지 않았다. [추가 기록](pc9-missing-component.md#추가-보고-119번-pa8-관련-부품도-누락).

## 결론

**PD13·PG14·PI9는 BT 전용 enable보다 보드 전원·재시작 제어 후보에 가깝다.**
반면 **PA8·PI1은 BT transport open/close와 직접 대응**한다. 어느 핀이 실제
BT IC의 nSHUTD에 연결되는지, 또는 레귤레이터 EN·트랜지스터·공유 전원 회로를
거치는지는 펌웨어만으로 확정할 수 없다. 전용 BT 제어가 따로 있다는 사실이
PD13/PG14/PI9가 BT에 공급되는 상위 전원에도 영향을 줄 가능성을 배제하지 않는다.

| 구간 | PD13 | PG14 | PI9 (보드 revision ≥3) |
|---|---|---|---|
| BL 공통 GPIO 초기화, APP 공통 GPIO 초기화 | HIGH | LOW | HIGH |
| 최종 OFF 요청이 PG13 HIGH 검사까지 통과 | LOW | HIGH | HIGH |
| 보드 재시작 함수 | 먼저 LOW | 중간에 LOW | HIGH → 지연 → PG14 LOW → 지연 → LOW |
| BT transport open/close 내부 | 이 세 핀에 대한 직접 쓰기를 찾지 못함 | 동일 | 동일 |

표는 **코드가 쓰는 값**이다. 외부 전압·전원 차단 완료의 실측이 아니다.
PI9는 정상 초기화와 OFF 모두 HIGH이므로 OFF 순간 새 상승 에지가 난다고 할 수 없다.
낮은 revision은 PI9 쓰기와 재시작의 PG14 LOW 부분을 건너뛴다. MCU silicon revision과
보드 strap revision은 별개다.

## 언제, 어떤 경로에서 제어되는가

### 1. BT보다 앞선 공통 부팅 초기화

BL `0x200002B0` 안의 전원 부분과 APP `0x08036962` 안의 전원 부분이 같은 출력
순서를 가진다. revision≥3이면 PG14 LOW → PI9 HIGH가 선행하고, 공통 부분에서
PG14 LOW → PD13 HIGH를 쓴다. BT 제어 핀의 초기값은 구분해야 한다.
PI1은 BL과 APP 모두 LOW지만, PA8은 BL `0x2000089E`에서 HIGH,
APP `0x08037002`에서 LOW다. 이후 APP의 BT open이 명시적으로 LOW→HIGH를 만든다.

| 쓰기 | BL call site | APP call site |
|---|---|---|
| revision≥3 PG14 LOW | `0x20000668` | `0x08036D28` |
| revision≥3 PI9 HIGH | `0x2000067A` | `0x08036D3A` |
| 공통 PG14 LOW | `0x200006B0` | `0x08036D60` |
| 공통 PD13 HIGH | `0x200006C2` | `0x08036D72` |

원시 근거: `boot-init-power.asm.txt`, `app-init-power.asm.txt`.

### 2. IGN OFF 입력 자체와 최종 종료는 다름

APP 전원 API `0x08031C26` → 함수표 `0x20000970 +0x10` → `0x0804261E`.
이 마지막 함수가 PG13을 **다시 읽어 HIGH일 때만** IRQ 차단 후 PD13 LOW
(`0x08042648`) → PG14 HIGH (`0x0804265A`) → revision≥3 PI9 HIGH
(`0x08042676`)를 쓴다. PG13 LOW이면 GPIO를 바꾸지 않고 `0x0C`로 반환한다.
끝의 16,000회 루프를 근거 없이 고정 ms로 환산하지 않는다.

상위는 OFF 이벤트/설정/검사 상태/타이머로 갈라진다. 기존 확인 경로에는 설정 key
0x13의 `0xA5A50000 → ...0002`와 타이머 인수60,000, 다른 분기의10,000 재시도가
있다. 따라서 **IGN OFF 즉시 무조건 세 핀을 바꾸는 구조가 아니다**.

BL 실패 서비스 `0x200045F6`도 PG13 HIGH일 때 같은 조합을 쓴다. 설치 실패 후
`0x200045F0` 루프에서 호출된다. 별도로 BL 보드 OFF 함수 `0x20002E02`에도 같은
출력이 있지만, 이번 조사에서 그 함수의 실제 호출 경로는 확정하지 못했다.
존재하는 함수와 도달이 확인된 실패 루프를 구분한다.

원시 근거: `app-off.asm.txt`, `boot-failure-off.asm.txt`, `boot-off.asm.txt`.
상위 상태의 근거는 [기존 전원 분석](../../docs/2026-09-10-noodoe-power-state-and-sleep.md).

### 3. 펌웨어 설치 인계·상태 전환의 시스템 재시작

APP `0x08031C68` → 함수표 `+0x20` → `0x080427FA` → `0x08043C50`.
이 함수는 다음을 수행한다.

1. IRQ 차단, PD13 LOW.
2. revision≥3: PI9 HIGH → 16,000회 루프 → PG14 LOW → 같은 루프 → PI9 LOW → 같은 루프.
3. SCB AIRCR에 `0x05FA0004`, 즉 SYSRESETREQ 기록.

상위 API는 ON 캐시가 참이거나 revision≥3일 때 이 경로를 허용한다.
OFF/revision<3이면3을 반환하며 GPIO 재시작 함수를 호출하지 않는다.
확인된 사용례에는 OTA 설치 요청 인계 `0x08021B46`, OFF 타이머 정리 후
`0x080329D6`, OFF 상태에서 ON 복귀를 처리하는 일부 분기 등이 있다.
직접 caller 후보들은 `callers.json`에 보존했다. 모든 caller의 상위 UI 의미를
이 조사에서 확정한 것은 아니다.

### 4. 새로 재확인: CPU fault도 같은 GPIO 재시작을 사용

V516 APP 벡터3..6은 각각 `0x08070D57`, `0x08070D5F`, `0x08070D67`,
`0x08070D6F`다. Thumb bit를 제외한 HardFault/MemManage/BusFault/UsageFault
진입점이 모두 `0x08043C50`을 직접 호출한다. 위 전원 API의 IGN 조건을 거치지 않는다.
따라서 순정에서 **MCU fault가 발생해도 세 GPIO의 재시작 시퀀스가 나타날 수 있다**.
이 에지를 관측했다고 무조건 사용자의 IGN 조작이나 BT reset으로 해석하면 안 된다.
원시 근거: `fault-reset.asm.txt`, `app-reset.asm.txt`.

## BT 전용 경로와 비교

BT transport open `0x0803E73A`의 실제 준비 구간:

```text
0x0803E82A: PA8 LOW
0x0803E83C: PI1 HIGH
USART1 / RX DMA 준비
delay(10)
0x0803E954: PA8 HIGH
delay(150)
상위 HCI 초기화 진행
```

close에는 PA8 LOW (`0x0803EAFC`) → transport/task/USART 정리 → PI1 LOW
(`0x0803EB3C`)가 있다. open은 자원 준비가 성공한 분기에서만 GPIO 구간으로 들어간다.
원시 근거: `bt-open-start.asm.txt`, `bt-open-release.asm.txt`, `bt-close.asm.txt`.
ms 해석은 [기존 통신 감사](../2026-09-12-full-ioc-map/communications-audit.md)의
delay 함수 분석과 대조했다. 호스트 재현은 delay 인수와 순서를 검증하며 물리 시간을 재지 않는다.

PA8은 reset/nSHUTD 계열, PI1은 enable/전원 계열 동작에 부합한다는 **기능 추론**이다.
실제 net 이름은 미확정이다. 현재 CFW `BSP_BT_HCI.c`도 이 두 GPIO 순서를 이미 사용한다.

또한 [9월12일 순정 실제 부팅 기록](../2026-09-12-bt-als-focus/stock-full-boot-probe-01/README.md)에서
HCI Reset 대기 실패 직전에 **PA8 HIGH, PI1 HIGH, CTS HIGH, RX NDTR256**가 관측됐다.
따라서 당시 실패를 단순히 MCU가 PA8/PI1을 올리지 않은 것으로 설명할 수는 없다.
그 측정은 MCU 쪽 IDR이며, BT 칩 쪽 전압·전원·slow clock 정상의 증거는 아니다.

## 실물에서 우선 확인할 연결

기존 [LQFP176 핀 매핑](../2026-09-12-full-ioc-map/pin-map.md) 기준:

| GPIO | MCU 다리 번호 | 현재 우선 해석 |
|---|---:|---|
| PD13 |101|보드 전원 유지/종료 제어 후보|
| PG14 |157|보드 종료/재시작 제어 후보|
| PI9 |11|revision별 재시작 관련 출력|
| PA8 |119|BT reset-release 제어 후보|
| PI1 |132|BT enable 제어 후보|

후속 정보에서 해당0402는118번 PC9에 연결된다고 보고됐으므로 그 부품에 대해서는
119번 PA8 근접 후보를 철회한다. 부품 종류는 반대 패드의 연결을 확인해야 한다.
그 뒤 별도로119번 PA8 연결 부품의 누락도 보고됐으므로 두 위치를 구분해 확인한다.
BT에 대한 별도 판별은 PA8/PI1 MCU측과 BT모듈측의 통전·전압 대응, 그리고 PD13/PG14/PI9가
연결되는 레귤레이터/트랜지스터 식별이다. 모듈측 supply가 이 셋 중 하나의 하위라면
BT에 간접 영향을 줄 수 있다. 이번 작업에서는 이 GPIO를 실기기에서 바꾸는 실험을 하지 않았다.

## 감사 범위와 재현

- 전체 A SHA256: `38207bed741a8fef9e6d2ed24b86c8cb8b376ba5e4c957163f5edd83fbeb8037`.
- APP가 V516 OTA와 일치하고 마지막4B만FF인 것을 다시 assert했다.
- `audit.py`: 각 halfword에서 독립 decode하여 GPIO WritePin 직접 호출을 검색.
  APP59개, 복원 BL45개, Flash BL prefix0개. 포트/마스크는 전부 복원했다.
  대상 세 핀의 쓰기는 APP11곳, BL10곳이며 위 시퀀스에 모인다.
- `calls.json`의 일부 level은 보수적 국소 상수 전파로 unresolved다. 이를 추측으로
  채우지 않고 `sequences.json`의 원본 실행 및 ASM으로 대상 시퀀스를 검증했다.
- GPIO register literal/ODR·BSRR bit-band 후보도 확인했다. 직접 MMIO 계산이나
  alias·간접 함수 포인터를 모두 배제하는 수학적 전수 증명은 아니다.
- `sequences.py`: 원본 ARM 명령을 simulated MMIO에서 실행. rev2/3, IGN LOW/HIGH,
  부팅/종료/실패루프/재시작/BT open-close 및4 fault의 **25개 조건 통과**.
  실제 GPIO WritePin/ReadPin 명령과 BSRR 쓰기를 실행했다. GPIO 설정, RTOS 대기와
  일부 외부 service는 stub이므로 하드웨어 전기 특성·전체 부팅 성공을 검증한 것은 아니다.

재현: 작업 루트에서 Python으로 `audit.py`, 이어서 `sequences.py`를 실행한다.
두 스크립트는 원본을 읽고 이 분석 폴더에만 출력하며 ST-LINK나 시리얼에 접근하지 않는다.
