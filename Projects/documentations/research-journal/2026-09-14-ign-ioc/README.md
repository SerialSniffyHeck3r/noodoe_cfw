# IGN 절전용 IOC 준비 — 재생성 대기

현재 단계는 IOC 수정과 정적 확인까지다. GUI 재생성, 새 펌웨어 빌드·설치, 절전 동작 검증은 수행하지 않았다.

## 수정한 설정

- RTC: Internal WakeUp 가상 기능 추가. 물리 출력 핀은 추가하지 않는다.
- RTC_WKUP_IRQn: 인터럽트와 HAL handler 생성 활성화, preemption priority 5 / subpriority 0. 기존 EXTI15_10 및 FreeRTOS syscall 우선순위와 호환되는 값이다.
- RTC WakeUpClock: LSE / 16 = 2048 Hz, WakeUpCounter 2047. 생성 초기값은 약 1초이며 사용자 대기 시간 설정이 아니다. 이후 BSP에서 실제 기한·watchdog 제한에 따라 동적으로 재설정한다.
- FreeRTOS configUSE_TICKLESS_IDLE=2: User defined functionality enabled. INCLUDE_vTaskSuspend=1을 명시한다. 설치된 CubeMX의 CMSIS V2 템플릿은 빈 weak vPortSuppressTicksAndSleep()을 생성한다. 재생성만으로 절전이 동작하는 것은 아니다.

## 이미 맞는 설정

- HSE 25 MHz, SYSCLK 168 MHz. RTC는 별도 LSE 32768 Hz, prescaler 127/255.
- IGN PG13: 양쪽 에지 EXTI, 내부 pull 없음. 기존 라벨을 유지해 생성 매크로 사용자 코드를 보존한다.
- RTC Alarm A 및 기존 핀·주변장치 설정, TIM6 HAL timebase, defaultTask 3072 words, RTOS heap 64 KiB.
- 모든 peripheral MX init은 main에서 자동 호출하지 않는 기존 설정을 유지한다.

## 재생성 후 반드시 연결할 부분

1. RTC_WKUP_IRQHandler와 HAL dispatch, FreeRTOSConfig의 tickless=2, weak sleep 함수 생성 여부를 확인한다. generated-before.zip 및 SHA256 목록으로 USER CODE 보존을 비교한다.
2. 기존 sync_project.ps1로 Cube가 되돌리는 APP 링커·커스텀 소스 설정을 복구하고 Debug/Release 빌드를 검증한다. 생성 전 빌드 성공을 새 IOC 검증으로 취급하지 않는다.
3. BSP_Power_Init은 현재 PG13을 plain input으로 바꾸고 EXTI13을 mask한다. 이를 수정하고 공유 EXTI15_10의 다른 버튼 dispatch를 보존해야 실제 IGN wake가 가능하다.
4. BSP_Clock은 순정 RTC에 attach하는 계약을 유지한다. MX_RTC_Init을 호출하면 날짜·시간 및 Alarm A가 초기화되므로 wake-up을 켜기 위해 이 함수를 새로 호출하지 않는다. RTC WUT/IRQ는 별도 BSP API에서 연결하고 Alarm A/B와 공존한다.
5. 사용자 정의 sleep 함수를 별도 BSP 파일에 구현한다. SysTick/TIM6 정지·경과 시간 보정, IWDG 제한, 이벤트·기한 기반 태스크 대기, peripheral/DMA 정리, SDRAM self-refresh, STOP 후 HSE/PLL 복귀를 함께 처리한다. 빈 weak 함수만 있는 상태는 절전 완료가 아니다.
6. BT 유지 단계는 일반 Sleep을 사용하고 BT 종료 후 STOP을 적용한다. 현 H4 통신에는 STOP 중 UART 수신을 보장할 sleep handshake가 구현되어 있지 않다.

## 확인 근거

설치된 STM32CubeIDE 1.18.1의 CubeMX 6.14.1 데이터베이스를 직접 확인했다.

- db/mcu/STM32F429I(E-G)Tx.xml: 이 MCU의 RTC rtc2_v2_3 및 FREERTOS v8.0.0_Cube 정의 선택.
- db/mcu/IP/RTC-rtc2_v2_3_Modes.xml: WakeUp / VS_RTC_WakeUp_intern, WakeUpClock 및 Counter 범위.
- db/mcu/IP/FREERTOS-v8.0.0_Cube_Modes.xml: tickless 2 및 vTaskSuspend 의존성.
- db/templates/freertos_app_c_v10_3_1_cmsis_v2.ftl: 사용자 정의 tickless weak 함수 생성.

before.ioc / after.ioc / ioc.diff / validation.json에 원본, 변경 및 정적 확인 결과를 보관했다. 장치 접근 및 UART 주입 변경은 하지 않았다.
