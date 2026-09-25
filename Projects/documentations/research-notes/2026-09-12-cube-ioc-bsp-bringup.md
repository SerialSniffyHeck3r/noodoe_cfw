# Noodoe: CubeIDE 1.18.1 / IOC / FreeRTOS로 시작하는 BSP 재구현

> 후속 상태: 이 문서는 최초 설정 지침이다. 이후 사용자가 GUI에서 코드 생성했고, 현재는 `Drivers/BSP`와 `App_Logic` 구조를 사용한다. 커스텀 source/include 경로는 재생성 후에도 유지됐으며 Debug/Release 빌드가 통과했다. 현재 프로젝트 상태는 [프로젝트 README](../STM32CubeProjects/FuckNudo_Noodoe_CFW_Project/README.md), 작업 규칙은 [AGENTS.md](../STM32CubeProjects/FuckNudo_Noodoe_CFW_Project/AGENTS.md)를 우선한다.

2026-09-12. 사용자가 Cube GUI와 IOC를 직접 편집한다. 이 문서는 설정 지침이며 프로젝트·IOC·펌웨어를 생성하거나 장치에 접근하지 않았다. 별도 OpenNoodoe 프로젝트는 대상에서 제외했다.

## 기준과 폴더

현재 연구 자료는 `<workspace>\Reversing`에 있다. 사용자가 생성한 프로젝트와 IOC를 읽기 전용으로 확인했다. 선택 MCU는 STM32F429IET6, 패키지는 STM32Cube FW_F4 V1.28.3, SYSCLK는 16MHz다. SYS/RTOS/peripheral은 아직 등록되지 않았고 프로젝트에는 `.project`와 `.ioc`가 있다.

실제 프로젝트 위치:

```text
<workspace>\Reversing\STM32CubeProjects\FuckNudo_Noodoe_CFW_Project
```

순정에 맞출 것은 핀/AF, 버스 모드, 실제 통신 속도, GPIO 극성과 전원 순서, 외부 칩 초기화, 저장소 배치, BL 인계 계약이다. 원본 BSP 소스/IOC를 확보한 것은 아니므로 이 동작을 재구현한다. 신규 RTOS wrapper, 폴더/함수 이름과 상위 구조는 별도로 설계할 수 있다. 현재 미확정인 클록/일부 제어 핀을 추정값으로 채운 상태를 순정과 동일하다고 부르지 않는다.

## 1. IDE에서 생성

CubeIDE 1.18.1은 CubeMX 6.14.1과 정렬된 버전이며 내장 IOC 편집기를 사용한다. [ST 릴리스 안내](https://community.st.com/stm32cubeide-mcus-28/stm32cubeide-1-18-1-released-150094).

1. `File → New → STM32 Project`에서 MCU Selector를 연다.
2. 실물 전체 마킹을 우선한다. 현재 512 KiB 관측과 LQFP176 가정에 맞는 후보는 **STM32F429IETx / STM32F429IET6**다. 핀 수는 대략적인 관측이므로 suffix 확정으로 취급하지 않는다. [ST STM32F429IE](https://www.st.com/en/microcontrollers-microprocessors/stm32f429ie.html).
3. 현재 `FuckNudo_Noodoe_CFW_Project`의 IOC가 이미 생성되어 있으므로 그대로 진행한다. 신규 생성 시에는 언어 C, STM32 프로젝트 유형을 선택한다. Empty 프로젝트는 IOC 생성 경로가 아니다.
4. 현재 선택한 STM32CubeF4 V1.28.3을 기록하고 초기 개발 중에는 고정한다. IDE 버전만으로 HAL/CMSIS 패키지 버전을 단정하지 않는다.
5. 생성된 `.ioc`를 핀·클록·주변장치 설정의 기준 파일로 관리한다. 실제 Cube 프로젝트가 생성된 뒤 custom source와 linker/startup을 추가한다.

Noodoe용 Board Selector 항목은 확보하지 않았다. Discovery 보드를 선택하면 그 평가보드의 핀·LCD·메모리 구성이 유입된다. 사용자 보드는 MCU Selector에서 출발한다.

## 2. 최초 IOC 구성

| 화면/항목 | 첫 설정 | 의미 |
|---|---|---|
| SYS / Debug | Serial Wire | PA13/PA14 유지. PB3는 순정 SPI1 SCK이므로 SWO/Trace 핀으로 점유하지 않음 |
| SYS / Timebase Source | TIM6 | HAL 시간 기준. TIM6 사용이 추가로 확인되면 TIM7 검토. 별도 PWM/독립 타이머 용도로 중복 사용하지 않음 |
| RCC / clock tree | HSI 16 MHz, SYSCLK=HSI, AHB/APB 분주 1 | 최초 scheduler 검사값. **순정 최종 클록을 재현한 값은 아님** |
| HSE/LSE, PLL | 첫 단계에서는 사용 안 함 | 순정 HSE/PLL 수치가 아직 확정되지 않음 |
| FreeRTOS | CubeF4 제공 FreeRTOS, CMSIS_V2 | 신규 상위 코드의 API 선택. 원본은 CMSIS v1 흔적이 있으나 BL ABI는 특정 RTOS API를 요구하지 않음 |
| SYS / NVIC grouping | preemption 4 bit, subpriority 0 bit | RTOS IRQ 우선순위 기준 |
| 사용하지 않는 핀 일괄 Analog 설정 | 해제 | BL이 설정한 전원 관련 핀을 무차별 변경하지 않음 |
| 초기 middleware | RTOS만 | BT/SPP, NOR 파일시스템, EVE 그래픽은 단계별 BSP/서비스 구현으로 추가 |

HSI 상태에서는 고속 HCI의 실제 baud나 순정 SPI/PWM 속도를 맞췄다고 볼 수 없다. 특히 3,686,400 baud를 HSI 16 MHz 환경에 그대로 넣어 원래대로 통신할 것이라고 기대하지 않는다. 주변장치 속도 검증 전에 최종 클록을 확정한다.

## 3. FreeRTOS 시작값

다음은 **신규 프로젝트 제안값**이다. 순정 전체 설정을 그대로 복원했다는 표가 아니다. 기존 분석은 FreeRTOS + CMSIS v1, 1 kHz tick, 약 64 KiB heap, stack overflow check 2를 지지한다.

| 항목 | 제안값 |
|---|---|
| CMSIS API | v2 |
| Tick rate | 1000 Hz, SysTick 사용 |
| Heap implementation | heap_4 |
| Total heap | 65536 bytes, 내부 일반 SRAM에 배치 |
| Preemption | Enabled |
| Tickless idle | Disabled, 전원/웨이크업 검증 후 추가 |
| Stack overflow check | 2 |
| Malloc-failed hook / configASSERT | 켜고 생성 후 오류 기록용 구현 확인 |
| 최초 task | `bringupTask`, `osPriorityNormal`, 실제 stack 4096 bytes |
| MAX_PRIORITIES / optimized selection | CMSIS v2 생성값 유지. v1의 우선순위 개수를 그대로 복사하지 않음 |
| Queue/mutex/software timer 객체 | 실제 사용하는 것부터 추가 |

Cube 화면이 스택을 words로 표시하면 1024 words가 4096 bytes다. CMSIS v2의 생성된 `osThreadAttr_t.stack_size`는 **bytes**다. 결과가 `4096` 또는 `1024 * 4`인지 확인한다. FreeRTOS native task API의 stack depth와 `configMINIMAL_STACK_SIZE`는 words다.

현재 ST CMSIS v2 wrapper는 넓은 우선순위 매핑을 사용하므로 보통 `configMAX_PRIORITIES=56`, `configUSE_PORT_OPTIMISED_TASK_SELECTION=0`을 요구한다. 사용하는 패키지 wrapper가 실제 기준이다.

`configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY=5`, lowest=15, priority bits=4로 잡는 구성이 F429 공식 예제와 일치한다. RTOS API를 호출하는 peripheral ISR은 수치상 5..15를 사용하고 ISR용 API만 호출한다. 숫자 0..4는 더 높은 긴급도이며 RTOS 호출용으로 쓰지 않는다. `configMAX_SYSCALL_INTERRUPT_PRIORITY`의 shifted 값은 이 경우 `0x50`이다. SysTick/PendSV는 생성된 kernel용 설정을 유지한다. TIM6 HAL tick callback에는 RTOS 작업을 섞지 않고, ISR에서 지연 대기를 하지 않는다.

64 KiB RTOS heap은 초기 예산이다. C library heap, main/IRQ stack, 정적 버퍼는 별도 공간이며 이미지 framebuffer를 이 예산에 무작정 넣지 않는다. 초기에는 CCM이나 외부 SDRAM에 DMA buffer/RTOS heap을 배치하지 않는다.

## 4. IOC 재생성에 견디는 3개 계층

```text
FuckNudo_Noodoe_CFW_Project/
  FuckNudo_Noodoe_CFW_Project.ioc
  Core/                 Cube 생성 진입 코드, IRQ, RTOS glue
  Drivers/              Cube HAL / CMSIS
  Middlewares/          FreeRTOS 등
  BSP/
    Inc/  Src/           보드/장치 드라이버와 BL 인계
  Logic/
    Inc/  Src/           계기판 프로토콜, 상태, 업데이트 정책, 서비스
  Graphics/
    Inc/  Src/           화면 모델, 배치, EVE 렌더 명령 구성
```

- BSP: 전원/IGN, 실제 UART/SPI/I2C/DMA, NOR, BT HCI transport와 controller 초기화, EVE transport, LCD 초기화, backlight, 부트 ABI.
- Logic: 차량 프레임 의미, 상태 모델, SPP 명령 처리·파일 전송 정책, 업데이트 요청, 사용자 설정. BT host stack은 middleware/service로 두고 BSP의 HCI transport를 이용한다.
- Graphics: Logic의 상태 snapshot/event를 받아 표시한다. GPIO/HAL handle을 직접 소유하지 않고 BSP의 display API를 쓴다.

이 세 계층이 RTOS task 세 개와 일대일 대응해야 하는 것은 아니다. 먼저 bringupTask 하나에서 순서대로 BSP를 검증하고, 이후 bus 소유권과 처리 지연을 기준으로 task를 나눈다. 그림 갱신 중 NOR 접근 등 같은 버스를 여러 task가 공유하면 BSP에서 직렬화한다.

IOC가 BSP 전체를 생성해 주지는 않는다. GPIO/클록/주변장치 init을 생성하고, 그 위에 외부 칩 명령·전원 순서·버스 소유권·오류 처리를 직접 구현한다. UART 설정만으로 Bluetooth SPP host stack이 생기지도 않는다.

## 5. Project Manager 설정

- Code Generator에서 주변장치별 `.c/.h` 분리와 USER CODE 보존을 켠다.
- 필요한 library 파일을 프로젝트로 복사해 패키지 버전 의존성을 기록한다.
- 직접 작성한 BSP/Logic/Graphics 파일은 별도 폴더에서 유지한다. 생성 파일의 USER CODE에는 연결 호출 정도를 둔다.
- Advanced Settings의 Driver Selector는 처음 HAL로 통일한다. 이 선택 자체가 순정 peripheral 설정과의 동일성을 보장하는 것은 아니다.
- 특정 버스의 자동 init 호출을 제외하고 함수 생성은 유지할 수 있다. 그 함수는 BSP가 순정 순서에 맞춰 호출한다. 외부에서 호출해야 하면 static visibility를 해제한다.
- 첫 GPIO/시스템/RTOS 초기화까지 무조건 모든 자동 호출을 제거하지 않는다. 하나의 명확한 초기화 경로를 유지한다.
- SPI/USART init 호출을 막아도 **MX_GPIO_Init의 CS/reset 출력 설정은 별도로 실행될 수 있다.** 미확정 제어 핀을 등록하기 전에 이 범위를 확인한다.
- DMA가 필요한 peripheral init보다 DMA clock 준비가 앞서는지 생성된 호출 순서를 확인한다.

설정 위치와 생성 범위: [ST CubeMX 사용자 설명서](https://dev.st.com/stm32cube-docs/stm32cubemx/6.18.1/en/docs/markup/CubeMX_UserManual/chapters/04_4_stm32cubemx_user_interface.html). 이 웹 설명서는 6.18.1이며 사용자 IDE의 6.14.1과 일부 UI 문구가 다를 수 있다.

## 6. 순정 BSP로 맞춰 갈 peripheral 지도

다음은 실제 V5.16 분석에서 얻은 MCU 핀/설정이다. 커넥터 번호와 동일시하지 않는다. 최신 수신 분석으로 정정된 내용은 과거 hardware JSON/CSV보다 우선한다. **초기 IOC에서 이 모두를 동시에 작동시키는 목록은 아니다.** 확인된 pin/AF를 지정하고 Signal Pinning으로 자동 재배치를 막으며 subsystem별로 활성화한다.

| 기능 | IOC peripheral / MCU 핀 | 설정과 아직 필요한 확인 |
|---|---|---|
| 계기판 UART | UART5: PC12 TX, PD2 RX, AF8 | 115200, 8N1, no flow, oversampling16. **RXNE byte IRQ 경로 확인**. DMA 설정 존재를 RX DMA ring 사용으로 해석하지 않음 |
| BT HCI | USART1: PA9 TX, PA10 RX, PA11 CTS, PA12 RTS, AF7 | 8N1, RTS/CTS. DMA2 RX S5 Ch4 / TX S7 Ch4. **시점별 baud 변경 존재**; 3,686,400 고정으로 복제하지 않음 |
| EVE 표시 엔진 | SPI1: PB3 SCK, PA6 MISO, PB5 MOSI, AF5; PA4 CS | 9월12일 추가 확인: master, 2-line, 8-bit, mode0, software NSS, MSB first. CS LOW transaction. 초기 /8 → 후반 /4 호출 경로. PB1은 EVE reset/PDN 후보이며 지연/극성은 BSP에서 구현. SWO와 PB3 충돌 주의 |
| LCD 제어 | SPI4: PE2 SCK, PE5 MISO, PE6 MOSI, AF5 | master, 16-bit, mode0, /16 근거. PE4 CS/control 후보와 reset/control polarity는 미확정 |
| 리소스 NOR | SPI5: PF7 SCK, PF8 MISO, PF9 MOSI AF5; PF6 CS | master, 8-bit, mode0, 순정 /2. software CS, idle HIGH. 초기 저속 RDID 검증 후 순정 속도로 맞춤. 9월12일 TX DMA2 S4 Ch2 / RX S3 Ch2 확인; 초기화의 halfword alignment와 실제 사용 경로는 추가 대조 대상이므로 최초에는 polling |
| 조도 | I2C3: PH7 SCL, PC9 SDA, AF4 OD | 400 kHz, 외부 장치의 7-bit address 0x45. Cube의 MCU Own Address에 0x45를 넣는 뜻이 아님 |
| MFi 인증칩 | I2C1: PB6 SCL, PB7 SDA, AF4 OD | 100 kHz, 외부 address0x11; PH13 reset 후보. 초기 안드로이드용 통신 bring-up에는 우선순위 낮음 |
| IGN 상태 | PG13, no pull | MCU LOW=ON / HIGH=OFF. 양에지 EXTI13, EXTI15_10 handler. 순정 TIM2 1kHz 처리에서 약11ms debounce |
| 전원 출력 | PD13, PG14 | BL 초기 PD13 HIGH / PG14 LOW 확인. shutdown 반대. 외부 회로의 확정 신호명은 아직 없음 |
| revision별 제어 | PI9 등 | revision>=3 분기 존재. 모든 donor에 고정한 GPIO 출력으로 적용하지 않음 |
| backlight 경로 | TIM5 CH4 / PI0 AF2 | 9월12일 MCU 설정 추가 확인: PWM1, polarity HIGH, ARR99, counter50kHz → 500Hz. GPIO AF push-pull/pull-up. 최초 출력은 외부 driver 동작을 대조한 뒤 시작; CCR/밝기 변환은 BSP에서 구현 |
| USB | USB_OTG_HS + embedded FS PHY | 순정 controller 종류 확인. 실제 신호 배선과 clock 조건 확인 후 IOC 활성화; OTG_FS와 구별 |
| FMC/SDRAM | 보류 | 메모리 모델/geometry/timing이 확정되지 않음 |

FT81x가 그래픽 출력을 맡는 현재 증거에서 LTDC/DMA2D/TouchGFX 평가보드 구성을 자동으로 선택할 근거는 없다. EVE transport/LCD 제어는 BSP, 표시할 화면과 display list 구성은 Graphics에 둔다.

## 7. 순정과 같은 클록을 만들기 위한 다음 읽기

HSI 16MHz 설정은 첫 scheduler 검사에 쓴다. 원래 BSP에 맞출 최종 IOC에는 순정 APP 실행 중 RCC 값을 읽고 HSE 주파수/PLL 계수/분주를 확정해 입력한다.

| 레지스터 | 주소 |
|---|---|
| RCC_CR | 0x40023800 |
| RCC_PLLCFGR | 0x40023804 |
| RCC_CFGR | 0x40023808 |
| FLASH_ACR | 0x40023C00 |
| PWR_CR | 0x40007000 |
| PWR_CSR | 0x40007004 |

HSE 소스일 때 이들 값만으로 외부 발진 주파수를 자동으로 알 수 있는 것은 아니다. 발진자 마킹/기존 코드 설정/실측을 대조한다. 이 문서 작성 중 위 주소를 장치에서 읽지 않았다.

## 8. IOC 외에 필요한 APP startup/링커

| 항목 | 값/계약 |
|---|---|
| APP FLASH | ORIGIN 0x08010000, LENGTH 448K |
| 일반 SRAM | ORIGIN 0x20000000, LENGTH 192K |
| VTOR | 0x08010000, .data/.bss 초기화보다 먼저 적용 |
| 초기 MSP | 실제 일반 SRAM의 stack; 순정 MSP 숫자를 그대로 복사할 필요 없음 |
| 보호할 내부 영역 | BL/메타/기기 데이터 0x08000000..0x0800FFFF |

BL은 SysTick을 끄고 MSP를 바꿔 APP로 분기한다. NVIC/peripheral/clock 전체를 reset 상태로 만들지 않는다. 신규 startup에서 IRQ/DMA 상태를 정리하고, 자기 VTOR와 clock/runtime을 준비해야 한다. HAL tick을 TIM6로 선택해도 이 인계 문제가 자동 해결되지는 않는다.

생성본의 `USER_VECT_TAB_ADDRESS`/`VECT_TAB_OFFSET=0x10000` 또는 동등한 실제 SystemInit 코드를 확인한다. 옵션 매크로 방식은 설치된 CMSIS 파일에 따라 달라질 수 있다. Cube 재생성 후 linker, startup과 ELF `.isr_vector` 위치를 다시 확인할 수 있게 관리한다. USER CODE 보존 옵션은 startup/linker의 모든 수정을 자동 보존하는 약속이 아니다.

## 9. 첫 실행과 이후 순서

1. IOC 생성, build. BSP startup 인계 수정과 APP 배치 확인.
2. BL 경유 Reset_Handler/main 도달을 확인하고 bringupTask에서 counter 증가와 osDelay 재개를 확인한다. 디버거가 APP로 직접 PC를 설정한 실행만으로 BL 경유 부팅을 검증했다고 보지 않는다.
3. 정상 reset 후 반복 부팅, heap/stack 사용량 관측, 원본 복원 경로 확인.
4. 원래 clock으로 맞춘 뒤 전원/IGN, UART5, NOR ID, 조도 ID를 단계별 검증한다.
5. EVE/LCD/backlight의 실제 출력, 이후 BT controller/HCI/SPP를 검증한다.
6. BSP 동작이 검증된 인터페이스 위에서 Logic과 Graphics를 확장한다.

## 근거와 우선순위

- [V5.16 peripheral 설정서](ak550-sr15-mcu-peripheral-settings.md): 초기 pin/mode 지도; 일부 미확정/이전 표현 존재.
- [9월9일 UART 정정](2026-09-09-ak550-firmware-uart-boot-review.md): UART5 RXNE, USART1 baud 재설정과 DMA Ch4.
- [PG13와 TIM2 실물 APP 코드](../analysis/2026-09-10-sleep-entry/input/README.md).
- [실제 BL 전원/인계](../analysis/2026-09-11-update-failure-contract/boot-failure/README.md).
- [APP 부트 계약](../analysis/2026-09-11-update-failure-contract/mcu-app/README.md).
- [복구 범위](2026-09-11-noodoe-bad-firmware-and-recovery.md).

공식 RTOS 및 생성 코드 근거:

- [ST CMSIS v2 wrapper 설정 조건](https://github.com/STMicroelectronics/stm32-mw-freertos/blob/master/Source/CMSIS_RTOS_V2/freertos_os2.h)
- [ST CMSIS v2 task stack 변환](https://github.com/STMicroelectronics/stm32-mw-freertos/blob/master/Source/CMSIS_RTOS_V2/cmsis_os2.c)
- [ST F429 FreeRTOS 예제 설정](https://github.com/STMicroelectronics/STM32CubeF4/blob/master/Projects/STM32F429I-Discovery/Applications/FreeRTOS/FreeRTOS_ThreadCreation/Inc/FreeRTOSConfig.h): IRQ 숫자 규칙 참고용이며 평가보드의 핀/LCD 설정은 이식하지 않음.
- [FreeRTOS IRQ 제약](https://freertos.org/Why-FreeRTOS/FAQs/Troubleshooting)
- [FreeRTOS heap_4](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/portable/MemMang/heap_4.c)

사용 패키지가 실제 기준이며 설계 시작값의 실물 동작은 아직 검증하지 않았다. 9월12일 SPI1/TIM5/SPI5 추가 판독은 [교차검증 메모와 명령어 발췌](../analysis/2026-09-12-cube-bsp-crosscheck/README.md)에 있다.

현재 IOC에서 `KeepUserCode=true`, `FreePins=false`는 이미 원하는 설정이다. `CoupleFile=false`는 주변장치별 파일 분리를 켜면 바뀔 항목이다. `RCC.HSE_VALUE=25000000`은 생성 기본 파라미터로 보이며 순정 보드 HSE 실측 근거가 아니다. 현재 SYSCLK=HSI 16MHz라는 설정과 구분한다.
