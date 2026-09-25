# 2026-09-12 — Noodoe APP 인계 및 HAL/RTOS 브링업

## 결과

순정 BL을 보존한 새 APP를 실제 모듈에서 실행했다. Debug/Release 모두 빌드 오류·경고가 없고, APP 읽기 비교 및 하위 64 KiB 보존 확인, 순정 BL을 거친 소프트웨어 system reset 3회, HAL TIM6 tick/FreeRTOS tick/태스크 heartbeat 증가를 통과했다.

최종 장치에는 Debug 이미지를 다시 기록하고 wrapper로 1회 더 검증했다. 준비 과정에서 추가한 watchdog debug freeze 비트는 원래의 0으로 복구하고 코어를 실행 상태로 남겼다. `final-running-state.log`가 마지막 장치 상태의 근거다.

| 항목 | Debug | Release |
|---|---:|---:|
| APP BIN 크기 | 20,848 bytes | 13,072 bytes |
| 실행 기준 클록 | HSI 16 MHz | HSI 16 MHz |
| 반복 reset 시험 | 3회 통과 | 3회 통과 |
| 관측 heap 최소 여유 | 61,312 bytes | 61,320 bytes |
| defaultTask stack 최소 여유 | 984 words / 3,936 bytes | 997 words / 3,988 bytes |
| boot 상태 / task 단계 | 0x20 / 6 | 0x20 / 6 |
| fault magic/code | 0 / 0 | 0 / 0 |

stack 수치는 시험 경로의 high-water mark이며 아직 구현하지 않은 드라이버의 최대 사용량을 보장하지 않는다. IWDG에는 reload만 수행하며 새로 시작하거나 옵션 바이트를 변경하지 않았다.

## 실제 BL 인계를 확인한 근거

- system reset 후 `g_bsp_boot.entry_vtor = 0x20000000`: 순정 BL의 SRAM 벡터 상태에서 진입했다.
- 진입 CONTROL/BASEPRI/IPSR는 모두 0이었다. PLLCFGR는 `0x07405419`, CFGR는 `0x0000940A`였다.
- C 초기화 전 HSI로 전환하고 APP의 FLASH 벡터를 설치했다. main baseline의 VTOR는 `0x08010000`, PRIMASK는 1이었다.
- 이후 태스크에서는 CONTROL=2(PSP, privileged), BASEPRI=0, PRIMASK=0이었다.
- Debug의 각 두 시점 사이 heartbeat가 22회, kernel tick이 2,288, HAL tick이 2,289 증가했다. Release는 heartbeat 23회, kernel tick 2,346, HAL tick 2,347 증가했다.
- TIM6 IRQ 카운터도 증가했다. 이 카운터는 즉시 갱신되며 HAL/kernel 관측값은 태스크가 주기적으로 복사한다. snapshot 중 CPU halt 영향도 있어 세 카운터의 절대값이 같아야 한다고 판단하지 않는다.

이는 `main` 주소로 PC를 강제로 설정한 시험이 아니다. debugger breakpoint 없이 symbol-address 기반 RAM snapshot과 system reset으로 실제 실행 경로를 검증했다.

## FLASH 보존과 검증

- MCU ID `0x419`, 512 KiB, RDP `0xAA`. ST-LINK serial `[redacted ST-LINK serial]`, V2J34S7.
- 쓰기는 `0x08010000`부터 검증된 APP BIN 크기만큼 수행했다. 이번 이미지에서 erase 대상은 sector 4 하나다.
- `[0x08000000, 0x08010000)`의 BL/설치 메타/기기 데이터는 기록 전후 순정 백업과 byte-for-byte 일치한다.
- 보존 64 KiB SHA256: `f38ed839c4fbce74d4d8227fcf13764194f405c4860cda75c628fce66064b574`.
- 순정 전체 512 KiB SHA256: `38207bed741a8fef9e6d2ed24b86c8cb8b376ba5e4c957163f5edd83fbeb8037`.
- Debug BIN SHA256: `7bd0a4cf249fa7e7d73ac955c0a3980548113979a620f17c1aacb8ce4e4f946d`.
- Release BIN SHA256: `4c0fcd36632d26491b54d440cd89aa15b0da418ad4b9500e57d6517c55bc7af3`.

APP 기록 시 마지막 사용 sector에서 BIN 뒤의 남은 공간은 erased 상태가 될 수 있고, 뒤쪽 sector에는 이전 APP의 미사용 코드가 남는다. 현재 진입점과 링크 범위는 새 이미지 안에 있으며 하위 BL 보존과는 별개의 사실이다. 모든 APP sector가 새 이미지로 채워졌다고 주장하지 않는다.

## 이번에 발견하고 처리한 문제

1. 처음 950 kHz/run 상태의 전체 dump 두 개는 51 bytes 차이가 났다. `pre-flash-a.bin`, `pre-flash-b.bin`은 신뢰할 백업이 아니다.
2. 100 kHz/core halt로 바꾼 `pre-flash-halted-100-a.bin`과 `...-b.bin`은 서로 같고 기존 순정 전체 dump와도 일치했다. 이후 자동 작업은 100 kHz를 고정한다.
3. 첫 `HOTPLUG` APP 기록은 sector 4 erase에서 실패했다. 같은 이미지·속도로 `NORMAL reset=SWrst`를 사용하니 기록과 검증이 성공했다. 자동 기록은 이 접속 방식을 사용하고 관측은 HOTPLUG를 유지한다. 이 비교만으로 erase 실패의 내부 원인을 특정하지 않는다.
4. MPU 정리를 C 함수 내부에서만 하면 그 함수의 prologue가 먼저 stack에 접근한다. assembly 진입점에서 C 호출 전에 MPU를 해제하도록 보완했다.

## 코드와 자동화

프로젝트: `Reversing/STM32CubeProjects/FuckNudo_Noodoe_CFW_Project`.

- `Drivers/BSP/src/bsp_reset.s`: strong Reset_Handler. 생성 weak startup을 직접 수정하지 않는다.
- `bsp_boot.c`: pre-C BL 인계, NVIC/SysTick/DMA 정리, 제한된 clock wait, 선택적 peripheral reset, main baseline 검사.
- `bsp_bringup.c`: 태스크 heartbeat, HAL TIM6 IRQ, tick 및 heap/stack 진단.
- `bsp_fault.c`: 오류 레지스터를 남기고 SWD 분석을 기다리는 실패 처리.
- `Linker/Noodoe_APP.ld`: FLASH `0x08010000/448K`, RAM `0x20000000/192K`, NOLOAD 진단. 미구현 CCM 초기화를 사용하는 배치는 거부한다.
- Core 수동 변경은 USER CODE 블록 안의 연결이다. App_Logic은 사용·include·빌드하지 않는다.
- `tools/check_project.ps1`: 두 구성의 링크/include/USER CODE/KeepUserCode 검사.
- `tools/validate_image.py`: ELF 파일의 실제 load 주소·벡터·strong reset·초기 CPSID i·NOLOAD와 startup 초기화 범위 검사, BIN/manifest 생성.
- `tools/build.ps1`: 실제 CubeIDE managed build와 ELF 검사. 하드웨어를 조작하지 않는다.
- `tools/bringup.ps1` / `bringup.py`: 빌드, 새 A/B 전체백업, APP-only 기록, byte-for-byte 읽기, 하위64KiB 비교, reset과 진단 검사를 자동 수행한다.

프로젝트 폴더에서:

```powershell
.\tools\bringup.ps1 -Configuration Debug
```

기본 실행은 장치 APP 기록과 reset을 포함한다. 재기록 없는 재검사는 `-TestOnly -SkipBuild`를 사용한다. 다른 ST-LINK는 `-Serial`로 지정한다. 실패 시 자동 전체 erase/옵션 변경/무조건 원복을 하지 않으며 로그와 상태를 남기고 중단한다.

## 핵심 증거 파일

- `debug-test-02/summary.json`, `cycle-*-a.json`, `cycle-*-b.json`: Debug 3회 실행 시험.
- `program-normal.log`: 첫 성공한 Debug 기록/검증.
- `release-run-03/summary.json`: 빌드부터 Release 기록·3회 실행 검사까지 한 번에 성공한 자동 실행.
- `release-run-03/before-full-a.bin`, `before-full-b.bin`: Release 기록 전 동일한 Debug 상태의 전체 백업.
- `release-run-03/program-app.log`, `app-readback.bin`, `preserved-before.bin`, `preserved-after.bin`: 기록·검증·BL 보존 근거.
- `image-validator-review/tests.json`, `startup-storage-tests.json`: 잘못된 벡터/주소/startup/storage 등 10개 거부 시험.
- `final-debug-build.log`, `program-final-debug.log`: 마지막 개발용 Debug 재빌드 및 기록.
- `../bringup-runs/2026-09-12-034131-507-Debug/`: 최종 Debug의 wrapper 실행 검사.

## 확인 범위

확인한 순정 BL의 privileged Thread 진입을 처리한다. 임의 Handler mode/비특권 진입, 실행 중 WWDG나 외부 watchdog의 모든 경우를 복구하는 범용 startup은 아니다. HAL/CMSIS/FreeRTOS 원본은 바꾸지 않았다.

현재 클록은 브링업용 HSI 16 MHz다. 화면/SPI/외부 NOR/SDRAM/UART/BT/사용자 업데이트 프로토콜의 작동은 이번 성공 판정에 포함하지 않는다. FLASH BL은 보존했지만 보드의 모든 저장 장치를 백업·복원한 작업도 아니다.

현재 변경 뒤 실제 GUI Generate Code 재실행 및 물리 전원 차단/재인가 시험은 하지 않았다. 재생성 보존을 위해 별도 startup/링커와 USER CODE를 사용했고 자동 검사로 손실을 감지한다. 이를 실제 GUI 재생성 시험을 완료했다는 뜻으로 해석하지 않는다.
