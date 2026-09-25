# BSP 초기 실행 확인

현재 BSP는 순정 resident BL 인계, APP/RTOS 실행, 디스플레이·백라이트와 세 버튼을 다룬다. App_Logic 의존성은 없다. BT 드라이버와 업데이트 수신기는 아직 포함하지 않는다. 상위 버튼/표시 API 사용법과 보존하는 LCDTest는 [BUTTONS.md](BUTTONS.md)를 참고한다.

## 확인한 순정 BL 계약

실제 덤프 A의 복원 RAM `0x200045A0..0x200045DE`는 APP 첫 word에 `(MSP & 0x2FFC0000) == 0x20000000` 검사를 하고 SysTick CTRL/LOAD/VAL을 0으로 만든 뒤 APP MSP를 설정하여 Reset 벡터로 BLX한다. VTOR는 BL SRAM `0x20000000`에 남는다. Reset 주소 범위·Thumb bit·APP CRC·실행 후 건강 검사는 이 jump에 없다.

따라서 새 C 런타임이 `.data/.bss`로 BL SRAM 벡터와 코드를 덮기 **전에** 실행 소유권을 가져와야 한다. 이 구현은 확인한 BL의 privileged Thread 진입을 전제로 한다. 임의 예외 Handler에서 분기한 상태, 비특권 코드의 권한 상승, 활성 WWDG·외부 watchdog의 복구를 제공하는 범용 점프 라이브러리가 아니다.

근거는 `Reversing/analysis/2026-09-11-update-failure-contract/mcu-app/boot-jump-abi.asm.txt`, 같은 폴더의 README 및 `boot-failure/README.md`다. 유효한 pending 설치 요청이 남아 있으면 BL이 다음 reset 때 NOR 이미지를 APP에 다시 복사할 수 있으므로 장치 시험에서 메타 상태도 별도로 확인한다.

## 파일과 함수의 경계

| 구현 | 입력·실행 시점 | 책임 |
|---|---|---|
| `bsp_reset.s / Reset_Handler` | BL의 APP 진입 | IRQ 차단, 원래 CONTROL/BASEPRI/IPSR/VTOR 확보, MSP·자체 VTOR·MPU 정리, 초기 BSP 호출, 표준 C 초기화, main 진입 |
| `bsp_boot.c / BSP_BootEarly` | PRIMASK=1, 유효 MSP, 자체 VTOR, C 초기화 전 | 인계 기록, DMA/NVIC/예외 pending 정리, tick 독립 제한 대기, HSI 전환, 선택적 주변장치 reset. IRQ는 아직 해제하지 않음 |
| `bsp_boot.c / BSP_BootRuntimeReady` | C 초기화 뒤 main USER CODE BEGIN 1 | HSI·VTOR·CPU 상태와 기록 검증, SystemCoreClock 갱신 후 IRQ 해제 |
| `bsp_bringup.c / BSP_BringupMark` | main/RTOS 초기화의 USER CODE | 마지막으로 지난 단계와 clock/VTOR 기록 |
| `bsp_bringup.c / BSP_BringupHalTick` | TIM6 HAL tick callback | 실제 HAL timer IRQ 횟수 증가 |
| `bsp_bringup.c / BSP_BringupTask` | 생성 defaultTask에서 호출 | osDelay 복귀 후 heartbeat, HAL/kernel tick, heap 및 stack 여유 기록 |
| `bsp_fault.c / BSP_FaultRecord` | Core fault·Error_Handler·RTOS 실패 hook | 레지스터와 오류 코드 기록, 정상 실행으로 복귀하지 않고 대기 |
| `bsp_fault.c / BSP_FaultClear` | main 첫 진입 | 이전 실행의 fault 유효 표시 해제 |

강한 Reset_Handler는 Cube가 만든 weak 정의보다 우선한다. 생성 startup 자체를 수정하지 않으며 ELF의 실제 벡터가 BSP 구현으로 연결되는지 검증한다. `Linker/Noodoe_APP.ld`는 `.noinit.bsp_boot`와 `.noinit.bsp_fault`를 `.bss` 밖의 NOLOAD로 배치한다. `mpu_ctrl_before_c_cleanup`은 assembly에서 MPU를 끈 **이후**, C 정리 직전에 관측한 값이다. BL의 원래 MPU 값을 기록한 필드가 아니다.

## 보존하는 보드 상태

순정 BL 초기 출력은 PD13 HIGH / PG14 LOW이며 PI9는 보드 revision에 따라 다르다. 이 BSP는 GPIO 방향·출력 및 PWR/RTC/LSI/HSE·RTCPRE를 보존한다. 전체 AHB reset을 수행하는 HAL_DeInit은 사용하지 않는다. 타이머/SPI/I2C/UART/USB 등의 선택적 reset으로 AF 출력은 정지할 수 있으며, 해당 장치는 추후 드라이버에서 다시 초기화해야 한다.

초기 클록은 HSI 16 MHz 기준이다. PLL을 SYSCLK에서 먼저 떼고 ready 상태를 확인한 뒤 끈다. FLASH latency와 전압을 고속 PLL 상태에서 낮추지 않는다. 대기 한도는 반복 횟수이며 밀리초 보장이 아니다. 초기 함수는 HAL_GetTick, libc 또는 초기화된 전역에 의존하면 안 된다.

IWDG에는 reload key `0xAAAA`만 쓰고 start key나 옵션 바이트를 쓰지 않는다. 장치 관측의 `WDG_SW=1`은 software-start 설정이며 당시 IWDG가 실행 중인지까지 증명하지 않는다. 이번 디버그 준비 로그에는 `DBGMCU_APB1_FZ(0xE0042008)=0x1800` 기록이 있으며 WWDG/IWDG의 **CPU halt 중 freeze** 설정에 해당한다. freeze는 실행 중 watchdog 정지나 펌웨어의 WWDG 복구를 뜻하지 않는다. 이 레지스터 설정은 reset 등으로 달라질 수 있으므로 각 디버그 세션에서 확인한다. 근거: `Reversing/analysis/2026-09-12-bringup/initial-target.log`, `backup-halted-100.log`.

## 진단 읽기

| 심볼 | 의미 |
|---|---|
| `g_bsp_boot.status` | `0x10`: 초기 BSP 반환 준비, `0x20`: main baseline 확인 완료. `0x800000xx`: 초기 진입/DMA/클록/런타임 검사 실패 |
| `g_bsp_boot.wait_*` | 대기 실패의 레지스터 주소·값·마스크·기대값. 성공 시 0으로 남을 수 있음 |
| `g_bsp_bringup.stage` | 1 main, 2 HAL init 뒤, 3 clock 뒤, 4 GPIO 뒤, 5 task 생성 뒤, 6 task 진입 |
| `g_bsp_bringup.heartbeat` | osDelay(100 tick)에서 복귀한 횟수. 서로 다른 시점의 증가로 scheduler 진척 확인 |
| `g_bsp_bringup.tim6_interrupts` | HAL TIM6 tick IRQ 횟수. kernel tick과 별개의 관측값 |
| `g_bsp_fault.magic/code` | `0x4641554C`이면 기록 완료된 fault. 코드·CFSR/HFSR 등과 함께 해석 |

fault의 MSP/PSP는 기록 함수 안에서 읽은 값이다. 예외 진입 당시 stacked PC/LR 전체를 수집하는 crash dump가 아니다. stack 손상 등으로 C fault 기록 함수에 도착하지 못하면 레코드가 없을 수 있으며, magic이 없다는 것만으로 fault 부재를 보장하지 않는다.

2026-09-12에는 Debug와 Release 모두 실제 BL→APP 진입, HAL/RTOS 진척, 소프트웨어 reset 3회를 통과했다. 결과는 `Reversing/analysis/2026-09-12-bringup/README.md`에 정리했다. 현재 변경 뒤 GUI 재생성 및 물리 전원 차단 시험은 아직 하지 않았다. 정적 확인·빌드·장치 실행·재생성 보존을 구분하여 기록한다.
