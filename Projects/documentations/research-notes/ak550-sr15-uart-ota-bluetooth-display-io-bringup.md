# AK550 Noodoe SR1.5 차량 UART, OTA, Bluetooth, 화면 및 I/O 브링업 연구서

> **2026-09-11 실물 부트 분석:** 이번 A/B 덤프에서 실제 부트 코드, 외부 NOR staging, 설치 메타데이터와 IGN OFF 설치 인계를 확인했다. [실물 부트·BT 업데이트 연구노트](2026-09-11-noodoe-bootloader-and-bluetooth-update.md)를 우선한다. 아래 7.2의 `0x0C=state/query`는 오류이며 실제로는 RIDING_STATUS다. 7.3의 chunk reply 검사 상태 머신은 커스텀 구현 제안과 순정 동작을 구분해야 한다. 순정 APK의 chunk 진행 조건은 outer ACK 성공이며, task DONE 즉시 reboot를 가정하면 안 된다. 아래 7.4의 미확정 부트·staging·CRC 설명은 후속 분석으로 갱신되었다.

> **2026-09-09 재검증:** SR1 CRC 재현, 하위 flash의 16KiB shadow/4KiB program 구분, UART5 byte-IRQ 수신 및 USART1 baud 재설정에 대한 정정은 [최신 BIN·APK 대조 보고서](2026-09-09-ak550-firmware-uart-boot-review.md)를 우선한다. 아래는 당시 분석 기록이며, 충돌하는 설명을 실물 작업의 확정 근거로 사용하지 않는다.

작성일: 2026-09-05 KST

이 문서는 AK550용 Noodoe SR1/SR1.5 보드를 완전 신규 펌웨어로 기동하기 위한
실행 기준서다. CV3의 SR2/STM32H7 자료는 적용하지 않는다. 기계 판독용 단계표는
`hardware/ak550-sr15-subsystem-bringup.json`에 있다.

## 1. 적용 대상과 증거 경계

| 항목 | 값 |
| --- | --- |
| 기준 이미지 | `SR1.5_ota_V516.bin` |
| SHA-256 | `3b64673054ca84b9cf504f4cc7ebcb2e6615b9fbf59df79a37a92dcf14f037ca` |
| 이미지 범위 | `0x08010000..0x0807FFFB` |
| 초기 MSP | `0x20025318` |
| reset handler | `0x08076718` |
| MCU | STM32F429xx-class Cortex-M4F, 정확한 suffix/package 미확정 |
| 실차 | AK550, firmware 5.16, resource 5.14, boot 0.15, PCBA SR0701 |

여기서 `확정`은 바이너리, 순정 APK, 실차 로그 또는 부품 ID가 직접 뒷받침하는
사실이다. `강함`은 복수 증거가 일치하지만 PCB marking이나 logic trace가 남은
항목이고, `열림`은 실물에서 닫아야 하는 후보이다. 이미지 밖의 하위 flash와
물리 배선은 펌웨어 파일만으로 확정하지 않는다.

## 2. 시스템을 한 장으로 보면

```text
AK550 meter/vehicle board
        | UART5, 115200 8N1, F5/XOR
        v
STM32F429 Noodoe application
  | USART1 HCI        | SPI1 host          | SPI5
  v                   v                    v
TI CC256x          FT81x EVE          128 MiB NOR
  | Bluetooth         | RGB 480x480         themes/photos/OTA candidate
  | Classic RFCOMM    v
  +-------------- Android phone       LCD panel
                         ^              ^
                         |              | SPI4 DCS control
                  SPP 5A FF/A5 5A       | TIM5 backlight

I2C3 -> OPT3001 ambient light
I2C1 -> MFi authentication coprocessor
EXTI/GPIO -> power/up/enter/down and board-state inputs
```

가장 중요한 구분은 다음과 같다.

1. 차량 UART와 휴대전화 SPP는 서로 다른 프로토콜이다.
2. TI 칩은 Bluetooth controller이고, RFCOMM/SPP host stack은 STM32의 Bluetopia다.
3. Bluetooth OTA는 실행 중인 STM32 앱 또는 resident loader가 받아야 한다.
   STM32 ROM bootloader가 무선 SPP를 직접 처리하지 않는다.
4. FT81x는 그림을 만드는 graphics engine이고, 별도 SPI4는 LCD DDIC의
   sleep/display 상태를 제어한다. 둘 중 하나만 초기화해서는 정상 화면을 보장할 수
   없다.

## 3. 전체 브링업 순서

순서는 의존성 순서이자 위험도 순서다.

| 단계 | 동작 | 쓰기 위험 | 합격 기준 |
| ---: | --- | --- | --- |
| 0 | PCB 촬영, 부품 marking, 전원 net, connector 방향 기록 | 없음 | 보드 revision과 IC 목록을 사진으로 고정 |
| 1 | SWD read-only 접속, IDCODE/flash size/option bytes/RDP 읽기 | 없음 | 읽은 원시값과 도구 로그 보존 |
| 2 | internal flash를 두 번 읽고 hash 비교 | 없음 | 전체 dump 두 개가 byte-identical |
| 3 | SPI5 NOR를 두 번 읽고 hash 비교 | 없음 | 128 MiB dump 두 개가 byte-identical |
| 4 | 순정 실행 중 RCC/GPIO/AF/DMA/EXTI/TIM register snapshot | 없음 | 최종 clock과 동적 pin 설정 확보 |
| 5 | SRAM 실행 최소 펌웨어: clock, watchdog, 진단 UART만 | RAM만 | reset 반복 없이 heartbeat 출력 |
| 6 | SPI5 `RDID/RDSR`와 I2C3 ID read | 읽기 전용 | `C2 20 1B`, `5449/3001` |
| 7 | EXTI/input 관측, UART5 RX passive parser | 읽기 전용 | 입력 edge와 F5 frame을 손실 없이 기록 |
| 8 | FT81x ID, panel status, 검은 display list | 제어만 | `REG_ID=0x7C`, panel 응답, 안정된 검은 화면 |
| 9 | 낮은 duty부터 backlight와 RGB test pattern | 제어 | 480x480 pattern, current/temperature 정상 |
| 10 | CC256x HCI reset, version, service pack, SPP | controller RAM | Android RFCOMM 연결과 echo/DeviceInfo |
| 11 | stock boot/update 경계 분석과 dry-run OTA | 없음 | 모든 ACK/resume/timeout을 기록 |
| 12 | 복구가 증명된 뒤에만 custom flash/OTA | flash write | 재부팅 후 version 확인과 강제 복구 성공 |

Stage 0부터 4까지 끝나기 전에는 새 firmware를 internal flash에 쓰지 않는다.
특히 RDP를 해제하려는 동작은 mass erase를 일으킬 수 있으므로 읽기가 막히면 그대로
중단한다.

## 4. 차량 UART5와 F5 프로토콜

### 4.1 전기 및 STM32 설정

| 항목 | 값 | 상태 |
| --- | --- | --- |
| peripheral | UART5 | 확정 |
| TX/RX | PC12 AF8 / PD2 AF8 | 강함 |
| format | 115200, 8N1, no flow, oversampling 16 | 확정 |
| RX DMA | DMA1 Stream0 Channel4 | 확정 |
| TX DMA | DMA1 Stream7 Channel4 | 확정 |
| IRQ | UART5, DMA1 Stream0/7 | 비기본 handler 확인 |
| handle | `0x2002289C` | V5.16 주소 고정 |

첫 custom image에서는 PC12 TX를 alternate-function output으로 만들지 않는다. PD2를
high-impedance input으로 두고 RX만 수집한다. 순정 보드에서 idle level, 실제 전압,
connector 방향, 상대 장치가 먼저 송신하는지 확인한 뒤 `VEHICLE_TX_ARMED` 같은 별도
compile-time gate로 TX를 연다.

### 4.2 frame과 parser

```text
F5 | CMD | LEN | PAYLOAD[LEN] | XOR
```

`XOR`는 `F5`부터 마지막 payload byte까지 모두 XOR한 값이다. parser는 임의 위치에서
시작할 수 있어야 하며 다음 조건을 지켜야 한다.

1. `0xF5`를 찾을 때까지 버린다.
2. length에 상한을 두되 `250`과 `71`은 허용한다.
3. 완전한 frame이 올 때까지 DMA ring에서 기다린다.
4. checksum 실패 시 현재 frame을 폐기하고 다음 `0xF5`부터 재동기화한다.
5. raw timestamp, direction, frame, checksum result를 먼저 저장한 뒤 의미를 decode한다.

알려진 테스트 vector는 `F5 01 01 02 F7`이다.

### 4.3 command와 telemetry

| 방향 | CMD | payload | 현재 해석 |
| --- | ---: | ---: | --- |
| vehicle -> Noodoe | `0x21` | 최소 9 | 기본 live telemetry |
| vehicle -> Noodoe | `0x22` | 최소 11 | 확장 live telemetry |
| vehicle -> Noodoe | `0x41` | 정확히 250 | meter/profile blob |
| vehicle -> Noodoe | `0x42` | 정확히 71 | battery/status/event blob |
| Noodoe -> vehicle | `0x01` | 1 | `01/02/04/08` bitmask, 의미 미확정 |
| Noodoe -> vehicle | `0xA1` | 2 | `01 xx` 형태의 제어/status, 의미 미확정 |

`0x21/0x22`의 현재 필드 지도는 다음과 같다.

| offset | decode | 판정 |
| --- | --- | --- |
| `[0]` | 속도 | 매우 강함: 차속 5 전후 riding 전이, max speed와 UI consumer |
| `[1]` | 미상 | 저장 경로만 확인 |
| `[2]` | 0..100 fuel-like 값 | 매우 강함: 20/40/60/80 구간화 |
| `[3]` | low/high nibble 상태 두 개 | 의미 열림 |
| `[4..7]` | little-endian u32 odometer 후보 | 매우 강함: decimal digit 분해 |
| `[8]` | `raw - 40` temperature | 강함: ambient/coolant 출처 열림 |
| `[9..10]` | little-endian u16, `0x22` 전용 | 의미 열림 |

직접 bridge도 확인된다. 속도 계열 상태는 phone `C2 NOTIFY_RIDING`을 만들고,
`0x41` 250 bytes는 RAM `0x20021A40`을 거쳐 SPP `0x16 GET_METER_PROFILE`의
250-byte body가 된다. `0x42` 71 bytes는 `0x200227D4`를 거쳐 `C6
UPDATE_BATTERY_DATA`가 된다.

### 4.4 필드를 확정하는 실험

logic analyzer는 vehicle와 Noodoe 사이에서 passive로 연결하고, 한 캡처에서 변수
하나만 바꾼다. 모든 캡처에는 ignition/engine/wheel 상태, stock display 사진,
휴대전화 SPP log와 공통 monotonic timestamp를 남긴다.

| 실험 | 변경 변수 | 확정하려는 byte |
| --- | --- | --- |
| VU-01 | key OFF/ON | status nibble, `0x41/0x42` diff |
| VU-02 | 좌/우 방향지시등 각각 | bit/nibble mapping |
| VU-03 | high beam, brake, side stand 각각 | bit/nibble mapping |
| VU-04 | 바퀴 0, 저속, 5 초과 | `[0]`, riding threshold |
| VU-05 | 알려진 거리 주행 전후 | `[4..7]` scale/unit/endian |
| VU-06 | 냉간, warm-up, 외기 비교 | `[8]` sensor identity |
| VU-07 | fuel 변화 장기 기록 | `[2]` scale/filtering |
| VU-08 | 배터리 전압만 변화 | `0x42` field 후보 |

`0x01`과 `0xA1`은 실차에 임의 송신하지 않는다. 먼저 순정 firmware가 실제로 보내는
frame을 양방향 tap으로 수집하고, 수신 측 응답과 물리 효과를 확인한 뒤 bench의
분리된 보드에서 replay한다.

### 4.5 UART 합격 조건

- 30분 capture에서 checksum-valid frame loss가 측정 가능하고 overflow가 없다.
- checksum 오류 뒤 다음 valid frame으로 자동 복구한다.
- unknown command와 원문을 버리지 않는다.
- TX는 별도 armed build가 아니면 전기적으로 high-Z다.
- 실차 의미표에는 측정 전 `확정`을 붙이지 않는다.

## 5. Bluetooth controller와 SPP 브링업

### 5.1 하드웨어 분할

순정 firmware는 `USART1`을 `PA9 TX`, `PA10 RX`, `PA11 CTS`, `PA12 RTS`,
8N1/RTS/CTS로 사용한다. runtime baud는 `3,686,400`이다. direct USART1 IRQ는
IAR default self-loop이고 실제 전송 경로는 DMA2 Stream5 RX와 Stream7 TX다.

firmware에는 Bluetopia `4.0.2.1`, TI vendor-specific HCI command, HCILL, 31개의
write-memory block을 포함한 service-pack 흔적이 있다. 이는 다음 구조를 뜻한다.

```text
Android RFCOMM client
  <-> Bluetooth air link
TI CC256x controller
  <-> HCI UART + RTS/CTS
STM32 Bluetopia host
  <-> Noodoe command dispatcher
```

정확한 CC256x/CC2564 silicon revision, module marking, shutdown/reset GPIO는 아직
열려 있다. embedded service pack을 marking 확인 없이 재생해서는 안 된다.

### 5.2 controller 초기화 순서

TI 자료와 firmware 흔적을 합치면 최소 초기화기는 다음 상태기계가 되어야 한다.

1. controller 전원 rail과 shutdown/reset net을 식별하고 순정 부팅 waveform을
   기록한다.
2. MCU USART1을 우선 `115200 8N1 RTS/CTS`로 열고 HCI Reset을 보낸다.
3. Command Complete를 기다리고 Read Local Version Information을 저장한다.
4. 반환된 chip/version에 맞는 TI service pack을 선택한다.
5. BTS script의 HCI command를 한 개씩 보내고 각 expected event를 확인한다.
6. baud 변경 vendor command의 Command Complete를 기존 baud에서 받은 다음에만
   MCU와 controller를 함께 `3,686,400`으로 전환한다.
7. service pack과 필요한 add-on을 끝낸 뒤 VS lock/end 절차를 수행한다.
8. 안정된 HCI event loop를 확인한 후에만 eHCILL 저전력을 켠다.
9. Bluetopia 또는 대체 host stack에서 GAP, L2CAP, RFCOMM, SDP, SPP server를 연다.
10. Serial Port UUID `00001101-0000-1000-8000-00805F9B34FB`를 광고하고 Android
    RFCOMM 연결을 받는다.

TI CC2564MODA 계열의 공식 기본값은 power-up UART `115200 8N1`이고 최대 4 Mbps,
RTS/CTS를 지원한다. service pack은 controller RAM patch라서 매 power cycle마다
다시 적재해야 한다. 다만 이것은 계열 공식값이며 실제 보드 marking과 HCI version이
같다는 확인 전에는 부품 확정값이 아니다.

### 5.3 구현에 필요한 진단 계층

HCI logger는 최소한 다음을 원시 binary와 decoded text로 동시에 남겨야 한다.

- power/reset edge와 baud 변경 timestamp
- HCI command opcode, parameters, Command Status/Complete
- vendor event와 service-pack block index
- RTS/CTS stall, DMA overrun, framing/noise error
- ACL handle, L2CAP CID, RFCOMM DLCI와 credit
- pairing method, link key 저장 여부, disconnect reason/RSSI

처음 목표는 Noodoe 전체 protocol이 아니라 Android에서 RFCOMM socket을 열고 32-byte
echo를 반복해도 누락이 없는 것이다. 그 다음 기존 `5A FF` outer frame과 `A5 5A`
command frame, sequence ACK, DeviceInfo `0x05`를 올린다.

### 5.4 MFi와 BLE를 분리해서 생각한다

I2C1의 `0x11` 장치는 MFi authentication coprocessor 경로로 강하게 보인다. Android
Classic SPP bring-up에는 이를 먼저 구현할 필요가 없다. iPhone/iAP 호환 단계에서만
순정 I2C transaction과 reset timing을 복원한다.

현재 AK550 firmware 5.16과 실차 Android 통신 경로는 Classic SPP다. APK 안의 BLE,
Nordic DFU, StOta library 존재는 이 보드의 active radio path 증거가 아니다.

### 5.5 Bluetooth 합격 조건

- cold boot 100회에서 HCI version read와 patch 완료가 재현된다.
- 잘못된 service pack revision이면 쓰기를 시작하지 않고 식별 오류로 멈춘다.
- baud switch 전후 byte trace가 연속적이고 CTS stall에서 복구한다.
- Android가 재페어링 없이 reconnect하고 single-SPP ownership을 명시한다.
- controller reset 뒤 link key/identity 저장 정책이 예측 가능하다.
- SPP 10 MiB loopback의 hash가 일치한 뒤에만 OTA를 시험한다.

## 6. 화면, 저장장치와 입력장치 브링업

### 6.1 안전한 GPIO 초기값

reset 직후 가장 먼저 외부 장치를 무작위로 구동하지 않게 해야 한다.

- 확인되지 않은 GPIO는 analog/high-Z.
- SPI5 NOR CS 후보 PF6은 output high.
- LCD/EVE CS, PDN, reset 후보는 순정 waveform과 polarity를 알 때까지 high-Z.
- backlight PWM pin은 정체를 알 때까지 high-Z.
- UART5 TX도 high-Z.
- watchdog과 brownout 원인을 serial log에 남긴다.

### 6.2 SPI5 resource NOR를 먼저 읽는다

| 항목 | 값 |
| --- | --- |
| bus | SPI5, PF7 SCK / PF8 MISO / PF9 MOSI, PF6 CS active-low |
| mode | 8-bit, mode 0, prescaler 2 후보 |
| ID | `C2 20 1B` |
| geometry | 1 Gbit = 128 MiB, 4 KiB sector, 256-byte page |
| first commands | `0x9F RDID`, `0x05 RDSR`, `0x03/0x13 READ` |

두 번의 전체 dump가 일치하기 전에는 `0x06 WREN`, page program, sector/block/chip
erase를 전부 금지한다. 16 MiB 경계를 넘는 read는 4-byte addressing mode와 `0x13`
동작을 실제 chip 기준으로 검증한다. 사진, 테마, 공장 데이터와 OTA staging이 모두
있을 수 있으므로 빈 영역처럼 보이는 `0xFF`도 보존 대상이다.

### 6.3 I2C3 ambient light

I2C3는 PH7 SCL, PC9 SDA, 400 kHz, 7-bit address `0x45`다. 우선 register
`0x7E`에서 `0x5449`, `0x7F`에서 `0x3001`을 읽어 OPT3001을 확인한다. 그 뒤에만
configuration `0x01`, result `0x00`, threshold `0x02/0x03`을 다룬다. 순정
I2C3 EV/ER vector는 default이므로 첫 driver는 polling과 timeout으로 작성한다.

### 6.4 입력과 EXTI

firmware에서 비기본 EXTI group은 `EXTI2`, `EXTI3`, `EXTI4`, `EXTI9_5`,
`EXTI15_10`이다. 후보 edge는 PI3 falling, revision-dependent PI4/PI5 falling,
PG13 both, PD12 both다. 정확한 power/up/enter/down 배정은 아직 닫히지 않았다.

OQC test가 휴대전화에 내보내는 5 bytes는 다음 후보 구조다.

```text
byte 0 bit 0: power on
       bit 1: power off
       bit 2: button up
       bit 3: enter
       bit 4: down
       bit 5: MFi enabled
byte 1..4: ambient-light value, little-endian
```

실차에서 OQC test를 켰을 때 1초 주기로 밝기가 바뀐 것은 확인됐다. 그러므로 OQC는
범용 UART loopback이 아니라 입력 bit와 light-sensor 값을 phone으로 보고하면서
backlight test pattern도 실행하는 생산검사 모드다. 새 펌웨어에서는 각 EXTI line의
raw level과 edge timestamp만 먼저 출력하고, 버튼 하나씩 눌러 mapping을 닫는다.

### 6.5 FT81x EVE와 480x480 scan-out

SPI1은 PB3 SCK, PA6 MISO, PB5 MOSI AF5다. 정확한 CS/PDN/INT GPIO와 FT810/811/
812/813 suffix는 열려 있다. 낮은 SPI clock에서 `REG_ID(0x302000)==0x7C`만 먼저
읽는다. command FIFO는 `REG_CMD_READ=0x3020F8`, `REG_CMD_WRITE=0x3020FC`,
ring mask `0x0FFC`다.

V5.16에서 복원된 timing 후보는 다음과 같다.

| register concept | value |
| --- | ---: |
| HSIZE / VSIZE | 480 / 480 |
| HCYCLE / HOFFSET | 550 / 37 |
| HSYNC0 / HSYNC1 | 0 / 4 |
| VCYCLE / VOFFSET | 505 / 18 |
| VSYNC0 / VSYNC1 | 0 / 2 |
| SWIZZLE | 3 |
| PCLK | 4 후보 |

안전한 화면 순서는 다음과 같다.

1. EVE PDN/reset waveform을 순정에서 측정한다.
2. EVE wake command 뒤 `REG_ID=0x7C`를 timeout으로 기다린다.
3. `REG_PCLK=0`으로 RGB pixel clock을 끈다.
4. timing, swizzle, output drive와 pixel format을 설정한다.
5. `CLEAR_COLOR_RGB(0,0,0)`, `CLEAR(1,1,1)`, `DISPLAY`, display-list swap을
   실행한다.
6. LCD DDIC가 display-on 상태인지 확인한다.
7. 마지막에만 PCLK 후보값을 쓰고 RGB scan-out을 시작한다.

Bridgetek 공식 자료에서도 `REG_PCLK=0`은 출력 clock 비활성화다. 그래서 잘못된
timing으로 패널을 먼저 구동하지 않는 이 순서가 중요하다.

### 6.6 SPI4 LCD DDIC와 backlight

SPI4는 PE2 SCK, PE5 MISO, PE6 MOSI AF5, PE4 CS/control 후보이며 16-bit mode 0,
prescaler 16, polling이다. 확인된 명령 family는 MIPI-DCS형이다.

```text
0x11  sleep out
delay about 120 ms
0x29  display on

0x28  display off
0x10  sleep in
0x0A  get power mode, expected 0x9C candidate
```

PE4가 CS인지 D/C인지, PI11/PC13이 reset/control인지 logic trace로 먼저 닫아야 한다.
패널 marking을 확보하면 해당 DDIC datasheet의 reset pulse, read dummy cycle, word
packing과 대조한다.

TIM5는 약 50 kHz counter, ARR 99, PWM 약 500 Hz/100 steps 후보지만 channel, pin,
polarity는 열려 있다. 순정 firmware에서 brightness 0/50/100을 바꾸며 모든 TIM5
channel register와 후보 pin을 동시에 측정한다. 새 firmware에서는 0%에서 시작하고
1%, 5%, 10% 순으로 올리며 rail current와 LED driver 온도를 감시한다.

### 6.7 화면과 I/O 합격 조건

- NOR와 sensor ID가 반복 read에서 안정적이다.
- 모든 button은 press/release당 정확히 한 logical event를 만든다.
- backlight off에서도 EVE와 panel current가 비정상적으로 증가하지 않는다.
- 480x480 red/green/blue/checker pattern의 방향, RGB order, tearing을 사진으로 남긴다.
- 2시간 static pattern에서 EVE FIFO fault, panel reset, thermal drift가 없다.
- ambient-light 변화가 raw lux log로 먼저 검증된 뒤에만 자동 밝기에 연결된다.

## 7. OTA와 bootloader 경계

### 7.1 현재 AK의 세대

실차의 firmware `5.16`, protocol `0.0`, hardware `0`은 순정 APK 분기에서
`VERSION_1_5`로 분류된다. `VERSION_1_5`와 `VERSION_2_0`은 서버 정책은 달라도
현대식 SPP file-transfer engine을 공유한다. `VERSION_1_0`은 별도 legacy
command state machine이지만 이것 역시 해당 APK에서는 Classic SPP 위에 있다.

### 7.2 modern SPP wire protocol

| 단계 | command | 핵심 값 |
| --- | ---: | --- |
| task BEGIN/CONTINUE/DONE | `0x0A` | type, location, total, attribute, content ID |
| file START/FINISH/CANCEL | `0x0B` | transfer ID, identity, CRC32, size, path |
| state/query | `0x0C` | 정확한 용도는 trace로 계속 검증 |
| file data | `0x0D` | task ID, transfer ID, data type, bytes |

Firmware 설치는 type `FILE=2`, location `0x0800`, file ID `1`이다. content ID는
major/minor를 little-endian u16 두 개로 넣고 16 bytes로 pad한다. CRC32는 원본을
4-byte 경계까지 zero-pad한 뒤 계산한다.

순정 app 2.1.14의 상수는 data buffer 최대 `11818`이지만 실제 코드는
`getMaxNumberDivisibleBy4(11818)`을 적용하므로 full data chunk는 `11816` bytes다.
data command 자체에는 6-byte task/transfer/type header가 앞선다. 이 둘을 혼동하면
offset과 CRC가 어긋난다.

순정 2.1.14에는 resume 의도와 구현을 따로 보아야 하는 흔적도 있다. START reply의
`receivedLength`를 읽고 그만큼 input stream을 `skip()`하지만, decompile된 같은
클래스에는 `mFileTransferIndex = receivedLength` 대입이 보이지 않는다. 그러므로
순정 코드를 맹목적으로 복제하지 않는다. 새 구현은 stream position, local offset,
meter cumulative offset을 모두 `receivedLength`로 맞춘 뒤 첫 재개 청크의 응답까지
검증해야 한다. 실제 bytecode 재검증과 중단/재개 capture 전까지는 이를 순정 앱의
resume 결함 후보로 기록한다.

### 7.3 정확한 송신 상태기계

```text
DeviceInfo + generation/series/package/hash validation
  -> 0x0A BEGIN, negotiate timeout 20 s
  -> 0x0B START(file)
       reply의 task ID / transfer ID / receivedLength 검증
       receivedLength == size: 이미 완료된 파일로 처리
       0 < receivedLength < size: stream seek 후 resume
  -> 0x0D data <= 11816 bytes
       sequence ACK + 16-byte command reply 확인
       acceptedChunk와 cumulativeReceived가 예상 offset인지 검증
       task가 살아 있는 동안 10 s 주기 0x0A CONTINUE
  -> 반복
  -> 0x0B UPDATE_TERMINATE/FINISH
  -> 모든 파일이 끝나면 0x0A DONE
  -> meter reboot/disconnect 예상
  -> reconnect 후 DeviceInfo firmware/resource/boot version 재확인
```

V5.16의 handler 주소는 `0x0A 0x08028CE4`, `0x0B 0x08028EA0`, `0x0C
0x0802902C`, `0x0D 0x080290FE`다. `0x0D` reply 16 bytes에는 u16 status, task ID,
transfer ID, detail/result, u32 accepted chunk, u32 cumulative received가 들어간다.

현재 OpenNoodoe는 `0x0D`의 task/transfer/cumulative progress를 검사하지만 START
reply의 task/transfer/`receivedLength`를 parse하지 않고 offset 0에서 시작한다.
또한 firmware task에 10초 CONTINUE, 순정과 같은 20초 negotiate timeout,
post-reboot install proof가 없다. 따라서 전송 버튼이 끝났다는 사실은 설치 성공
증거가 아니다. armed OTA를 열기 전에 이 항목들을 고쳐야 한다.

### 7.4 내부 flash에서 확인된 것과 확인되지 않은 것

```text
0x08000000..0x08007FFF  boot/recovery 후보, OTA image에 없음
0x08008000..0x0800FFFF  sectors 2/3, persistent/update-state 후보
0x08010000..0x0807FFFB  V5.16 application image
```

실행 중인 application에는 `0x080426D4`에서 sectors 2/3 범위만 preserve-update하는
routine이 있다. 그러나 이것만으로 실제 first-stage bootloader 위치, staging 위치,
install marker, rollback 정책은 확정되지 않는다. image 마지막 word
`0x70067874`도 magic/CRC/length/signature 중 무엇인지 열려 있다.

STM32 ROM bootloader는 physical recovery 수단이다. STM32F42/43 계열 ROM은 silicon
revision에 따라 USART, CAN, USB DFU, I2C, SPI 인터페이스를 지원하지만 Bluetopia,
CC256x service pack, RFCOMM을 초기화하지 않는다. BOOT0로 ROM에 진입하면 Android
SPP 연결은 유지되지 않는다.

### 7.5 완전 신규 펌웨어에서 무선 업데이트를 남기는 방법

최종 구조는 다음처럼 책임을 분리하는 것이 타당하다.

```text
immutable/recovery loader in lower internal flash
  - minimal clock, watchdog, internal flash writer
  - image manifest/hash/signature/version verification
  - install journal and power-loss recovery
  - known-good image selection

main clean-room application at 0x08010000 or dump-confirmed slot
  - full CC256x + RFCOMM/SPP stack
  - receives OTA into external NOR staging
  - verifies transport CRC and full image hash
  - writes install request journal, then resets

external NOR
  - two-slot staging or append-only package area
  - manifest, image, hash/signature, received bitmap
```

첫 독립 이미지는 internal flash를 건드리지 않는 SRAM 실행물이어야 한다. 첫 flash
이미지는 하위 stock loader를 그대로 보존한 application slot용이어야 하며, stock
loader의 image policy가 규명되기 전에는 Bluetooth로 custom image를 설치하려 하지
않는다. 장기적으로 own loader를 쓰더라도 다음 불변조건이 필요하다.

- loader sector는 정상 OTA가 erase할 수 없다.
- staging image는 전체 hash와 target board ID가 일치해야 한다.
- install journal은 erase 전, sector copy 후, 최종 commit의 단방향 상태를 갖는다.
- 전원 차단을 모든 sector 경계에서 반복해도 SWD 또는 loader recovery가 가능하다.
- `DONE`은 전송 종료일 뿐이며 성공은 reboot 후 DeviceInfo와 self-test로 판정한다.
- downgrade, cross-series, CV3/SR2 image는 기본 거부한다.

### 7.6 OTA 실험의 중단 조건

- internal flash와 NOR의 이중 dump가 없거나 hash가 다름
- RDP/WRP/option byte를 완전히 기록하지 못함
- BOOT0/NRST/SWD recovery가 재현되지 않음
- firmware package의 series, size, hash, trailer가 하나라도 불명
- 배터리 단독 전원, 엔진 가동, 차량 주행 가능 상태
- START reply의 receivedLength 또는 0x0D cumulative offset 불일치
- CTS stall, SPP disconnect, low voltage, watchdog reset

## 8. clean-room firmware 모듈 경계

```text
platform/
  startup_stm32f429.s
  clock_snapshot.c       # donor register-derived only
  linker_sram.ld
  linker_app_08010000.ld

drivers/
  diag_uart.c
  spi_polled.c
  nor_mx66_readonly.c
  i2c_polled.c
  opt3001.c
  uart5_dma_ring.c
  ft81x_host.c
  lcd_dcs.c
  backlight_tim5.c
  input_exti.c
  cc256x_hci_uart.c
  cc256x_bts_loader.c

protocol/
  vehicle_f5.c
  hci_trace.c
  spp_transport.c
  noodoe_frame.c
  file_transfer.c

boot/
  image_manifest.c
  staging_nor.c
  install_journal.c
  recovery_loader.c

app/
  bringup_main.c
  diagnostics.c
  device_info.c
```

초기에는 HAL/driver가 의미 해석을 하지 않는다. 예를 들어 `uart5_dma_ring`은 bytes,
`vehicle_f5`는 valid frames, telemetry layer는 speed/fuel 후보를 담당한다. 같은 원칙을
HCI bytes, RFCOMM frames, Noodoe commands에도 적용하면 순정 protocol을 흉내 내는
코드와 새 UI가 결합되지 않는다.

## 9. 보드를 받는 날 남겨야 할 artifact

```text
captures/board-YYYYMMDD-serial/
  photos/
  power/rails.csv
  swd/idcode.txt
  swd/option-bytes.txt
  swd/registers-stock-running.json
  flash/internal-a.bin
  flash/internal-b.bin
  flash/SHA256SUMS.txt
  nor/nor-a.bin
  nor/nor-b.bin
  logic/power-reset.sal
  logic/uart5-stock.sal
  logic/cc256x-boot.sal
  logic/spi1-display-init.sal
  logic/spi4-panel-init.sal
  logic/spi5-rdid.sal
  logic/i2c3-opt3001.sal
  logic/inputs-exti.sal
  usb/descriptors.txt
  bringup-log.md
```

각 실험은 observation, hypothesis, action, raw artifact, result, revision, next gate를 한
줄씩 남긴다. 실패한 캡처도 지우지 않는다. 보드에서 읽은 hash와 firmware V5.16
archive hash가 다르면 고정 주소와 GPIO 해석을 자동 적용하지 않는다.

## 10. 지금 확정된 결론과 실물에서 닫을 것

### 지금 확정 또는 매우 강한 것

- 차량 link는 UART5 115200 8N1의 F5/XOR protocol이다.
- speed, fuel-like, odometer, temperature 후보가 실제 firmware consumer에 연결된다.
- Android active path는 Classic SPP이고 STM32가 host stack을 가진다.
- TI CC256x 계열 controller는 USART1 HCI, RTS/CTS, DMA와 service pack을 사용한다.
- 화면은 FT81x EVE 480x480 경로와 별도 LCD DCS control, backlight로 나뉜다.
- 128 MiB Macronix NOR와 OPT3001은 read-only ID로 빠르게 식별 가능하다.
- modern OTA의 Android-side framing, chunk, resume와 keepalive 구조는 복원됐다.

### 실물 없이는 닫을 수 없는 것

- MCU suffix/package, crystal과 최종 RCC tree
- CC256x exact revision/module, reset/shutdown GPIO, service-pack 정확한 선택
- FT81x suffix와 CS/PDN/INT, LCD DDIC 모델과 reset/control polarity
- TIM5 PWM channel/pin/polarity, EXTI 입력 pin의 실제 버튼 이름
- UART `0x01/0xA1`, telemetry unknown fields, `0x41/0x42` 세부 의미
- lower flash의 bootloader/metadata layout와 option-byte 보호
- NOR filesystem, OTA staging offset, image trailer/signature와 rollback policy

이 열린 항목들은 추측으로 코드를 채우는 목록이 아니라, 첫 실물 세션에서 측정해
닫아야 할 체크리스트다.

## 11. 1차 자료와 로컬 증거

공식 자료:

- ST RM0090, STM32F429 peripheral/register reference:
  `https://www.st.com/resource/en/reference_manual/dm00031020-stm32f405415xx-stm32f407417xx-and-stm32f427437xx-and-stm32f429439xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf`
- ST AN2606, system-memory bootloader interfaces:
  `https://www.st.com/resource/en/application_note/an2606-.pdf`
- TI CC2564MODA datasheet:
  `https://www.ti.com/lit/ds/symlink/cc2564moda.pdf`
- TI CC256x service pack:
  `https://www.ti.com/tool/CC256XB-BT-SP`
- TI CC256x vendor-specific HCI guide:
  `https://www.ti.com/lit/an/swra751/swra751.pdf`
- Bridgetek FT81x programmer guide:
  `https://brtchip.com/wp-content/uploads/Support/Documentation/Programming_Guides/ICs/EVE/FT81X_Series_Programmer_Guide.pdf`
- TI OPT3001 datasheet:
  `https://www.ti.com/lit/ds/symlink/opt3001.pdf`
- Macronix MX66L1G45G datasheet:
  `https://www.macronix.com/Lists/Datasheet/Attachments/8734/MX66L1G45G%2C%203V%2C%201Gb%2C%20v1.5.pdf`

로컬 재현 근거:

- `docs/ak550-sr15-mcu-peripheral-settings.md`
- `docs/vehicle-and-external-protocol.md`
- `docs/custom-firmware-bootloader-strategy.md`
- `analysis/2026-09-01-firmware-update-architecture/README.md`
- `analysis/2026-09-02-firmware-updater-assembly-validation/README.md`
- `analysis/2026-09-01-sr15-hardware-crosscheck/README.md`
- `analysis/2026-08-31-opennoodoe-live-validation/README.md`
- `hardware/sr15-v516-board-contract.json`
- `hardware/ak550-sr15-v516-mcu-peripheral-settings.json`
