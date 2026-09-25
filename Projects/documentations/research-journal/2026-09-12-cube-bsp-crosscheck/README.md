# Cube BSP 설정 보강: SR1.5 V5.16 원본 재확인

2026-09-12. Cube IOC 안내를 위해 기존 하드웨어 JSON의 미확정 항목 일부를 원본에서 다시 읽었다. 프로젝트 생성·수정, 장치 접속, 원본 변경은 수행하지 않았다. 별도 OpenNoodoe 프로젝트 및 그 실행 기록은 이번 판단의 근거로 사용하지 않았다.

## 원본과 재현 증거

- 원본: [1657088080998-s1-SR1.5_ota_V516.bin](C:/shared/KYMCO/Reversing/artifacts/ota-archive/2026-08-31-full/blobs/firmware/1657088080998-s1-SR1.5_ota_V516.bin)
- SHA-256: `3b64673054ca84b9cf504f4cc7ebcb2e6615b9fbf59df79a37a92dcf14f037ca`
- 길이 `458748` (`0x6FFFC`), 로드 주소 `0x08010000`.
- [이번에 저장한 주소·명령어·리터럴 발췌](C:/shared/KYMCO/Reversing/analysis/2026-09-12-cube-bsp-crosscheck/targeted-disassembly.txt)
- [사용한 기존 disassembler](C:/shared/KYMCO/Reversing/analysis/2026-09-01-sr15-hardware-crosscheck/disassemble_targets.py): 입력 SHA와 크기를 검사한 뒤 Thumb 명령과 PC-relative literal을 출력한다. 발췌는 선택한 함수 범위이며 원본 소스/심볼 복원이 아니다.
- GPIO 테이블은 [기존 복원 startup-data.bin](C:/shared/KYMCO/Reversing/analysis/2026-09-10-sleep-entry/mcu/startup-data.bin) 및 [복원 방법](C:/shared/KYMCO/Reversing/analysis/2026-09-10-sleep-entry/mcu/README.md)으로 대조했다. 이것은 실측 SRAM 덤프가 아니라 동일 BIN의 초기값 복원이다.

GPIO 포트 표 `0x20000998`의 `+0/+4/+0x20`은 각각 GPIOA `0x40020000`, GPIOB `0x40020400`, GPIOI `0x40022000`이다. 마스크 표 `0x20000A20`의 `+0/+2/+8`은 각각 `0x0001/0x0002/0x0010`이다. 아래 핀명은 이 초기값과 실제 호출의 조합이며 커넥터 번호를 뜻하지 않는다.

## 1. SPI1: EVE transport

`0x08033C1A`는 인수 r0를 r4에 보존하고 SPI1 handle `0x20022438`을 구성한다. `0x08033C40..0x08033CA0`에서 확인한 값:

| 항목 | 코드 값 | 해석 |
|---|---|---|
| Instance | `0x40013000` | SPI1 |
| Mode | `0x104`, handle+4 | Master |
| Direction / DataSize | 0 / 0, +8 / +0x0C | 2-line / 8-bit |
| CLKPolarity / CLKPhase | 0 / 0, +0x10 / +0x14 | Mode 0 |
| NSS | `0x200`, +0x18 | Software NSS |
| BaudRatePrescaler | r4, +0x1C | 호출자 선택 |
| FirstBit / TIMode / CRCCalculation | 0 / 0 / 0 | MSB first, TI mode off, CRC off |

직접 호출자 두 곳도 확인했다. `0x0801FD2E`의 인수 `0x10`은 **DIV8**, `0x0801FF84`의 인수 `0x08`은 **DIV4**이다. 초기화 경로 중 속도 전환이 있으므로 SPI1 divider가 항상 하나라고 기록하지 않는다. 실제 SCK Hz는 APB2 클록 확인 뒤 계산한다.

핀은 기존 분석의 **PB3 SCK / PA6 MISO / PB5 MOSI, AF5**와 일치한다. MSP `0x080373F2..0x08037426`은 AF push-pull, no pull, GPIO speed 값 1로 구성한다. 이 MSP 범위는 기존 disassembler로 재확인했으며 이번 발췌 파일에는 init/제어/호출자 부분을 수록했다.

### CS와 PDN/reset 후보

- **PA4**: `0x08033D00..0x08033D0E`에서 GPIOA, mask `0x10`, 값 0으로 transaction 시작 직전에 출력한다. EVE transfer의 active-low CS 해석을 뒷받침한다. 이번 발췌는 CS assert 지점을 포함하며 모든 transfer 종료·오류 분기의 해제까지 감사한 것은 아니다.
- **PB1**: `0x08034140..0x0803419A`에서 GPIOB, mask `0x02`를 제어한다. 인수 r1이 비영이면 LOW → delay 함수에 `0x14` 전달 → HIGH → 같은 delay이고, 0이면 HIGH → delay → LOW → delay이다. EVE 초기화 호출자 `0x0801FD22`는 인수 1을 전달한다. 따라서 **EVE PDN/reset 제어 후보**를 PB1로 좁힐 수 있다. 물리 FT81x PDN 핀까지의 연결과 지연 단위는 이번 범위에서 별도 확정하지 않았다.

## 2. SPI5 DMA: 실제 stream/channel과 남은 주의점

SPI5 MSP 분기의 `0x0803750A..0x08037616`이 DMA handle을 SPI handle에 직접 연결한다.

| 역할 | DMA instance / channel | handle 연결 |
|---|---|---|
| TX | DMA2 Stream4 `0x40026470`, Channel2 `0x04000000`, memory-to-peripheral `0x40` | handle `0x2002213C` → SPI handle+0x48 |
| RX | DMA2 Stream3 `0x40026458`, Channel2 `0x04000000`, peripheral-to-memory 0 | handle `0x2002219C` → SPI handle+0x4C |

둘 다 peripheral increment off, memory increment on, Normal mode다. **DMA2 Stream0은 이 SPI5 TX/RX 쌍이 아니다.** 기존 JSON의 `Stream0/3/4 candidate`를 셋 모두 SPI5용으로 설정하면 안 된다.

이 MSP는 peripheral/memory data alignment에 각각 `0x800/0x2000`을 넣어 **halfword**로 설정한다. 한편 SPI5 초기화 `0x080430F0..0x08043150`은 8-bit DataSize와 DIV2를 설정하고, byte 전송 wrapper `0x080431C6..0x08043200`는 CR1 DFF도 직접 지운다. 그러므로 초기 IOC 필드만으로 최종 DMA 전송 단위까지 단순 재현할 수 없다. 동적 전송 단위·길이/버퍼 처리의 전체 경로는 이번 범위에서 닫지 않았다. 새 BSP에 halfword DMA를 그대로 복사하라는 권고가 아니다.

## 3. TIM5: CH4와 PI0 확인

이전 JSON의 채널/핀 미확정 상태를 다음 범위에서 보강한다.

| 항목 | 원시 주소·값 | 해석 |
|---|---|---|
| TIM instance | `0x0804351A..22`: `0x40000C00` → handle `0x20022998` | TIM5 |
| ARR | `0x08043524..2A`: `0x63` | 99 |
| PSC | `0x0804352C..3A`: 선택한 timer clock / `0xC350` − 1 | counter 목표 50kHz, ARR 기준 PWM 목표 500Hz |
| Channel | `0x080435CC`, `0x08043604`, `0x08043620`: `0x0C` | TIM_CHANNEL_4 |
| PWM mode / polarity | `0x080435F4..0x08043602`: `0x60`, polarity 0 | PWM1, active HIGH |
| GPIO MSP | `0x08038224..0x08038240`: GPIOI, mask1, AF2 | **PI0 / TIM5_CH4** |
| GPIO electrical mode | 같은 MSP: mode2, pull1, speed3 | AF push-pull, pull-up, very-high speed |

`0x080435B6..0x0804363C`은 기존 CH4 출력을 정지한 뒤, 인수가 0이면 재시작하지 않고, 비영이면 duty를 구성해 재시작한다. 인수 100 이상은 99로 제한한다. 핀에서 실제 LED 밝기까지의 극성·외부 드라이버 연결은 물리 검증과 구분한다.

주의: 기존 문서의 handle `0x2002295C`는 TIM5 IRQ `0x08070E28` 쪽에서 보이는 별도 참조다. **위 PWM 초기화가 사용하는 handle은 `0x20022998`**이다. 두 객체의 운용 관계 전체를 이번에 복원한 것은 아니므로 주소를 혼용하지 않는다.

## 적용 한계

이 메모는 특정 V5.16 원본의 MCU 설정을 확인한 것이다. 실제 회로도, 커넥터 핀 번호, 모든 board revision의 동작, 정상 운용 최종 클록을 대신하지 않는다. 기존 JSON/문서를 일괄 수정하지 않았으며 [Cube IOC 안내](C:/shared/KYMCO/Reversing/docs/2026-09-12-cube-ioc-bsp-bringup.md)에서 이 메모의 확인 범위를 인용한다.
