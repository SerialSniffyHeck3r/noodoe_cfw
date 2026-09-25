# AK550 Noodoe SR1.5 V5.16 하드웨어 및 실차 로그 교차 분석

작성일: 2026-09-01 KST

대상은 보존된 `SR1.5_ota_V516.bin` 한 개다. 이 문서는 펌웨어 정적 분석,
사용자가 작성한 루트 보고서, OpenNoodoe 실차 SPP 로그를 함께 대조한 결과다.
바이크에 쓰기 명령이나 펌웨어 설치 명령은 보내지 않았다.

## 한 줄 결론

이 Noodoe는 계기판 전체가 아니라 **STM32F429 계열 MCU를 중심으로 한 보조 표시
모듈**이다. 차량 계기판/ECU 데이터는 `UART5/F5`로 받고, 휴대전화와는 TI
CC256x Bluetooth 컨트롤러를 거쳐 Classic SPP로 통신하며, FT81x EVE가
480x480 LCD를 구동하고, MX66L1G45G 128 MiB NOR에 사진·테마·리소스를 저장한다.

## 증거 등급

- **확정**: 이 바이너리 또는 실차 로그에서 직접 확인되고 공개 데이터시트와 일치.
- **강한 추론**: 명령/ID/핀/버스 특성이 한 부품으로 수렴하지만 PCB 실물 마킹은 미확인.
- **미확인**: 후보는 있으나 현재 증거로 부품명을 하나로 고를 수 없음.

## 펌웨어 정체와 메모리 배치

| 항목 | 결과 | 등급 |
| --- | --- | --- |
| SHA-256 | `3b64673054ca84b9cf504f4cc7ebcb2e6615b9fbf59df79a37a92dcf14f037ca` | 확정 |
| 이미지 크기 | `458748` (`0x6FFFC`) bytes | 확정 |
| 로드 주소 | `0x08010000` | 확정 |
| 초기 SP | `0x20025318` | 확정 |
| Reset handler | `0x08076718` | 확정 |
| 애플리케이션 범위 | `0x08010000..0x0807FFFB` | 확정 |
| 하위 영역 | `0x08000000..0x0800FFFF`는 bootloader/영구 데이터 후보 | 강한 추론 |

Reset 코드는 `SystemInit(0x08073E90)`과 IAR 런타임을 거쳐
`main(0x0802FD20)`으로 간다. IAR 압축 초기화 레코드도 확인되어 RAM 초기값과
`Bluetooth Display Board`, 제조사, MFi 관련 문자열을 복원할 수 있다.

## 하드웨어 지도

### 1. 메인 MCU: STM32F429 계열

**확정(계열), 정확한 패키지는 미확인.** Cortex-M vector table, ST HAL 주변장치
주소, FPU 명령, GPIO alternate-function 설정, `STMicroelectronics` 문자열이
모두 STM32F429 계열과 일치한다. 외부 FT81x를 쓰므로 STM32의 LTDC는 주 화면
경로가 아니다.

공식 자료: [STM32F427/429 데이터시트](https://www.st.com/resource/en/datasheet/stm32f429ni.pdf)

### 2. Bluetooth: TI CC2564/CC2564B 계열

**강하게 확정된 계열, 세부 silicon/module variant는 미확인.**

- `USART1`, `3,686,400 baud`, 8N1, RTS/CTS.
- PA9 TX, PA10 RX, PA11 CTS, PA12 RTS, AF7.
- DMA2 Stream7/5 Channel4.
- 펌웨어 내부에 TI CC256x service pack 전체가 포함된다.
- `HCI_VS_Start_VS_Lock`, `HCI_VS_Write_Memory_Block`, HCILL 명령이 존재한다.
- 첫 service-pack 대상 주소와 바이트가 TI CC2564 공개 사례와 일치한다.
- Bluetopia 버전 문자열 `4.0.2.1`이 존재한다.

즉 STM32가 Bluetooth baseband를 직접 구현하는 것이 아니라, USART1 HCI로 TI
컨트롤러를 부팅하고 패치한 뒤 Bluetopia stack/SPP server를 실행한다.

공식 자료: [TI CC256x VS HCI guide](https://www.ti.com/lit/an/swra751/swra751.pdf),
[TI SPPLEDemo guide](https://www.ti.com/lit/ug/swra772/swra772.pdf)

### 3. 차량 계기판 링크: UART5

**확정.**

- `UART5`, 115200, 8N1, flow control 없음.
- PC12 TX / PD2 RX, AF8.
- DMA1 Stream7 TX / Stream0 RX, Channel4.
- wire frame: `F5 | command | length | payload | XOR`.
- 수신 명령: `0x21`, `0x22`, `0x41`, `0x42`.
- 송신 명령: `0x01` 1-byte bitmask, `0xA1` 2-byte payload.

`0x21/0x22`는 첫 4바이트의 상태/니블 플래그, payload+4의 little-endian u32,
payload+8의 `raw - 0x28` 온도 후보를 내부 상태로 옮긴다. `0x22`에는 추가 u16이
있다. `0x41`은 정확히 250 bytes, `0x42`는 정확히 71 bytes다.

### UART5와 SPP의 직접 브리지

이 부분은 길이 유사성이 아니라 RAM 주소와 호출 경로까지 닫혔다.

| 차량 UART5 입력 | 내부 경로 | 휴대전화 SPP 출력 | 판정 |
| --- | --- | --- | --- |
| `F5 0x21/0x22` 상태 | `0x080330F0`, 선택 필드가 기준값 5를 상/하향 통과 | `C2 NOTIFY_RIDING`, payload `1/0` | 확정 |
| `F5 0x41`, 250 bytes | `0x20021A40` 저장, getter `0x08033314` | `0x16 GET_METER_PROFILE`, status u16 + 동일 250 bytes | 확정 |
| `F5 0x42`, 71 bytes | `0x200227D4` 저장, event 5 | `C6 UPDATE_BATTERY_DATA`, 71 bytes | 확정(이 펌웨어) |

`0x16` read는 내부 event `0x1D`로 변환되어 위 250-byte RAM 버퍼를 가져온다.
실차 캡처의 응답 길이도 정확히 252였고 status는 0이었지만, 데이터 250 bytes는
전부 0이었다. `C2`와 실제 inner-command `C6`는 현재 모든 캡처에서 한 번도
관측되지 않았다. 로그에 보이는 outer sequence 값 `C6`나 ACK의 `C6`를 command
ID로 오인하면 안 된다.

공식 Android 앱은 이 기능을 Ionex용 `MeterProfile`과 `BatteryRunTimeInfo`로
명명한다. 앱의 `InputCommandParser`와 firmware serializer를 다시 맞춘 결과는
다음과 같다.

- `0x16`: status 2 bytes 뒤에 VCU serial 16, meter firmware 1,
  full battery profile 99 + 99, default battery profile/padding 35 bytes가 온다.
  합계는 firmware가 만드는 250 bytes와 정확히 같다.
- `C6`: full battery status 29 + 29, default battery status 13 bytes다.
  합계 71 bytes이며 firmware serializer와 순정 앱의 최소 command 길이
  `79 = 8-byte header + 71-byte body`가 정확히 일치한다.

따라서 이전의 "앱은 C6 90-byte body를 기대한다"는 해석은 폐기한다. 이
firmware와 보존된 순정 APK는 이 Ionex 경로에서 같은 포맷 세대다. 다만 AK550
실차에서 `0x16` profile이 전부 0이고 `C2/C6`가 미관측인 점, 순정 코드의 명칭을
함께 보면 이 경로가 AK550 일반 주행정보가 아니라 여러 차종용 공용/Ionex
호환 코드라는 해석은 여전히 가장 강하다.

이 경로가 **바이크 -> Noodoe**다. 휴대전화 SPP 원문과 같은 프로토콜이 아니다.

### 4. 화면: FTDI/Bridgetek FT81x EVE

**확정된 family, FT810/811/812/813 세부형은 미확인.**

- SPI1: PA6 MISO, PB3 SCK, PB5 MOSI, AF5.
- `REG_ID=0x7C`, `0x302000` 계열 register map과 command FIFO가 FT81x와 일치.
- LCD timing: 480x480 active, HCYCLE 550, HOFFSET 37, HSYNC 0..4,
  VCYCLE 505, VOFFSET 18, VSYNC 0..2, SWIZZLE 3, PCLK 후보 4.
- JPEG/PNG/RAW 리소스는 STM32가 FT81x display list/asset로 조합한다.

공식 자료: [FT81x Programmer Guide](https://brtchip.com/wp-content/uploads/Support/Documentation/Programming_Guides/ICs/EVE/FT81X_Series_Programmer_Guide.pdf)

### 5. 저장장치: Macronix MX66L1G45G

**부품 family/ID는 확정, 정확한 package와 grade는 미확인.**

펌웨어의 flash driver는 다음을 수행한다.

- `0x9F RDID` 후 `C2 20 1B`가 아니면 장치를 거부.
- 총 용량 `0x08000000` bytes = 128 MiB = 1 Gbit.
- 4 KiB sector, 32 KiB block, 256-byte page.
- `0x03/0x13` read, `0x02/0x12` program, `0x20/0x21` sector erase처럼
  주소 상위 바이트 유무에 따라 3-byte/4-byte 명령을 나눔.
- `0x06` write enable, `0x05` status poll.

Macronix 공식 MX66L1G45G 데이터시트의 RDID가 정확히 `C2 20 1B`다. SPI5가
8-bit, DIV2, DMA RX/TX이고 대량 파일 경로에 적합하므로 이 flash의 버스로 보는
것이 매우 강하다. PCB에서 trace/마킹을 확인하기 전까지 bus 결론만 “강한 추론”으로 둔다.

공식 자료: [MX66L1G45G 데이터시트](https://www.macronix.com/Lists/Datasheet/Attachments/8734/MX66L1G45G%2C%203V%2C%201Gb%2C%20v1.5.pdf)

### 6. USB

**확정.** `USB_OTG_HS` core (`0x40040000`)를 사용하지만 설정은
`USB_OTG_SPEED_HIGH_IN_FULL=1`, `USB_OTG_EMBEDDED_PHY=2`, EP0 64 bytes다.
따라서 외부 ULPI HS PHY가 아니라 **HS core의 내장 FS PHY**, 즉 실제 링크는
Full Speed다. “USB HS core를 썼으니 외부 ULPI 칩이 있다”는 해석은 틀리다.

공식 상수: [ST USB low-level header](https://github.com/STMicroelectronics/stm32f4xx-hal-driver/blob/master/Inc/stm32f4xx_ll_usb.h)

### 7. I2C와 남은 버스

| 버스 | 확인 내용 | 현재 판정 |
| --- | --- | --- |
| I2C1 | 100 kHz, Bluetopia/iAP 인접 코드가 register `0x10..0x30`을 교환 | Apple MFi authentication coprocessor 경로로 강한 추론 |
| I2C3 | 400 kHz, 7-bit, 1-byte register-address read/write | 실제 slave는 미확인 |
| SPI4 | PE2/PE5/PE6 AF5, 16-bit, DIV16, polling | 실제 부품 미확인 또는 생산/옵션 회로 |
| SPI5 | PF7/PF8/PF9 AF5, 8-bit, DIV2, DMA2 | MX66L1G45G 저장장치로 강한 추론 |

## Bluetooth 로그와 펌웨어의 직접 대조

### 계층 구조

```text
Android app
  -> RFCOMM/SPP outer transport: 5A FF, sequence, ACK
  -> Noodoe app command: A5 5A | cmd | attr | ... | lengthLE | payload | checksum
  -> STM32/Bluetopia command dispatcher
  -> display, NOR filesystem, OQC state, notification renderer

Vehicle meter/ECU
  -> separate UART5: F5 | cmd | length | payload | XOR
  -> STM32 vehicle-state parser
  -> 0x21/0x22 can emit C2; 0x41 backs command 0x16; 0x42 can emit C6
```

펌웨어 `0x08025EB2`에 `A5 5A`가 있고, `0x08027F98`의 builder가 이를
packet[0:2]에 쓴다. `0x08073F50`의 0x00..0x16 handler table은 실차에서 본
명령 ID와 일치한다.

| 명령 | 펌웨어의 길이/조건 | 실차 로그 | 판정 |
| --- | --- | --- | --- |
| `0x05 DEVICE_INFO` | read 0 bytes, reply 82 bytes | 0 / 82 | 일치 |
| `0x09 WEATHER` | write 최소/정확 51-byte 구조 | 51 | 일치 |
| `0x0A NEGOTIATE` | write 27, reply 5 | 27 / 5 | 일치 |
| `0x0B FILE_CONTROL` | START 최소 32, reply 10 | 32 / 10 | 일치 |
| `0x0D FILE_DATA` | write 최소 7, progress reply 16 | 대형 JPEG chunk / 16 | 일치 |
| `0x11 OQC_DATA` | write 137, read reply 139 또는 status 2 | read 0 / reply 139 | 일치 |
| `0x12 OQC_TEST` | write state, reply 3 | OQC start/stop 동작 | 구조 일치 |
| `0x15 NOTIFICATION` | 최소 6, u16-length 문자열 3개 | 실차 알림 성공 | 구조/동작 일치 |
| `0x16 METER_PROFILE` | read 0, reply status 2 + profile 250 | 0 / 252, profile all zero | 경로/길이 일치 |
| `0xC1` | attr 0x10, 67 bytes | RUNNING_CREATION 67 | 일치 |
| `0xC2` | attr 0x10, 1 byte | 미관측 | UART 상태 전이가 없었던 것과 양립 |
| `0xC3` | attr 0x10, 1 byte | key-on notify 반복 관측 | 일치 |
| `0xC4` | attr 0x10, 5 bytes | OQC_NOTIFY 5 | 일치 |
| `0xC6` | attr 0x10, 71 = 29 + 29 + 13 bytes | 미관측 | 순정 앱 parser와 정확히 일치; AK에서는 비활성/미사용과 양립 |

파일 전송 실패도 펌웨어 판정과 설명이 된다. 실차는 negotiate에
`INVALID_STATE(5)` 또는 `INVALID_DATA(8)`를 돌려줬고, 이는 RF/프레이밍 실패가
아니라 STM32 command handler가 payload/state를 정상 수신한 뒤 거부한 것이다.
사진이 안 들어간 핵심은 Bluetooth 물리층이 아니라 위치/identity/task 상태기계다.

여기에 과거 실패를 설명하는 더 직접적인 표본이 있다. 2026-08-31 로그의
`0x0A` 5건은 payload가 26 bytes였고 계기판이 매번 status `6`으로 거부했다.
2026-09-01 수정본은 27 bytes를 보냈고 같은 AK550이 status `0`으로 negotiate를
승인한 뒤 `0x0B START`까지 진행했다. 즉 이 사례는 추측이 아니라 **한 바이트
누락 -> 명령 거부, 길이 수정 -> 성공**의 A/B 기록이다.

### 아직 연결하지 못한 값

`0x21/0x22`에서 C2 전이를 결정하는 nibble의 실제 계기판 의미와 나머지 상태
offset은 아직 닫히지 않았다. 현재 로그에는 이 값이 기준 5를 넘나드는 실험이
없다. 다음 실차 실험은 임의 패킷 주입이 아니라 **정지 상태에서 한 입력만
바꾸는 상관 분석**이어야 한다.

순정 APK 쪽 의미 부여의 근거는
`analysis/jadx/com.noodoe.sunray_2.1.14/.../InputCommandParser.java`와
`.../utils/IonexCommonStruct.java`다. 특히 `MeterProfile`,
`FullBatteryProfile(99)`, `FullBatteryStatus(29)`,
`DefaultBatteryStatus(13)`의 이름과 offset은 펌웨어만 보고 붙인 추정명이 아니다.

## 커스텀 펌웨어 관점의 의미

1. 커스텀 앱만 만들 때는 TI HCI나 FT81x를 직접 다룰 필요가 없다. 기존
   SPP `5A FF` + `A5 5A` 계층만 재현하면 된다.
2. 커스텀 STM32 펌웨어를 만들 때는 최소 4개 driver가 필요하다: TI HCI,
   UART5 F5, FT81x SPI1, MX66L1G45G SPI5.
3. 128 MiB NOR는 테마/사진/리소스의 실제 보존 대상이다. 파일시스템 구조를
   모른 채 erase/program하면 MCU firmware보다 훨씬 많은 사용자/공장 데이터를 잃는다.
4. CC256x service pack은 MCU image 안에 들어 있으므로 커펌에서도 controller
   reset, baud 전환, patch download, HCILL 순서를 재현해야 한다.
5. bootloader 영역과 OQC/identity storage는 아직 dump되지 않았다. 커펌보다
   먼저 SWD readout 가능 여부, 전체 internal flash, external NOR 원본을 보존해야 한다.

## 다음 실험

1. PCB를 열기 전, OQC read와 DeviceInfo를 다시 읽어 현재 identity를 이 보고서와 묶는다.
2. 시동 ON, 엔진 OFF 상태에서 버튼 하나/밝기 하나만 바꾸며 `C1..C6` 로그를 수집한다.
3. 엔진 ON 실험은 차속 0에서 coolant/fuel/odometer 중 한 값만 변하는 장시간 캡처로 제한한다.
4. USB 연결 시 Windows descriptor와 VID/PID/interface를 읽어 firmware의 USB strings와 대조한다.
5. 분해 시 MCU, CC256x module, MX66L1G45G, FT81x 마킹과 SPI/I2C trace를 고해상도로 촬영한다.
6. SWD 접근 전 전압, NRST, SWDIO/SWCLK, readout protection을 읽기 전용으로 확인한다.

## 재현

```powershell
$python = '<local-user>\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
& $python .\analysis\2026-09-01-sr15-hardware-crosscheck\analyze_sr15.py `
  .\artifacts\ota-archive\2026-08-31-full\blobs\firmware\1657088080998-s1-SR1.5_ota_V516.bin `
  .\captures `
  --output .\analysis\2026-09-01-sr15-hardware-crosscheck\crosscheck.json
```

스크립트는 크기와 SHA-256이 다르면 고정 주소 분석을 거부한다. 생성된
`crosscheck.json`은 firmware handler table과 모든 `protocol.log`의 실제 명령/길이
빈도를 함께 기록한다.
