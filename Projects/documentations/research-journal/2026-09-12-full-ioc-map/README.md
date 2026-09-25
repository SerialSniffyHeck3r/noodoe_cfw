# 순정 Noodoe 주변장치 → CubeMX IOC 매핑

2026-09-12. 대상은 실제 백업의 STM32F429IE / LQFP176, SR1.5 V5.16 APP이다. OpenNoodoe 구현은 사용하지 않았다. 순정의 실행 경로·HAL 인자·GPIO 초기화와 설치된 CubeMX 6.14.1 데이터베이스를 대조하여 프로젝트 IOC를 구성한다. **코드 생성과 새 설정의 실물 실행은 아직 하지 않았다. 사용자가 Generate Code를 수행한 뒤 생성 C와 BSP 연결을 검증한다.**

**최종 반영 완료:** 실제 프로젝트 IOC SHA-256은 `4c304d36e3e507a73a013f7e20498b67552ccd42d59eb6a7ab7e817b15d27947`이다. 133개 물리 GPIO/oscillator pin, 9개 virtual pin, DMA request7개를 포함한다. 동일 파일을 설치된 CubeMX로 load/save-only 검증하여 **Clock Configuration Error=false, Pinout & Configuration Error=false**를 확인했다. [최종 manifest](result.json), [Cube 검증 로그](schema-validation/validation.log), [매핑 검사](mapping-validation.json)에 결과가 있다. 일부 hidden parameter가 save에서 생략되는 것은 조회로 확인했으며, standalone save 결과의 EWARM 프로젝트 필드는 실제 IOC에 반영하지 않았다.

## 사용자가 다음에 할 일

1. CubeIDE에서 열려 있던 IOC를 닫고, 프로젝트의 `FuckNudo_Noodoe_CFW_Project.ioc`를 다시 연다. 이전 편집기의 오래된 내용을 파일 위에 저장하지 않는다. 외부 변경 재읽기 질문이 나오면 디스크의 변경본을 읽는다.
2. Clock Configuration에서 SYSCLK/HCLK **168 MHz**, APB1 **42 MHz**, APB2 **84 MHz**를 확인한다. Project Manager의 Toolchain은 **STM32CubeIDE**다.
3. **Generate Code**를 수행한다. 그 뒤 재생성 완료를 알려 주면 생성 파일·Debug/Release 빌드·BSP 순서를 이어서 검증한다.

새 주변장치 초기화 함수는 모두 생성하되 **Do Not Generate Function Call**을 선택하고 공개 함수로 두었다. main이 전원 GPIO, watchdog, RTC, SDRAM을 한꺼번에 초기화하지 않고 이후 BSP에서 순서를 정한다. `SystemClock_Config`, HAL TIM6 timebase, FreeRTOS 연결은 유지한다. 이 IOC를 재생성하는 것만으로 화면 드라이버가 완성되는 것은 아니다.

## 근거와 범위

- 원본 전체 백업 SHA-256: `38207bed741a8fef9e6d2ed24b86c8cb8b376ba5e4c957163f5edd83fbeb8037`.
- OTA APP SHA-256: `3b64673054ca84b9cf504f4cc7ebcb2e6615b9fbf59df79a37a92dcf14f037ca`, 로드 기준 `0x08010000`.
- [display-audit.md](display-audit.md): EVE/LCD/NOR/PWM, clock 계산, 호출 경로와 동적 설정.
- [communications-audit.md](communications-audit.md): UART/BT/I2C/USB/FMC/RTC/TIM2/DMA/공통 GPIO 및 BT 초기화 추가 추적.
- 두 감사의 JSON에는 주소·구조체 값과 모든 GPIO 동작 순서를 보존했다. 이 문서의 요약보다 상세 주소를 필요로 하면 JSON과 대응 disassembly를 사용한다.
- [revision-strap-live.log](revision-strap-live.log): 이번 작업의 유일한 장치 접근. SWD로 GPIO 레지스터를 읽었으며 쓰기·다운로드·erase는 수행하지 않았다.

실물 strap PA3/PH3/PH2/PB10의 읽은 값은 `[0,1,1,0]`이고, 순정 `0x08036C68`의 비트 조립대로 **board revision 6**이다. 따라서 공통 GPIO/LCD의 `revision >= 3` 분기를 반영한다. 이것은 칩 silicon Revision 3 및 flash `0x0800C080`의 DDIC protocol selector 3과 서로 다른 값이다. 현재 APP가 보존한 strap 입력 상태를 읽었으며 PCB 실크 revision을 직접 판독했다는 뜻은 아니다.

## 주변장치 설정

| 주변장치 / 용도 | IOC의 초기 설정 | 핀 / 중요한 제한 |
|---|---|---|
| RCC / PWR / FLASH | HSE 25 MHz, PLL M25/N336/P2/Q7, AHB÷1, APB1÷4, APB2÷2, Scale1, flash 5WS에 해당하는 168 MHz 구성 | PH0/PH1 HSE, PC14/PC15 LSE. HSE25M은 순정 상수 `0x08045E30` 및 기존 live PLL의 결합 근거이며 새 주파수 실측은 아님 |
| SYS / SWD | Serial Wire, HAL tick TIM6 유지 | PA13 SWDIO, PA14 SWCLK. PB3는 SPI1 SCK |
| SPI1 / EVE 표시 엔진 | Master, mode0, 8bit, SW NSS, MSB, ÷8 = 10.5 MHz | PB3 SCK, PA6 MISO, PB5 MOSI. 순정은 EVE 초기화 후 ÷4 = 21 MHz로 변경 |
| SPI4 / LCD DDIC | Master, mode0, 16bit, SW NSS, MSB, ÷16 = 5.25 MHz | PE2 SCK, PE5 MISO, PE6 MOSI. PE4 GPIO 프레이밍, DMA 사용 근거 없음 |
| TIM5 CH4 / 백라이트 | PSC1679, ARR99, PWM1, active HIGH, 초기 pulse0. APB1 timer84M 기준 500 Hz | PI0 AF2. 채널 start와 PI8/PC8 전원 순서는 BSP 담당 |
| SPI5 / 외장 NOR | Master, mode0, 8bit, SW NSS, MSB, ÷2 = 42 MHz | PF7/8/9, PF6 CS. 명령은 8bit polling, bulk는 16bit DMA로 전환 |
| FMC / SDRAM | Bank1, column10, row13, 16bit, 4 internal banks, CAS3, clock HCLK÷2, read burst ON, pipe0 | 39개 AF12 핀. JEDEC command/refresh는 BSP 담당. 설정상 geometry64MiB이며 칩 실제 용량을 판독한 것은 아님 |
| UART5 / 계기판 | 115200, 8N1, TX/RX, no flow control, oversampling16 | PC12 TX, PD2 RX. 순정 F5 수신은 1byte RX interrupt; 설정된 RX DMA를 사용하는 ring이라고 해석하지 않음 |
| USART1 / BT HCI | **115200 bootstrap**, 8N1, RTS/CTS, oversampling16 | PA9 TX, PA10 RX, PA11 CTS, PA12 RTS. HCI 명령 이후 일반 3686400 / 특수 RTC 조건 230400으로 전환 |
| I2C1 / 인증 칩 | 100 kHz, duty2, 7bit, own address0 | PB6 SCL, PB7 SDA, AF4 OD/no pull. PH13 초기 HIGH. 상대 slave address와 MCU own address를 구분 |
| I2C3 / 조도 센서 | 400 kHz, duty2, 7bit, own address0 | PH7 SCL, PC9 SDA, AF4 OD/no pull. OPT3001 slave address0x45 |
| USB OTG HS | **embedded FS PHY / Device Only**, 6 endpoints, VBUS sensing/DMA/SOF/low power/dedicated EP1 OFF | PB14 DM/PB15 DP. ULPI 및 PA11/PA12 USB FS가 아님. USB_DEVICE class middleware는 이후 선택 |
| RTC | LSE32768, async127/sync255, 24h, 내부 Alarm A / IRQ15 | 현재 날짜·알람 값은 실행 상태. 생성 기본 날짜/알람을 순정 데이터로 오해하거나 기존 RTC에 그대로 덮어쓰지 않음 |
| TIM2 | Up, PSC83/ARR999, 내부 clock, IRQ15 | 순정의 HAL timebase. 현 CFW HAL은 이미 검증한 TIM6 유지, TIM2 초기화 호출은 보류 |
| CRC | F4 hardware CRC 기본 설정 | `0x0804256A`가 instance40023000을 초기화. 다항식04C11DB7, 초기FFFFFFFF, word feed |
| IWDG | DIV256, reload3840 | `0x080351AA` / HAL `0x080424C6`. 초기화 자체가 watchdog을 시작하므로 자동 호출하지 않음 |
| DMA / NVIC / GPIO / EXTI | 아래 stream·우선순위·GPIO 설정 | unknown GPIO를 임의로 전원핀/미사용핀으로 단정하지 않음 |
| FreeRTOS | 기존 CMSIS-RTOS v2, heap4/64KiB, stack/malloc hook 유지 | 이 middleware 선택은 순정 RTOS를 그대로 재현했다는 뜻이 아님 |

SPI1/4 AF GPIO는 medium speed, SPI5는 high speed. I2C/FMC는 high speed, USB/PI0는 very high speed다. GPIO speed는 SCK 주파수가 아니라 출력 드라이버 slew 설정이다. Pull과 speed를 핀별로 IOC에 넣었다.

ADC, DAC, CAN, SDIO, LTDC, DMA2D, RNG 등의 실제 활성 초기화 경로는 이번 선택된 순정 경로에서 확인되지 않아 임의로 켜지 않았다. 화면은 SPI 연결 EVE와 별도 LCD 제어의 조합이다. MCU가 지원하는 주변장치와 보드가 실제 사용하는 주변장치를 구분한다. FLASH/PWR/SYSCFG 및 Cortex/NVIC는 RCC/HAL/부트 인계에 포함되며 별도 외부 버스 핀을 만들지 않는다.

## DMA와 IRQ

| 용도 | Stream / channel | 방향·정렬·increment | DMA priority / NVIC priority |
|---|---|---|---|
| UART5 RX 설정 | DMA1 S0 / Ch4 | P→M, byte, PINC0/MINC1 | HIGH / 7 |
| UART5 TX | DMA1 S7 / Ch4 | M→P, byte, PINC0/MINC1 | LOW / 7 |
| USART1 RX | DMA2 S5 / Ch4 | P→M, byte, PINC0/MINC1 | VERY_HIGH / 5 |
| USART1 TX | DMA2 S7 / Ch4 | M→P, byte, PINC0/MINC1 | VERY_HIGH / 5 |
| SDRAM copy | DMA2 S0 / Ch0 | M→M, byte, PINC1/MINC1 | HIGH / 4 |
| SPI5 RX | DMA2 S3 / Ch2 | P→M, halfword, 초기 PINC0/MINC1 | HIGH / 4 |
| SPI5 TX | DMA2 S4 / Ch2 | M→P, halfword, 초기 PINC0/MINC1 | LOW / 4 |

모두 Normal, effective single burst. UART/SPI FIFO OFF. SDRAM M2M은 **FIFO ON / Quarter**로 표현했다. 순정 init 구조체의 FIFO0/thresholdFull을 문자 그대로 옮기지 않은 이유는 HAL이 FIFO0일 때 threshold를 적용하지 않고 FTH00을 남기며, STM32 하드웨어가 M2M EN 시 DMDIS를 자동으로 켜기 때문이다. 따라서 실행 중 실제 설정은 ON/Quarter다. [ST RM0090, DMA_SxFCR DMDIS](https://www.st.com/resource/en/reference_manual/dm00031020-stm32f405415-stm32f407417-stm32f427437-and-stm32f429439-advanced-armbased-32bit-mcus-stmicroelectronics.pdf).

SPI5 bulk read는 RX MINC1/TX MINC0(dummy), bulk write는 RX MINC0(dummy)/TX MINC1이다. 폭과 increment를 전송 전에 함께 맞추는 BSP가 필요하다. IOC의 8bit SPI + halfword DMA는 순정의 서로 다른 명령/bulk phase를 표현하며, 8bit인 채 DMA를 시작하라는 의미가 아니다.

추가 IRQ: UART5=7, TIM5=8, TIM2=15, RTC Alarm=15, OTG_HS=6, EXTI15_10=5, EXTI9_5=15, EXTI3/4=8. USART1 direct IRQ는 flow-control 없는 대체 경로에만 나타나므로 현재 RTS/CTS 경로에서는 켜지 않았다. I2C EV/ER IRQ도 polling 경로에 억지로 추가하지 않았다.

NVIC priority group4. IOC의 명시 priority/user-enable 플래그도 설정하여 Cube가 RTOS 기본 priority5로 바꾸지 않도록 했다. **priority4 ISR은 FreeRTOS API를 호출할 수 없다**(현 max syscall priority5). 이후 DMA 완료 처리는 RTOS 호출 가능한 낮은 우선순위 단계로 전달해야 한다. 모든 함수가 deferred이므로 아직 이 interrupt들이 실행된 상태는 아니다.

## GPIO 초기값과 순서

전체 핀 표는 [pin-map.md](pin-map.md), 원본 순서별 기록은 communications-audit.json의 `gpio_init_sequences`다. 표는 누적 공통 GPIO 초기화의 **최종 설정**을 반영한다. 생성 `MX_GPIO_Init`은 GPIO를 묶어서 설정하므로 순정의 시간 순서 자체까지 같다는 뜻은 아니다.

- EVE: PA4 HIGH / pullup / low speed, PB1 HIGH / no pull / low speed. PB1 active-low pulse 후 EVE host command/REG_ID 확인이 필요하다.
- LCD: PE4 LOW / pullup / low speed, PI11 HIGH / no pull / low speed, 이번 board>=3의 PC13 LOW / no pull / low speed. DDIC bring-up에서 PI11 LOW, PC13 HIGH→LOW→HIGH 및 10/20/50ms 기다림이 필요하다. PE4는 단순 CS low-held 전송과 다른 프레이밍이다.
- 백라이트: PI8 LOW, PC8 HIGH가 공통 GPIO 초기 상태. 순정 PWM init에서 PI8 HIGH→PC8 LOW→3ms. 화면 확인 이전에는 pulse0 유지한다.
- NOR: PF6 HIGH / pullup / high speed. 뒤늦은 생성 GPIO 호출로 동작 중 CS를 건드리지 않도록 BSP가 소유한다.
- 전원 관련 관측: PD13 HIGH, PG14 LOW, board>=3 PI9 HIGH. 순정과 같은 초기값을 보존하되 전원 유지의 정확한 회로 역할과 active polarity는 동작 근거 범위 안에서만 해석한다.
- 입력/EXTI의 최종 mux, pull, edge를 반영했다. 순정이 analog로 둔 bonded GPIO 27개도 명시했다. PI12/14/15는 원본 mask에 있지만 LQFP176 비본딩이므로 IOC 핀으로 만들지 않았다.

PA8/PI1은 BT open 경로의 GPIO 제어에 참여한다. PA8 LOW→UART/RX DMA 초기화→10ms→PA8 HIGH→150ms 순서와 PI1 HIGH 제어가 있다. 단순히 전압이 HIGH였다는 이유로 고정 전원 rail로 모델링하지 않는다.

## IOC 밖에서 구현해야 하는 부분

1. **BL 인계 후 초기화 순서.** 기존 early BSP의 HSI16MHz 정리, MPU/VTOR/IRQ/DMA 정리, 사용자 startup/APP linker를 유지한다. 생성 clock 설정은 그 뒤 168MHz로 올린다. early 코드의 16MHz 검사를 168MHz로 바꾸면 안 된다.
2. **SDRAM.** MX_FMC_Init 뒤 clock-enable→1ms→PALL→8 auto-refresh→mode0x230→refresh636@84MHz. 실제 메모리 검사 전에는 SDRAM을 heap/framebuffer로 사용하지 않는다.
3. **EVE/LCD/backlight.** PDN/reset/CS 전환, EVE ID0x7C 확인, SPI1 가속, DDIC 0x1100/120ms/0x2900 및 status read, 마지막 PWM enable. IOC는 이 장치 프로토콜을 생성하지 않는다.
4. **BT.** IOC의 bootstrap115200으로 HCI Reset → vendor baud command0xFF36(payload LE32 target baud) → host baud 변경 → service pack. RTC BKP19=A5A50003이면 target230400, 그 외3686400. 자세한 함수 경로와 패치 위치는 통신 감사 참조.
5. **USB.** PCD FIFO RX512/TX0 128/TX1 372 words와 class/descriptor/callback은 USB stack 단계에서 설정한다. 현재 IOC는 hardware PCD만 생성한다.
6. **RTC/IWDG.** 기존 calendar/backup domain을 보존하고 필요한 알람만 설정한다. 생성 RTC 기본 날짜/알람 코드를 무조건 호출하지 않는다. watchdog은 태스크와 실패 처리 경로가 준비된 뒤 시작한다.
7. **IRQ/RTOS.** HAL handler와 DMA handle 연결, DMA-visible RAM 위치, callback 소유권을 실제 생성본에 맞춰 검증한다. IOC NVIC checkbox만으로 상위 로직이 완성되지 않는다.

## 검증 파일과 작업 상태

- `before.ioc`: 변경 전 실제 프로젝트 IOC 원본.
- `build_ioc.py`, `build_extra.py`, `build_dma.py`: 출처를 분리한 재현 가능한 초안 생성기. 이 스크립트들은 코드 생성이나 장치 명령을 실행하지 않는다.
- `mapped-draft.ioc`: 편집 산출물. 실제 프로젝트와 일치 여부는 최종 validation manifest에 기록한다.
- `validate_mapping.py` / `mapping-validation.json`: MCU 패키지의 AF 지원, 핀 중복, 감사에서 복원한 GPIO 최종 상태, 주요 클록 설정을 검사한다.
- `schema-notes.md` 및 Cube load/save 로그: 설치된 CubeMX를 **명령행 load/save only**로 검증한 근거. standalone Cube save는 프로젝트 종류를 EWARM으로 바꿀 수 있어, 그 결과를 실제 IOC에 통째로 덮어쓰지 않는다.
- 프로젝트 `tools/check_generated.py`: 재생성 전 오래된 16MHz C를 새 IOC의 결과로 잘못 빌드하지 않도록 검사한다. 현재 이 검사에서 새 peripheral 파일 없음 / old clock / 자동 GPIO 호출 등이 나오는 것은 재생성 대기 상태를 탐지한 것이다.

현재 장치에 마지막으로 기록한 이미지는 이전 bring-up의 16MHz Debug 이미지다. 이번에는 장치 실행 상태를 새로 시험하지 않았다. 이 작업에서 새 IOC 코드 생성·컴파일·다운로드·화면 점등 시험을 했다고 주장하지 않는다. 사용자가 재생성하면 생성 함수와 MSP/DMA/IRQ 설정을 먼저 검토하고, 필요한 BSP를 붙인 후 새 168MHz 실행과 화면을 시험한다.
