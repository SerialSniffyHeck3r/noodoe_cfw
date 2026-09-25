# AK550 SR1.5 V5.16 UART 바이너리 재감사

작성: 2026-09-09 KST. 원본 BIN을 읽기만 했으며, 차량·계기판·디버거·USB 장치에는 접속하지 않았다.

## 적용 범위와 재현

- 원본: `artifacts/ota-archive/2026-08-31-full/blobs/firmware/1657088080998-s1-SR1.5_ota_V516.bin`
- SHA-256: `3b64673054ca84b9cf504f4cc7ebcb2e6615b9fbf59df79a37a92dcf14f037ca`
- 크기 458,748 bytes, 분석 기준 주소 `0x08010000`.
- 도구: Python 3.12 runtime, workspace 전용 Capstone 5.0.9, Thumb + little-endian + MCLASS.
- 아래 주소는 이 SHA의 이미지 전용이다. 구입한 2017년식 AK550 모듈이 동일 revision/이미지인지 아직 확인하지 않았다.
- 2018년식 실차와 2017년식 구입 계기판은 AK550용이라는 사용자 정보가 있다. 별도 장치인 차량 계기판과 Noodoe 사이 물리 핀아웃은 미확인이다.

```powershell
& '<local-user>\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' .\analysis\2026-09-09-ak550-uart-audit\audit_uart.py
```

스크립트는 이미지 SHA/크기를 검사한 뒤 원시 바이트 포함 disassembly와 `verified-anchors.json`을 생성한다. 이번 실행에서 21개 주소의 바이트를 확인했다. 임의 범위는 `audit_uart.py 0x0804EA14 0x17A`, 직접 branch 참조는 `audit_uart.py xref 0x08043C08`로 재확인할 수 있다.

**범위 disassembly 전체를 실행 코드라고 해석하면 안 된다.** 함수 사이 literal pool/정적 데이터는 명령처럼 출력될 수 있다. 아래 결론은 실제 분기·호출·메모리 접근이 이어지는 구간만 사용했다. 함수명은 심볼이 없는 바이너리에 붙인 기능상 이름이다.

## 결론: 기존 UART 설명에서 고칠 세 부분

1. UART5의 DMA 설정은 존재하지만, 실제 F5 수신 분석에는 **1바이트씩 인터럽트로 받는 경로**가 확인된다. `DMA가 설정됨`과 `F5 parser가 DMA ring을 소비함`을 구분해야 한다.
2. `0x41=250 bytes`, `0x42=71 bytes`는 **consumer의 고정 복사 길이**다. 순정 parser가 이 길이를 검사한다는 의미가 아니다.
3. USART1에 3,686,400을 넣는 초기화 상수는 확인되지만, 곧바로 **호출자 지정 baud로 BRR를 다시 계산**한다. 상수 하나로 초기·최종 실효 baud를 모두 확정해서는 안 된다.

세 항목 모두 문서에서 출발했지만 현재 BIN의 원시 명령으로 재확인한 결과다.

## UART5: 설정과 실제 수신 경로

### 설정 자체는 확인됨

| 항목 | 원시 증거 | 판정 |
| --- | --- | --- |
| UART5 base `0x40005000` | `0x0804367C`: `df f8 98 09`, literal `0x08044018` | 직접 확인 |
| handle `0x2002289C` | `0x08043680`: `df f8 90 19`; `0x08043684`: `08 60` | 직접 확인 |
| baud 115200 | `0x08043686`: `5f f4 e1 30` = `movs.w r0,#0x1c200`; handle+4에 저장 | 직접 확인 |
| 8N1, TX/RX, flow 없음, oversampling16 | `0x08043690..436BE`: word length/stop/parity/flow/oversampling=0, mode=0x0c | HAL 구조 및 레지스터 의미 대조 |
| UART5 clock | `0x08037A32`: `50 f4 80 10`, RCC APB1ENR `0x40023840`에 bit20 | 직접 확인 |
| DMA1 clock | `0x08037A54`: `50 f4 00 10`, AHB1ENR에 bit21 | 직접 확인 |
| TX DMA1 Stream7 | `0x08037AA4`: base `0x400260B8` → handle `0x200222BC` | 직접 확인 |
| TX Channel4 | `0x08037AAE`: `5f f0 00 60` = `0x08000000`, handle+4 저장 | 직접 확인 |
| RX DMA1 Stream0 | `0x08037B2A`: base `0x40026010` → handle `0x2002231C` | 직접 확인 |
| RX Channel4 | `0x08037B34`: `5f f0 00 60`, handle+4 저장 | 직접 확인 |
| GPIO AF8 | `0x08037A80`: `08 20`; `0x08037A8C/37AA0` GPIO init 호출 | 직접 확인 |

기존 문서의 `PC12 TX/PD2 RX`는 MCU 신호 후보로 유지할 수 있다. 다만 이 초기화 함수는 GPIO port/pin 값을 `0x20000998`, `0x20000A20`의 RAM 테이블에서 가져온다. 이번 감사에서는 IAR 초기화 테이블 전체 복원과 revision별 변경까지 새로 닫지 않았으므로, 이 두 신호를 실제 커넥터 번호로 바꾸거나 구입 PCB의 확정 핀아웃이라고 쓰지 않는다.

### 1바이트 인터럽트 경로의 명령 증거

`0x08043714`는 수신 버퍼 포인터를 받아 다음을 실행한다.

```text
0x08043728  01 22          movs r2,#1
0x0804372A  21 00          movs r1,r4
0x0804372C  df f8 e4 08    ldr.w r0,[pc,...] ; UART5 handle
0x08043730  08 f0 cc fc    bl 0x0804C0CC
```

`0x0804C0CC`는 버퍼 포인터·전송 개수를 handle에 저장하고 CR3 EIE, CR1 RXNEIE/PEIE를 켠다. 특히 `0x0804C11A`의 `51 f4 90 71`는 `orrs r1,r1,#0x120`이다. 여기서는 DMA stream을 시작하지 않는다.

수신 interrupt에서 F5 parser까지 이어지는 직접 호출은 다음과 같다.

```text
UART5 vector -> 0x08070E68
  0x08070E6C -> 0x0804C25C (UART IRQ 처리)
  0x0804C282 -> 0x0804C55C (RXNE 처리)
    0x0804C5A4..5A8 : USART_DR 읽고 버퍼에 byte 저장
    0x0804C5BC..5C6 : 남은 개수 감소, 0인지 확인
  0x0804C5E6 -> 0x08043C08 (완료 callback)
  0x08043C14 -> 0x0804E7C2 (F5 수신 dispatcher)
  0x0804E7CA -> 0x0804EA14 (F5 state machine)
```

F5 state machine은 다음 바이트를 받을 위치를 operation table의 callback으로 다시 전달한다. 이 간접 함수 포인터 테이블의 초기화 전체를 이번에 복원하지 않았다는 한계는 남지만, **RXNE interrupt에서 parser까지의 직접 호출 경로와 1바이트 receive-IT wrapper는 각각 확인됐다.** 따라서 단순히 non-default DMA IRQ가 있다는 이유로 `순정 F5 RX = DMA ring`이라고 단정할 수 없다. 커스텀 펌웨어에서 DMA ring을 선택하는 것은 별도의 구현 설계다.

## F5 parser: 정상 형식, 허용 명령, 길이 검사

wire 형식은 `F5 | CMD | LEN | PAYLOAD[LEN] | XOR`로 재확인된다.

| 동작 | 주소 / raw bytes |
| --- | --- |
| `F5` 시작 검사 | `0x0804EA3C`, `f5 28` |
| `0x21` 허용 | `0x0804EA60`, `21 28` |
| `0x41` 허용 | `0x0804EA6A`, `41 28` |
| `0x22` 허용 | `0x0804EA74`, `22 28` |
| `0x42` 허용 | `0x0804EA7E`, `42 28` |
| LEN=0 거절 | `0x0804EAAA`, `00 2d`; `0x0804EAAC`, `10 d0` |
| XOR 누산 | `0x0804EBBC`, `62 40` = `eors r2,r4` |
| checksum 비교 | `0x0804EB38`, `81 42`; 불일치 분기 `0x0804EB3A` |
| checksum 통과 후 payload 복사 | `0x0804EB44..4EB4E`, 길이는 입력 LEN |

중요한 차이:

- 순정은 위 네 수신 명령 이외를 command 단계에서 거절한다.
- LEN을 읽을 때 비어 있지 않은지만 검사한다. `0x41`에서 LEN=250 또는 `0x42`에서 LEN=71인지 확인하는 분기는 이 parser/dispatcher에 없다.
- 완료 후 `0x0804E7C2`는 CMD와 payload만 받고, 수신 LEN을 consumer에게 전달하지 않는다.
- `0x0804E976`는 250 bytes를 고정 복사하며, `0x0804E992`는 71 bytes를 고정 복사한다. `0x0804E920`는 telemetry의 9개 필수 byte와 `0x22`의 추가 2개 byte를 읽는다.

따라서 기존 문서의 250/71은 **정상 메시지에서 기대하는 크기 및 순정 consumer 크기**로 읽어야 한다. 짧은 LEN이 유효한 체크섬과 함께 들어오면 소비 함수가 수신된 길이보다 큰 영역을 읽을 수 있다는 정적 결함 후보가 있다. 여기서는 입력을 보내거나 실물 영향·코드 실행 가능성을 검증하지 않았다. 백업 경로가 발견됐다는 의미도 아니다.

추가로 `LEN+3`을 checksum 함수에 넘기기 직전 `0x0804EB26`에서 `uxtb r1,r1`로 8비트 축소한다. 정상 250-byte payload에서는 문제 없지만, 253..255 영역까지 일반적인 XOR 형식과 동일하다고 가정하면 안 된다. 로컬 `src/noodoe_protocol/vehicle_uart.py`는 모든 command와 0-byte payload도 generic frame으로 취급하고 길이 전체의 XOR를 계산한다. 이는 캡처 분석기로서 가능한 설계이며, 순정 수신기의 거절 정책을 그대로 재현하는 구현은 아니다.

`0x41` payload의 후속 저장은 `0x080332A6`에서 `0x20021A40`에 250 bytes, `0x42`는 `0x080332BC`에서 `0x200227D4`에 71 bytes이며 기존 SPP bridge 분석과 일치한다.

## USART1: Bluetooth HCI 경로의 설정과 baud 수정

| 항목 | 이번 raw evidence |
| --- | --- |
| USART1 base | `0x0803E840` loads `0x40011000` |
| handle | `0x0803E844` loads `0x2002285C` |
| HAL init baud 상수 | `0x0803E84A`, `5f f4 61 10` = `0x384000` = 3,686,400 |
| RTS/CTS | `0x0803E86C`, `4f f4 40 70` = `0x300`, handle+0x18 저장 |
| DMA2 clock | `0x0803787E`, AHB1ENR bit22 |
| TX DMA2 Stream7 | `0x08037896` literal `0x400264B8` |
| TX Channel4 | `0x080378A0`, `5f f0 00 60` -> handle+4 |
| RX DMA2 Stream5 | `0x0803791A` literal `0x40026488` |
| RX Channel4 | `0x08037924`, `5f f0 00 60` -> handle+4 |
| AF7 설정 | `0x080377DC`, `07 20` |

기존 Sept.1 문서의 USART1 DMA Channel4 주장은 실제 초기화 상수로 지지된다. Sept.5 설정서의 `DMA channel number 미확정`은 **이 이미지의 설정값에 한해서** Channel4로 보완할 수 있다.

그러나 baud는 아래 순서를 주의해야 한다.

```text
0x0803E88A  0d f0 cb fb    bl 0x0804C024       ; HAL UART init
0x0803E88E  a1 68          ldr r1,[r4,#8]      ; open 인자의 baud
0x0803E890  df f8 8c 04    ldr.w r0,[pc,...]  ; USART1 handle
0x0803E894  00 68          ldr r0,[r0]        ; USART1 base
0x0803E896  ff f7 51 fc    bl 0x0803E13C      ; BRR 재설정
```

`0x0803E13C`는 PCLK 조회 결과와 전달된 baud를 이용해 나눗셈하고, 필요에 따라 OVER8을 변경한 뒤 `0x0803E182` (`a0 60`)에서 USART_BRR에 직접 저장한다. `0x0803EB64`의 재설정 함수도 조건이 맞으면 요청 구조체의 +8 baud를 같은 함수로 보낸다.

**새 표현:** `USART1 HCI는 RTS/CTS 및 DMA2 S5/S7 Ch4 경로를 사용하며, HAL 초기값은 3,686,400이고 open/control 인자에 따른 baud 재설정 코드가 있다.` 호출자 인자의 초기화와 TI controller baud 변경 명령까지 함께 추적하거나 실물 파형을 재기 전에는 통신 전체를 단일 baud라고 확정하지 않는다. PA9/10/11/12와 실물 커넥터 위치의 구분도 UART5와 동일하다.

## 차량 UART와 부트로더 백업을 연결할 수 있는가

이번에 확인한 **실행 중 애플리케이션**의 UART5 F5 dispatcher에는 `0x21/0x22/0x41/0x42` 네 갈래가 있고, 내용은 telemetry/profile/status 소비다. 여기서 명시적인 메모리 읽기·내부 flash dump·bootloader 진입·펌웨어 데이터 쓰기 명령은 찾지 못했다. 송신 `0x01` enum-to-one-hot 요청과 `0xA1` 2-byte builder도 확인된다. 후속 [페이로드 분석](../../docs/2026-09-09-ak550-uart-payload-map.md)에서 `A1`은 조도 단계, `01=04`는 통신 재요청 경로와 연결했다. 백업 명령으로 볼 근거는 없다.

이것은 하위 flash의 별도 KYMCO bootloader가 UART5를 사용하지 않는다는 증명이 아니다. 해당 코드는 현재 OTA app 이미지 밖에 있다. 또한 STM32 ROM bootloader의 UART 지원 여부는 **칩별 ROM 인터페이스 표**의 문제이며, 현재 애플리케이션이 UART5로 계기판과 통신한다는 사실로 ROM에서 UART5를 지원한다고 유추하면 안 된다.

당장 bootloader 백업 방법을 설계할 때는 SWD 읽기 경로를 우선 유지한다. UART 대안을 열려면 정확한 MCU/ROM 버전, ROM이 지원하는 실제 UART instance, BOOT0/NRST 접근, 보드에서 그 net의 접근 여부, 읽기 보호 상태를 별도로 확인해야 한다. F5 parser의 길이 처리 결함 후보는 이 검증을 대신하지 않는다.

## 부트·업데이트 담당 분석에 대한 독립 확인

다른 분석에서 요청한 `0x080427C4`를 원본에서 별도로 읽었다. raw `17 f5 80 50`는 `adds.w r0,r7,#0x1000`이다. 직전 `r9=r7`, loop마다 `r9+=4`, `cmp r9,r0; bhs` 구조를 확인했다. 따라서 이 루프의 주소 범위는 **0x1000=4KiB**이며 0x4000=16KiB가 아니다. 전체 sector-preservation 성질은 해당 함수의 복사·erase 경로와 함께 별도 부트 분석 보고서를 기준으로 판단한다.

## 문서 대조와 다음 실물 기록

| 기존 문서 | 이번에 반영할 차이 |
| --- | --- |
| `docs/ak550-sr15-uart-ota-bluetooth-display-io-bringup.md` §4 | 순정 RX의 byte interrupt 경로와 DMA 설정을 분리; DMA ring은 custom 설계로 표시 |
| 같은 문서 §4.3 및 `docs/vehicle-and-external-protocol.md` | 250/71는 consumer 크기이며 stock LEN 검증 근거로 쓰지 않음 |
| `docs/ak550-sr15-mcu-peripheral-settings.md` §5.2 | USART1 Channel4 설정을 raw로 확정; 3,686,400과 실제 BRR 재설정을 구분 |
| `src/noodoe_protocol/vehicle_uart.py` | generic 분석기의 허용 범위가 stock RX whitelist/zero-length 정책과 다름; 코드 변경은 이번 범위 밖 |

실물에서 기록할 최소 추가 항목은 MCU 모델·board revision, SWD 접점 연속성, 정상 전원 상태의 UART5/USART1 파형, 그리고 SWD로 얻는 실제 BRR/CR1/CR3 및 DMA stream register snapshot이다. 이 정보가 바이너리의 초기화 설정과 실제 동작을 연결한다.

공식 의미 대조 자료: [ST STM32F4 HAL UART source](https://raw.githubusercontent.com/STMicroelectronics/stm32f4xx-hal-driver/master/Src/stm32f4xx_hal_uart.c). 현행 공개 소스는 interrupt 수신과 DMA 수신을 별도 경로로 명시한다. 이 바이너리와 같은 HAL version이라고 주장하는 근거로 사용하지 않았으며, 위 결론은 원시 레지스터 조작과 call path에서 도출했다.
