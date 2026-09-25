# Noodoe 실물 보드 초기 bring-up runbook

> **2026-09-09 재검증:** SR1 CRC 재현, 하위 flash의 16KiB shadow/4KiB program 구분, UART5 byte-IRQ 수신 및 USART1 baud 재설정에 대한 정정은 [최신 BIN·APK 대조 보고서](2026-09-09-ak550-firmware-uart-boot-review.md)를 우선한다. 아래는 당시 분석 기록이며, 충돌하는 설명을 실물 작업의 확정 근거로 사용하지 않는다.

> AK550 SR1/SR1.5 donor의 MCU, clock, pin, DMA, IRQ 설정과 수령 당일 절차는
> `docs/ak550-sr15-mcu-peripheral-settings.md`가 우선한다. 특히 과거 vector 표의
> application-range handler를 모두 활성 IRQ로 해석하지 않는다. CV3/SR2/H7 설정은
> AK550 donor에 적용하지 않는다.

작성일: 2026-09-02 KST

목표는 중고/예비 Noodoe 본체를 받아서 순정 복구 가능성을 보존한 채 하드웨어를
재현하고, 이후 커스텀 펌웨어를 올릴 수 있는 상태로 만드는 것이다. 무선 firmware
update 기능은 가능한 한 끝까지 살린다.

## 원칙

- `0x08000000..0x0800FFFF` bootloader/영구 영역은 처음부터 덮지 않는다.
- 첫 write 전에 내부 flash 전체, option bytes, 외부 NOR 전체를 저장한다.
- RDP가 걸려 있으면 해제하지 않는다. STM32 RDP 해제는 mass erase를 동반할 수
  있으므로 "읽기 불가" 자체를 증거로 기록한다.
- 커펌 1차 목표는 순정 bootloader가 받는 app slot인 `0x08010000` 이후만
  교체하는 것이다.
- 무선 업데이트 기능은 Android -> SPP -> 순정 bootloader/app update state
  machine을 가능한 한 그대로 재현한다.

## 0. 인수 직후 기록

1. 외관 사진, connector 사진, 라벨, QR/serial을 촬영한다.
2. 전원을 넣지 않은 상태에서 저항/단락을 확인한다.
3. 공급 전압, 전류 제한값, harness pinout 후보를 기록한다.
4. 순정 앱으로 연결 가능하면 DeviceInfo, OQC READ, firmware/resource version을
   저장한다.

## 1. SWD 읽기 전용 백업

먼저 ST-LINK/J-Link를 전압 감지 상태로 연결한다.

읽기 순서:

1. target voltage.
2. DBGMCU IDCODE.
3. option bytes.
4. RDP level.
5. 내부 flash read 가능 여부.
6. 가능하면 `0x08000000`부터 전체 flash dump.

저장 파일명에는 날짜, 보드 serial, 읽은 도구, voltage, RDP 상태를 넣는다.

예시 산출물:

```text
evidence/swd/2026-09-02_board-a_option-bytes.txt
evidence/swd/2026-09-02_board-a_flash-08000000-full.bin
evidence/swd/2026-09-02_board-a_read-command.log
```

## 2. 외부 NOR 백업

외부 NOR는 사진/테마/리소스/상태 데이터의 실제 저장소일 가능성이 높다.
순정 리소스 포맷을 잃지 않기 위해 firmware write보다 먼저 덤프한다.

우선순위:

1. STM32 순정 firmware를 통해 read 가능한 경로가 있는지 확인.
2. SPI5 test pad가 확인되면 clip 또는 high-Z probe로 JEDEC ID `C2 20 1B` 확인.
3. 128 MiB 전체 dump.
4. dump SHA-256 기록.

## 3. 순정 부팅 관찰

아직 firmware를 바꾸지 않은 상태에서 부팅 sequence를 관찰한다.

- UART5: 115200 8N1 traffic 여부.
- USART1: 3,686,400 baud HCI traffic 여부.
- SPI5: NOR JEDEC/read traffic.
- SPI1: FT81x `REG_ID=0x7C` read path.
- SPI4: LCD DCS-like init.
- I2C3: OPT3001 후보 ID read.
- TIM5: backlight PWM 후보.
- USB: Windows descriptor, VID/PID/interface.

이 단계는 "측정"이지 "주입"이 아니다.

## 4. 순정 무선 update 경로 보존

무선 update 기능을 살리려면 두 가지를 구분한다.

- STM32 ROM bootloader: SWD/UART/USB 같은 물리 복구 경로다. Bluetooth stack이
  꺼진 상태이므로 SPP 업데이트 경로가 아니다.
- Noodoe 순정 updater: 앱 또는 순정 bootloader/application이 살아 있을 때
  Bluetooth SPP로 firmware/resource를 받는 경로다.

따라서 커펌은 처음에는 순정 bootloader를 남기고, app 영역에서 다음을 구현한다.

1. 기존 SPP transport와 같은 pairing/name/channel 정책.
2. 기존 file negotiate/control/data/status 응답.
3. update package trailer/checksum/signature 검증 정책.
4. 실패 시 순정 bootloader 또는 백업 app으로 돌아가는 rollback 정책.

이 네 가지가 닫히기 전에는 실차용 OTA write를 켜지 않는다.

## 5. 커펌 1차 bring-up

첫 커펌은 기능보다 복구성을 우선한다.

1. `0x08010000` app slot에만 배치.
2. vector table, stack, reset handler, clock init.
3. fault handler가 UART/SWO로 죽은 주소를 남김.
4. SPI5 JEDEC read only.
5. UART5 passive parser.
6. backlight 최소 제어.
7. FT81x blank-safe init.
8. Bluetooth HCI event loop.
9. SPP echo/DeviceInfo 호환 응답.
10. update command는 read-only dry-run까지만.

## 6. write 기능 해금 조건

다음 조건을 모두 만족하기 전에는 flash erase/program을 하지 않는다.

- 내부 flash full dump 검증 완료.
- 외부 NOR full dump 검증 완료.
- bootloader 영역 해시와 app 영역 해시가 분리되어 있음.
- 최소 하나의 물리 복구 경로가 확인됨.
- 순정 app image를 다시 올리는 절차가 실험 보드에서 검증됨.
- update 실패 시 전원이 끊겨도 복구 가능한지 확인됨.

## 7. 산출물

- `hardware/sr15-v516-board-contract.json`
- `hardware/sr15-v516-pinmap.csv`
- PCB 사진과 IC marking table.
- SWD option-byte/readout report.
- internal flash dump manifest.
- external NOR dump manifest.
- USB descriptor report.
- logic-analyzer capture 목록.
- 커펌 linker script와 vector table.
- rollback/recovery 절차.
