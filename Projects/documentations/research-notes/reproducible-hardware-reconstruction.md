# Noodoe SR1.5 재현 가능한 하드웨어 복원 계획

작성일: 2026-09-01 KST

이 문서는 커스텀 펌웨어용 하드웨어 BSP를 만들기 위한 기준서다. 목표는
"그럴듯한 보드 추정"이 아니라, 같은 절차를 다시 실행하면 같은 contract가
나오고, 실물 계측으로 하나씩 확정할 수 있는 재현 가능한 하드웨어 모델을
만드는 것이다.

## 현재 기준 이미지

- 대상: `KYMCO Noodoe SR1.5 V5.16`
- 파일: `artifacts/ota-archive/2026-08-31-full/blobs/firmware/1657088080998-s1-SR1.5_ota_V516.bin`
- SHA-256: `3b64673054ca84b9cf504f4cc7ebcb2e6615b9fbf59df79a37a92dcf14f037ca`
- 로드 주소: `0x08010000`
- 애플리케이션 끝: `0x0807FFFB`
- 아직 덤프되지 않은 치명 영역: `0x08000000..0x0800FFFF`

이 이미지가 바뀌면 이 문서의 확정도도 같이 바뀐다. SR1.0, SR1.5, SR2.x가
동일한 보드라고 가정하면 안 된다.

## 소스 오브 트루스

현재 하드웨어 모델은 네 겹으로 검증한다.

| 계층 | 파일 | 역할 |
| --- | --- | --- |
| 펌웨어 자동 추출물 | `analysis/2026-09-01-custom-firmware-hardware-map/hardware-map.json` | STM32 주변장치 주소 참조 증거 |
| vector 자동 추출물 | `analysis/2026-09-01-custom-firmware-hardware-map/vector-map.json` | IRQ table, reset, active handler 증거 |
| 사람이 읽는 contract | `hardware/sr15-v516-board-contract.json` | 커펌 BSP가 따라야 할 버스/핀/프로토콜 계약 |
| 핀 검증표 | `hardware/sr15-v516-pinmap.csv` | 실물 계측할 핀/신호 checklist |

추가로 `src/noodoe_protocol/vehicle_uart.py`는 차량 UART protocol의 실제 구현
상수와 checksum을 가진다. 그래서 contract의 `F5 | CMD | LEN | PAYLOAD | XOR`
주장이 코드와 달라지면 validator가 실패해야 한다.

## 자동 검증

다음 명령은 contract가 자동 추출물 및 현재 코드와 맞는지 확인한다.

```powershell
$python = '<local-user>\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
$env:PYTHONPATH = (Resolve-Path .\src).Path
& $python .\tools\validate_hardware_contract.py
```

검증기는 다음을 실패 조건으로 본다.

- contract의 firmware SHA-256이 자동 생성 hardware map과 다름.
- vector map의 firmware SHA-256이 hardware map과 다름.
- contract의 vehicle UART magic이 현재 Python 구현의 `VEHICLE_MAGIC`과 다름.
- 현재 checksum 구현이 알려진 vector `F5 01 01 02 -> F7`과 다름.
- contract에 적은 peripheral이 hardware-map에 없음.
- contract에 적은 IRQ가 vector-map의 active application vector에 없음.
- contract에 적은 구체 pin이 pinmap CSV에 없음.
- pinmap CSV의 구체 pin이 contract에서 빠짐.

이 검증이 증명하지 못하는 것도 있다.

- PCB에 실제로 같은 패키지의 부품이 실장되었는지.
- connector pin number와 harness 배선.
- 전원 rail, level shifter, reset polarity.
- USB가 실제 외부 connector로 노출되는지.
- bootloader가 어떤 서명/CRC/rollback 정책을 쓰는지.

즉 validator는 "문서가 현재 분석 산출물 및 코드와 일관되는지"를 잡는 장치다.
"분해 없이 실물 보드가 확정됐다"는 뜻은 아니다.

## 현재 재현 contract

| 서브시스템 | 현재 contract | 확정도 | 커펌에서 필요한 이유 |
| --- | --- | --- | --- |
| MCU | STM32F429xx-class Cortex-M4F | 강함 | vector/IRQ/peripheral map 기준 BSP |
| OS | FreeRTOS + CMSIS-RTOS v1, IAR EWARM | 강함 | task/tick/interrupt 구조 이해 |
| Bluetooth | USART1 HCI, 3,686,400 baud, RTS/CTS, TI CC256x 후보 | 강함 | SPP stack 또는 controller bring-up |
| 차량 링크 | UART5, 115200 8N1, DMA1, F5/XOR frame | 확정 | 속도/연료/거리/온도 후보 수신 |
| EVE 표시 | SPI1, FT81x REG_ID 0x7C | 확정 | 480x480 렌더링 |
| LCD 제어 | SPI4, MIPI-DCS-like command plane | 강함 | sleep/display on/off/panel 상태 |
| 리소스 NOR | SPI5, JEDEC `C2 20 1B`, 128 MiB | 확정 | 사진/테마/리소스 보존 및 교체 |
| 조도 센서 | I2C3, addr `0x45`, TI OPT3001 ID | 확정 | 자동 밝기 |
| MFi AuthCP | I2C1, addr `0x11` 후보 | 강함 | iAP/legacy Apple 경로 보존 |
| Backlight | TIM5 PWM 약 500 Hz 후보 | 강함 | 밝기 제어 |
| USB | USB_OTG_HS core + embedded FS PHY 후보 | 강함 | descriptor/복구/업데이트 조사 |

## 실물 재현 절차

### 1. 쓰기 금지 상태에서 identity 고정

먼저 앱 또는 SPP 도구로 다음 정보를 저장한다.

- DeviceInfo 응답.
- OQC READ 응답.
- firmware/resource version.
- paired Bluetooth name/address.
- 현재 정상 동작하는 사진/테마/알림/네비/날씨 실험 로그.

이 단계에서는 OQC WRITE, reset, firmware install, resource install을 수행하지
않는다.

### 2. 보드 분해와 광학 기록

분해 후 다음을 촬영한다.

- PCB 양면 전체 고해상도.
- MCU, Bluetooth module/chip, NOR flash, FT81x, LCD connector, 전원 IC.
- test pad, SWD/JTAG 후보, UART 후보, USB D+/D- 후보.
- connector 번호가 보이도록 harness 주변.

사진 파일명에는 날짜, 보드면, 확대 위치를 넣는다. 나중에 pinmap CSV의
`electrical_verification` 항목에 대응시킨다.

### 3. 전기적 확인

전원 인가 전:

- GND continuity.
- 3.3 V/5 V/배터리 rail 후보 저항.
- SWDIO/SWCLK/NRST 후보 단락 여부.

전원 인가 후, 쓰기 없이:

- rail 전압.
- BOOT0/NRST idle level.
- UART5 PC12/PD2의 idle level과 115200 traffic.
- USART1 PA9/PA10/PA11/PA12의 3.6864 Mbps HCI traffic.
- SPI5 `0x9F` 시점의 CS/SCK/MISO/MOSI와 `C2 20 1B`.
- SPI1 display init 중 REG_ID read path.
- I2C3 `0x45` ACK 및 `0x5449/0x3001` ID read.
- TIM5 backlight 후보 pin의 밝기 ramp 상관.

### 4. 덤프 우선순위

커펌 또는 리소스 write 전에 반드시 덤프해야 하는 순서:

1. STM32 option bytes/RDP 상태 읽기.
2. 내부 flash 전체 `0x08000000..` 덤프. 특히 현재 OTA 이미지에 없는
   `0x08000000..0x0800FFFF`.
3. 외부 NOR 전체 128 MiB 덤프.
4. 가능하면 SRAM snapshot 또는 boot log.

내부 flash가 RDP로 잠겨 있으면 강제로 해제하지 않는다. RDP 해제는 보통 mass
erase와 연결될 수 있으므로, 그 상태 자체를 먼저 기록한다.

### 5. 커펌 bring-up 순서

보드를 살리는 순서는 화면보다 저장소/통신을 우선한다.

1. vector table, clock, SysTick, fault handler.
2. UART log output 또는 SWO.
3. SPI5 NOR JEDEC read only.
4. UART5 passive receive parser.
5. TIM5 backlight 최소 제어.
6. SPI1 FT81x REG_ID read, blank-safe display init.
7. USART1 TI CC256x reset/baud/service-pack/HCI event loop.
8. SPP server 또는 기존 Android updater가 연결할 최소 transport.
9. 리소스 filesystem read only.
10. write/erase/update 기능.

## 열린 항목

- STM32 정확한 주문명, flash/RAM/package suffix.
- CC256x 정확한 variant 또는 module명.
- FT81x 세부 모델.
- LCD panel/DDIC 모델과 SPI4의 정확한 CS/D-C/reset polarity.
- TIM5 PWM 출력 pin과 channel.
- USB connector 유무와 VID/PID/interface descriptor.
- SR1.0, SR1.5, SR2.x의 메모리 맵 차이.
- bootloader 영역의 update validation, rollback, trailer/checksum 정책.

## 다음 작업

1. `tools/validate_hardware_contract.py`를 테스트 루틴에 포함한다.
2. 실물 사진을 `evidence/pcb/` 아래에 저장하고, pinmap CSV의 각 row에 증거
   파일명을 붙인다.
3. passive UART5 capture를 `tools/summarize_vehicle_uart.py`로 요약해
   `docs/vehicle-and-external-protocol.md`의 offset 추정과 대조한다.
4. USB descriptor를 수집해 `usb_device` contract의 `open_items`를 줄인다.
5. 전체 내부 flash/NOR 덤프 전까지 firmware/resource write 기능은 lab UI에만
   잠그고 실차 기본 메뉴에서는 노출하지 않는다.
