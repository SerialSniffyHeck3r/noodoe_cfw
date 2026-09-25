# AK550 Noodoe SR1.5 MCU 및 Peripheral 설정서

> **2026-09-09 재검증:** SR1 CRC 재현, 하위 flash의 16KiB shadow/4KiB program 구분, UART5 byte-IRQ 수신 및 USART1 baud 재설정에 대한 정정은 [최신 BIN·APK 대조 보고서](2026-09-09-ak550-firmware-uart-boot-review.md)를 우선한다. 아래는 당시 분석 기록이며, 충돌하는 설명을 실물 작업의 확정 근거로 사용하지 않는다.

이 문서는 **AK550용 SR1/SR1.5 STM32F4 보드**만 대상으로 한다. CV3용
SR2/STM32H7 자료는 사용하지 않는다. 목적은 실물 보드를 받은 날 원본을 먼저
보존하고, 확인된 설정만으로 RAM bring-up과 clean-room BSP 작성을 시작하는 것이다.

기계 판독 원본은
`hardware/ak550-sr15-v516-mcu-peripheral-settings.json`이다.

## 1. 적용 조건

| 항목 | 값 |
| --- | --- |
| 기준 firmware | `SR1.5_ota_V516.bin` |
| SHA-256 | `3b64673054ca84b9cf504f4cc7ebcb2e6615b9fbf59df79a37a92dcf14f037ca` |
| 파일 크기 | `458748` bytes (`0x6FFFC`) |
| application base | `0x08010000` |
| application last byte | `0x0807FFFB` |
| initial MSP | `0x20025318` |
| reset handler | raw `0x08076719`, code `0x08076718` |
| MCU | STM32F429xx-class Cortex-M4F, 정확한 suffix/package 미확정 |

이 주소표는 해당 해시의 이미지에만 고정된다. 중고 보드의 firmware가 V5.16이더라도
이미지 해시를 읽기 전에는 같은 주소라고 가정하지 않는다.

## 2. 이번 재검증에서 고친 점

기존 자동 분석은 vector가 application 범위를 가리키면 모두 활성 IRQ로 셌다.
그러나 100개 application-range vector 중 73개는 다음 4-byte IAR weak handler다.

```text
FF F7 FE BF    b.w .
```

즉 `SPI1`, `SPI4`, `SPI5`, `USART1`, `I2C1_EV/ER`, `I2C3_EV/ER`,
`OTG_FS`에 vector 주소가 있다는 것만으로 interrupt driver가 있다는 결론은 틀렸다.

실제 비기본 external IRQ는 다음 17개다.

```text
EXTI2 EXTI3 EXTI4 EXTI9_5 EXTI15_10
DMA1_Stream0 DMA1_Stream7
DMA2_Stream0 DMA2_Stream3 DMA2_Stream4 DMA2_Stream5 DMA2_Stream7
TIM2 TIM5 RTC_Alarm UART5 OTG_HS
```

따라서 첫 BSP는 다음처럼 구현한다.

| peripheral | 순정에서 확인된 delivery 방식 |
| --- | --- |
| UART5 | UART IRQ + DMA1 Stream0/7 |
| USART1 | 직접 IRQ는 default, DMA2 Stream5/7 |
| SPI1 | 직접 IRQ는 default, 우선 polling |
| SPI4 | 직접 IRQ는 default, polling |
| SPI5 | 직접 IRQ는 default, DMA2 Stream0/3/4 후보 |
| I2C1/I2C3 | EV/ER IRQ는 default, polling |
| USB | OTG_HS만 비기본, OTG_FS 계열은 default |

전체 자동 생성표는
`analysis/2026-09-01-custom-firmware-hardware-map/IRQ-MAP.md`에 있다.

## 3. MCU, startup, memory

### 3.1 확인된 구조

- Cortex-M vector 순서가 STM32F429xx와 일치한다.
- `SystemInit` 후보 `0x08073E90`은 `SCB->VTOR=0x08010000`을 쓴다.
- IAR EWARM startup과 C runtime을 거쳐 application으로 들어간다.
- FreeRTOS + CMSIS-RTOS v1, tick 1 kHz 구조다.
- `BASEPRI=0x50`, priority 단계 12개, heap 약 64 KiB, stack overflow check 2다.
- UID word는 `0x1FFF7A10`, `0x1FFF7A14`, `0x1FFF7A18`에서 읽는 경로가 있다.

### 3.2 flash 경계

```text
0x08000000..0x08007FFF  boot/recovery 후보, OTA 이미지에 없음
0x08008000..0x0800FFFF  sector 2/3, persistent/update state 후보
0x08010000..0x0807FFFB  보존된 V5.16 application
```

하위 64 KiB는 “빈 공간”이 아니다. 실제 donor dump 전에는 bootloader, option
policy, persistent state의 정확한 경계를 알 수 없다. 커스텀 linker script는 처음부터
`0x08010000`을 application base로 쓰고 하위 영역을 절대 erase하지 않는다.

정확한 flash 총용량은 firmware 크기로 추정하지 말고 실물의 flash-size register와
MCU marking으로 정한다.

### 3.3 RAM

- initial MSP `0x20025318`은 STM32F429 SRAM 주소 범위와 일치한다.
- 일반 SRAM 외에 `0x10000000` CCM SRAM allocator 흔적이 있다.
- CCM은 DMA가 접근할 수 없으므로 DMA buffer를 CCM에 두면 안 된다.
- 첫 linker script의 DMA section은 일반 SRAM에 명시적으로 배치한다.

## 4. Clock 설정

### 4.1 직접 확인된 `SystemInit`

`0x08073E90`에서 다음 동작을 확인했다.

| register/effect | 값 또는 동작 |
| --- | --- |
| `RCC_CR` | HSI ON |
| `RCC_CFGR` | 0으로 초기화 |
| `RCC_CR` mask | `0xFEF6FFFF` 적용 |
| `RCC_PLLCFGR` | reset value `0x24003010` |
| HSE bypass | clear |
| `RCC_CIR` | clear |
| `SCB_VTOR` | `0x08010000` |

이것은 reset baseline이지 최종 clock tree가 아니다.

### 4.2 아직 쓰면 안 되는 값

SYSCLK 180 MHz는 현재 **미확정**이다. 이미지 안의 유일한 `180000000` literal은
`0x08057DB2`의 USB IRQ 경로에서 load 직후 다른 값으로 덮여 쓰이므로 clock 증거가
아니다. HSE crystal, PLLM/N/P/Q, AHB/APB prescaler, flash latency를 추측해서 넣지 않는다.

순정 firmware를 SWD로 halt한 뒤 아래 register를 한 번에 저장해야 최종 clock을
복원할 수 있다.

```text
RCC_CR RCC_PLLCFGR RCC_CFGR RCC_CIR
RCC_AHB1ENR RCC_AHB2ENR RCC_AHB3ENR
RCC_APB1ENR RCC_APB2ENR
FLASH_ACR PWR_CR PWR_CSR
```

추가로 crystal 양단을 probe하지 말고 먼저 캔 marking을 촬영한다. 필요할 때만
high-impedance active probe로 주파수를 확인한다.

## 5. Peripheral 설정표

### 5.1 UART5: 차량/미터 링크

| 항목 | 값 |
| --- | --- |
| baud/format | 115200, 8N1 |
| mode/flow | TX/RX, no flow control, oversampling 16 |
| TX/RX | PC12 AF8 / PD2 AF8 |
| RX DMA | DMA1 Stream0 Channel4 |
| TX DMA | DMA1 Stream7 Channel4 |
| handle | `0x2002289C` |
| IRQ | UART5 `0x08070E68`, DMA1 S0 `0x08070DAA`, S7 `0x08070E1C` |

첫 구현은 RX pin을 입력 상태로 두고 passive capture만 한다. 송신 enable은 실제
connector 방향과 3.3 V 여부를 확인한 뒤 별도 build flag로 연다.

### 5.2 USART1: Bluetooth HCI

| 항목 | 값 |
| --- | --- |
| runtime baud | 3,686,400, 8N1 |
| flow | RTS/CTS |
| TX/RX | PA9 AF7 / PA10 AF7 |
| CTS/RTS | PA11 AF7 / PA12 AF7 |
| DMA | DMA2 Stream5 RX, Stream7 TX |
| IRQ | USART1 direct IRQ는 default; DMA2 S5/S7만 비기본 |
| 상대 부품 | TI CC256x/CC2564 계열 후보, marking 미확정 |

3,686,400은 patch download 이후 runtime 설정일 수 있다. controller reset 직후의
초기 baud, shutdown/reset GPIO, DMA channel number, HCI service-pack 순서는 실물
logic capture 전까지 고정하지 않는다.

### 5.3 SPI1: FT81x EVE host

| 항목 | 값 |
| --- | --- |
| SCK/MISO/MOSI | PB3 AF5 / PA6 AF5 / PB5 AF5 |
| transfer | direct IRQ가 default이므로 우선 polling |
| ID probe | `REG_ID(0x302000) == 0x7C` |
| command FIFO | READ `0x3020F8`, WRITE `0x3020FC`, mask `0x0FFC` |

정확한 FT810/811/812/813 variant, divider, data width, CS, PDN, INT pin은 열려 있다.
첫 driver는 낮은 SPI clock과 read-only `REG_ID`만 수행한다.

### 5.4 SPI4: LCD DDIC control

| 항목 | 값 |
| --- | --- |
| SCK/MISO/MOSI | PE2 AF5 / PE5 AF5 / PE6 AF5 |
| CS/control 후보 | PE4 GPIO |
| format | master, 16-bit, mode 0, prescaler 16 |
| transfer | polling; SPI4 IRQ는 default |
| 확인된 명령군 | `0x10`, `0x11`, `0x28`, `0x29`, `0x0A` |
| init 골격 | Sleep Out `0x11`, 약 120 ms, Display On `0x29` |
| status 후보 | `0x0A` read 결과 `0x9C` |

`PI11`, `PC13`은 reset/control 후보지만 아직 drive 금지다. PE4가 CS인지 D/C인지,
active polarity가 무엇인지 passive capture로 닫기 전에는 `0x11/0x29`도 보내지 않는다.

### 5.5 SPI5: 128 MiB resource NOR

| 항목 | 값 |
| --- | --- |
| CS/SCK/MISO/MOSI | PF6 GPIO / PF7 AF5 / PF8 AF5 / PF9 AF5 |
| format | master, 8-bit, mode 0, prescaler 2 |
| safe CS | PF6 HIGH |
| JEDEC | `C2 20 1B` |
| geometry | 128 MiB, 4 KiB sector, 256-byte page |
| DMA | DMA2 Stream0/3/4 비기본 handler 후보 |
| direct IRQ | SPI5 IRQ는 default |

첫 명령은 `0x9F RDID`, `0x05 RDSR`, `0x03/0x13 READ`만 허용한다. `WREN`, program,
erase는 128 MiB 전체를 두 번 덤프해 SHA-256이 일치하기 전까지 코드에서 compile-out한다.

### 5.6 I2C1: MFi AuthCP

| 항목 | 값 |
| --- | --- |
| speed/address | 100 kHz, 7-bit `0x11` |
| SCL/SDA | PB6 AF4 OD / PB7 AF4 OD |
| reset | PH13 active-low 후보, safe boot HIGH |
| transfer | polling; EV/ER IRQ는 default |
| register set | `0x10/11/12`, `0x20/21`, `0x30/31` |

첫 bring-up에서는 address ACK와 read-only 상태만 본다. reset pulse 시간과 exact AuthCP
part를 확인하기 전에는 PH13을 임의로 토글하지 않는다.

### 5.7 I2C3: OPT3001

| 항목 | 값 |
| --- | --- |
| speed/address | 400 kHz, 7-bit `0x45` |
| SCL/SDA | PH7 AF4 OD / PC9 AF4 OD |
| transfer | polling; EV/ER IRQ는 default |
| manufacturer/device ID | `0x5449` / `0x3001` |

ID read가 가장 안전한 주변장치 생존 검사다.

### 5.8 TIM5: backlight

| 항목 | 값 |
| --- | --- |
| timer handle | `0x2002295C` |
| IRQ | nontrivial `0x08070E28` |
| counter 후보 | 약 50 kHz |
| ARR 후보 | 99 |
| PWM 후보 | 약 500 Hz, 100단계 |

정확한 channel, pin, polarity, 외부 LED driver는 미확정이다. 첫 custom image에서는
TIM5 PWM GPIO를 설정하지 않는다. 순정 동작 중 brightness 0/50/100을 바꾸며 해당
파형과 연결된 pin을 찾은 뒤 low-duty부터 활성화한다.

### 5.9 USB OTG HS

| 항목 | 값 |
| --- | --- |
| core | USB_OTG_HS |
| mode 후보 | HS core + embedded FS PHY |
| EP0 | 64 bytes 후보 |
| handle | `0x2001D7CC` |
| IRQ | OTG_HS `0x08070E96`만 비기본 |

OTG_FS, FS wakeup, HS wakeup, EP1 IN/OUT vector는 default다. D+/D-, VBUS, connector,
ULPI PHY 유무, VID/PID/descriptor를 실물에서 닫기 전에는 USB recovery를 설계 완료로
보지 않는다.

### 5.10 FMC/SDRAM

FMC 주소 참조와 display/resource 작업용 외부 memory 정황은 있으나 chip, geometry,
pinout, timing이 모두 미확정이다. FT81x가 scan-out을 담당하므로 LTDC/DMA2D를 화면
출력 경로로 켜지 않는다. SDRAM은 부품 marking과 address/data trace를 확보한 뒤
독립 테스트한다.

## 6. GPIO 및 EXTI

비기본 EXTI group은 `EXTI2/3/4/9_5/15_10`이다. 다음 edge 설정은 firmware 분석상
후보지만 net name이 아직 없다.

| pin | edge | 상태 |
| --- | --- | --- |
| PI3 | falling | 후보 |
| PI4 | falling | HW revision-dependent 후보 |
| PI5 | falling | HW revision-dependent 후보 |
| PG13 | rising + falling | 후보 |
| PD12 | rising + falling | 후보 |

`PG14 PI9 PD13 PI8 PI1 PE3 PA8 PC1 PC8 PB1 PA4`도 기능명이 미확정이다.
첫 custom image에서는 알려지지 않은 모든 pin을 analog 또는 high-impedance input으로
유지한다. 예외는 passive debug와 이미 안전 level이 확인된 PF6 HIGH뿐이다.

## 7. 보드 수령 당일 절차

### 단계 A: 전원을 넣기 전

1. 보드 앞뒤, connector, test pad, crystal, MCU/BT/EVE/NOR/SDRAM marking을 수직으로 촬영한다.
2. GND continuity와 주요 rail 저항을 기록한다.
3. MCU marking으로 정확한 STM32 part와 package를 정한다.
4. SWDIO, SWCLK, NRST, BOOT0, GND 후보를 continuity로 찾는다.
5. NOR가 별도 programmer로 읽힐 수 있는 package인지 확인한다.

### 단계 B: 전류 제한 첫 전원

1. 차량 전원 사양을 모르면 임의 voltage를 인가하지 않는다.
2. bench supply current limit를 낮게 시작하고 idle/boot current를 기록한다.
3. 모든 rail과 logic voltage를 측정한다.
4. 순정 firmware가 부팅되면 clock, UART, SPI, I2C activity를 passive probe한다.

### 단계 C: SWD read-only 식별

1. 가장 낮은 SWD clock으로 attach-under-reset을 시도한다.
2. `DBGMCU_IDCODE`, flash-size register, UID, CPUID를 저장한다.
3. option bytes와 RDP를 **읽기만** 한다.
4. RDP가 걸렸다면 read-unprotect를 누르지 않는다. 일반적으로 erase를 동반할 수 있다.
5. flash 총용량을 확인한 뒤 전체 internal flash를 두 번 덤프한다.

OpenOCD/GDB 골격은 다음과 같다. `<FLASH_BYTES>`는 실물에서 읽은 값으로 바꾼다.

```text
monitor reset halt
x/1wx 0xE0042000          # DBGMCU_IDCODE
x/1hx 0x1FFF7A22          # flash size in KiB candidate; part RM/DS와 재확인
x/3wx 0x1FFF7A10          # UID
dump binary memory donor-internal-a.bin 0x08000000 0x08000000+<FLASH_BYTES>
dump binary memory donor-internal-b.bin 0x08000000 0x08000000+<FLASH_BYTES>
```

도구별 option-byte 명령은 쓰기 명령과 이름이 비슷하므로 여기서 고정하지 않는다.
STM32CubeProgrammer의 read-only 화면과 CLI log를 모두 저장한다.

### 단계 D: external NOR 백업

1. 우선 순정 firmware 또는 SWD RAM stub으로 `0x9F`만 읽는다.
2. `C2 20 1B`가 맞는지 확인한다.
3. board power와 external programmer 전원을 동시에 넣지 않는다.
4. 128 MiB 전체를 A/B 두 번 읽고 각 SHA-256을 계산한다.
5. dump를 4 KiB 단위 entropy/blank map으로 분석하되 원본에는 쓰지 않는다.

### 단계 E: 백업 승인 기준

다음이 모두 끝나기 전에는 flash erase/program을 하지 않는다.

- internal dump A/B SHA-256 일치
- `0x08000000..0x0800FFFF`가 dump에 포함됨
- 128 MiB NOR dump A/B SHA-256 일치
- option bytes/RDP/UID/MCU marking 기록
- original internal/NOR dump의 별도 복사본
- 원본 복구 절차와 전원 차단 절차 문서화

## 8. 첫 clean-room bring-up

첫 코드는 flash가 아니라 SRAM에서 debugger로 실행한다. GPIO 전체를 재초기화하지
말고, 한 peripheral씩만 clock을 켜서 다음 순서로 검사한다.

1. fault handler + SWO 또는 별도 안전 UART logging
2. clock register snapshot과 계산값 출력
3. SPI5 `0x9F` read-only
4. I2C3 OPT3001 ID read
5. I2C1 address/status read-only
6. SPI1 EVE `REG_ID` read-only
7. UART5 RX passive parser
8. USART1 HCI passive capture와 reset/baud sequence 복원
9. SPI4 control pin polarity를 닫은 뒤 LCD status read
10. TIM5 pin/channel을 닫은 뒤 low-duty backlight

SRAM image가 peripheral을 하나씩 재현한 뒤에만 `0x08010000`용 flash image를 만든다.
하위 boot/persistent flash는 보존한다. 기존 bootloader가 custom image의 trailer 또는
checksum을 거부할 수 있으므로 첫 flash test는 debugger가 직접 PC/MSP/VTOR를 설정해
application으로 진입시키는 방식으로 분리한다.

## 9. Cube/HAL 프로젝트에 넣을 값

정확한 MCU suffix가 확인된 뒤 새 프로젝트를 만든다. 현재 넣어도 되는 seed는 다음뿐이다.

```text
VTOR offset       0x00010000
RTOS tick         1000 Hz
UART5             115200 8N1, no flow, PC12/PD2 AF8, DMA1 S0/S7 Ch4
USART1            3686400 8N1 RTS/CTS, PA9/10/11/12 AF7, DMA2 S5/S7
I2C1              100 kHz, PB6/PB7 AF4 OD
I2C3              400 kHz, PH7/PC9 AF4 OD
SPI4              16-bit mode 0 /16, PE2/5/6 AF5, polling
SPI5              8-bit mode 0 /2, PF7/8/9 AF5, PF6 GPIO HIGH, DMA 후보
```

다음은 실물 register snapshot 전까지 CubeMX에 추정 입력하지 않는다.

```text
HSE/PLL/AHB/APB clock tree
SPI1 divider/data size/NSS
TIM5 channel/pin/polarity
USB PHY/pins/VBUS
FMC SDRAM geometry/timing
EXTI net names and priorities
unknown GPIO output levels
```

## 10. 증거와 재현

- 설정 원본: `hardware/ak550-sr15-v516-mcu-peripheral-settings.json`
- board contract: `hardware/sr15-v516-board-contract.json`
- pin checklist: `hardware/sr15-v516-pinmap.csv`
- generated address map: `analysis/2026-09-01-custom-firmware-hardware-map/hardware-map.json`
- corrected vector map: `analysis/2026-09-01-custom-firmware-hardware-map/vector-map.json`
- disassembly cross-check: `analysis/2026-09-01-sr15-hardware-crosscheck/README.md`

재생성:

```powershell
$python = '<local-user>\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
& $python .\analysis\2026-09-01-custom-firmware-hardware-map\build_vector_map.py `
  .\artifacts\ota-archive\2026-08-31-full\blobs\firmware\1657088080998-s1-SR1.5_ota_V516.bin `
  --json .\analysis\2026-09-01-custom-firmware-hardware-map\vector-map.json `
  --markdown .\analysis\2026-09-01-custom-firmware-hardware-map\IRQ-MAP.md
& $python .\tools\validate_hardware_contract.py
```

이 문서는 “보드를 보지 않고 확정한 schematic”이 아니다. firmware에서 닫힌 설정,
실물에서 읽어야 닫히는 설정, 첫 custom image에서 절대 drive하면 안 되는 설정을 분리한
작업용 계약서다.
