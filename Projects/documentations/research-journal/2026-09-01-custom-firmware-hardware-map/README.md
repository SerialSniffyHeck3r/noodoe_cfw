# SR1.5 V5.16 커스텀 펌웨어 하드웨어 맵

이 문서는 해시가 일치하는 `SR1.5_ota_V516.bin`에서 자동 추출한 주변장치
주소 참조 맵이다. 보드 실물 마킹과 trace 확인 전까지는 부품 실장 여부가
아니라 **펌웨어가 접근하는 주소와 커펌에서 재구현해야 할 후보 driver**로 읽는다.

## 대상 이미지

- 경로: `artifacts\ota-archive\2026-08-31-full\blobs\firmware\1657088080998-s1-SR1.5_ota_V516.bin`
- SHA-256: `3b64673054ca84b9cf504f4cc7ebcb2e6615b9fbf59df79a37a92dcf14f037ca`
- 로드 주소: `0x08010000`
- 크기: `458748` bytes

## 커펌 bring-up 우선순위

| 주변장치 | 역할 | PC-relative load | literal | 판정 |
| --- | --- | ---: | ---: | --- |
| `USART1` | Bluetooth controller HCI | 5 | 4 | 주소 참조 확인 |
| `UART5` | vehicle meter/ECU F5 link | 8 | 2 | 주소 참조 확인 |
| `SPI1` | FT81x EVE display | 5 | 2 | 주소 참조 확인 |
| `SPI5` | external NOR storage candidate | 6 | 2 | 주소 참조 확인 |
| `I2C1` | MFi/authentication candidate | 3 | 2 | 주소 참조 확인 |
| `I2C3` | unidentified I2C path | 4 | 2 | 주소 참조 확인 |
| `SPI4` | unidentified SPI path | 4 | 2 | 주소 참조 확인 |
| `USB_OTG_HS` | USB device/update/debug candidate | 2 | 38 | 주소 참조 확인 |
| `DMA1` | UART5 DMA support | 26 | 6 | 주소 참조 확인 |
| `DMA2` | USART1/SPI5 DMA support | 23 | 8 | 주소 참조 확인 |
| `RCC` | clock tree | 276 | 36 | 주소 참조 확인 |

## 참조가 잡힌 STM32 주변장치

| 이름 | 주소 범위 | PC-relative load | literal | 메모 |
| --- | --- | ---: | ---: | --- |
| `RCC` | `0x40023800..0x40023BFF` | 276 | 36 |  |
| `FLASH_REG` | `0x40023C00..0x40023FFF` | 105 | 9 |  |
| `DMA1` | `0x40026000..0x400263FF` | 26 | 6 | UART5 DMA candidate |
| `DMA2` | `0x40026400..0x400267FF` | 23 | 8 | USART1/SPI5 DMA candidate |
| `EXTI` | `0x40013C00..0x40013FFF` | 23 | 8 |  |
| `PWR` | `0x40007000..0x400073FF` | 18 | 5 |  |
| `TIM1` | `0x40010000..0x400103FF` | 9 | 2 |  |
| `TIM8` | `0x40010400..0x400107FF` | 9 | 2 |  |
| `UART5` | `0x40005000..0x400053FF` | 8 | 2 | vehicle meter/ECU link candidate |
| `TIM5` | `0x40000C00..0x40000FFF` | 6 | 3 |  |
| `SPI5` | `0x40015000..0x400153FF` | 6 | 2 | MX66L1G45G external NOR candidate |
| `USB_OTG_FS` | `0x50000000..0x5003FFFF` | 5 | 40 |  |
| `USART1` | `0x40011000..0x400113FF` | 5 | 4 | TI CC256x HCI UART candidate |
| `GPIOA` | `0x40020000..0x400203FF` | 5 | 3 |  |
| `GPIOF` | `0x40021400..0x400217FF` | 5 | 3 |  |
| `GPIOI` | `0x40022000..0x400223FF` | 5 | 3 |  |
| `RTC_BKP` | `0x40002800..0x40002BFF` | 5 | 3 |  |
| `GPIOB` | `0x40020400..0x400207FF` | 5 | 2 |  |
| `SPI1` | `0x40013000..0x400133FF` | 5 | 2 | FT81x EVE display candidate |
| `SYSCFG` | `0x40013800..0x40013BFF` | 5 | 1 |  |
| `GPIOC` | `0x40020800..0x40020BFF` | 4 | 3 |  |
| `GPIOD` | `0x40020C00..0x40020FFF` | 4 | 3 |  |
| `GPIOE` | `0x40021000..0x400213FF` | 4 | 3 |  |
| `GPIOG` | `0x40021800..0x40021BFF` | 4 | 3 |  |
| `I2C3` | `0x40005C00..0x40005FFF` | 4 | 2 | unidentified 400 kHz slave path |
| `SPI4` | `0x40013400..0x400137FF` | 4 | 2 | unknown SPI peripheral |
| `GPIOH` | `0x40021C00..0x40021FFF` | 3 | 2 |  |
| `I2C1` | `0x40005400..0x400057FF` | 3 | 2 | MFi/authentication candidate |
| `USB_OTG_HS` | `0x40040000..0x4007FFFF` | 2 | 38 | HS core used as full-speed device |
| `TIM2` | `0x40000000..0x400003FF` | 2 | 11 |  |
| `USART6` | `0x40011400..0x400117FF` | 2 | 2 |  |
| `TIM3` | `0x40000400..0x400007FF` | 2 | 1 |  |
| `TIM4` | `0x40000800..0x40000BFF` | 2 | 1 |  |
| `CRC` | `0x40023000..0x400233FF` | 1 | 1 |  |
| `FMC` | `0xA0000000..0xA0000FFF` | 1 | 1 |  |
| `IWDG` | `0x40003000..0x400033FF` | 1 | 1 |  |
| `TIM10` | `0x40014400..0x400147FF` | 1 | 1 |  |
| `TIM11` | `0x40014800..0x40014BFF` | 1 | 1 |  |
| `TIM9` | `0x40014000..0x400143FF` | 1 | 1 |  |
| `RNG` | `0x50060800..0x50060BFF` | 0 | 1 |  |

## 해석

- `USART1`, `UART5`, `SPI1`, `SPI5`, `I2C1`, `I2C3`, `SPI4`, `USB_OTG_HS`가
  자동 스캔에서도 독립된 driver 후보로 잡히면 기존 하드웨어 모델과 맞물린다.
- `GPIO*`, `RCC`, `DMA1`, `DMA2`는 버스 자체보다 pinmux, clock, DMA 경로를
  복원할 때 시작점으로 쓴다.
- `literal_count`는 데이터 테이블의 주소값도 포함할 수 있다. `pc_relative_load`가
  더 강한 코드 접근 증거지만, 둘 다 최종 PCB 증거를 대체하지 않는다.
- `USB_OTG_FS` 주소 참조도 잡힌다. 현재 우선 결론은 기존 교차분석의
  `USB_OTG_HS core + embedded FS PHY`이며, 이 표의 FS 참조는 HAL 공용 코드나
  보조 USB 경로 후보로 따로 추적한다.

## 재현

```powershell
$python = '<local-user>\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
& $python .\analysis\2026-09-01-custom-firmware-hardware-map\build_hardware_map.py `
  .\artifacts\ota-archive\2026-08-31-full\blobs\firmware\1657088080998-s1-SR1.5_ota_V516.bin `
  --json .\analysis\2026-09-01-custom-firmware-hardware-map\hardware-map.json `
  --markdown .\analysis\2026-09-01-custom-firmware-hardware-map\README.md
```

