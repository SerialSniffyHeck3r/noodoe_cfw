# Noodoe LCDTest bring-up — 2026-09-12

**목표 달성:** 실제 누도에 사각 창·색깔 선을 표시했고, 백라이트를 25% 밝기로 켰다가 끄는 동작을 0.5초 간격으로 반복한다. 사용자가 “창과 선이 보이고 깜빡임도 정상”이라고 확인했다. 소프트웨어의 상태값만 보고 육안 성공을 추정한 것이 아니다.

## 현재 동작

main.c USER CODE에 있는 실제 strong task의 본문은 다음 한 줄이다.

```c
void StartDefaultTask(void *argument __attribute__((unused)))
{
  LCDTest();
}
```

생성 freertos.c의 defaultTask는 weak fallback이다. IOC를 `As weak`로 설정하고 USER CODE 안의 weak 선언으로 이번 생성본에도 동일한 링크 정책을 적용했다. 실제 GCC/ELF에서 strong task가 선택되는 것을 확인했다. 생성기 소유 코드는 USER CODE 밖에서 수정하지 않았다.

`LCDTest()`는 한 번 초기화한 뒤 계속 실행된다. 후속 개발에서도 시험 파일은 삭제하지 않고 이 호출만 교체한다. 테스트를 끄는 경우 현재 LCD 전용 preflight/task 검사 정책도 함께 변경해야 한다.

화면은 480×480 EVE 내부 RAM_DL의 **37개 raw display-list word**로 만든다. 짙은 배경, 제목 띠, 사각 테두리, 교차선과 가로선이다. 상위 그래픽 프레임워크, MCU framebuffer, 이미지·폰트·command FIFO renderer를 추가하지 않았다. SDRAM/NOR/BT/USB/RTC 초기화도 이번 시험에서 호출하지 않는다.

백라이트는 TIM5 CH4의 500Hz PWM이다. PSC1679 / ARR99 / CCR4=25이므로 ON 구간의 duty가 25%다. ON500ms → CCR4=0으로 OFF500ms를 반복한다. 광학 밝기 자체를 광도계로 25% 측정했다는 뜻은 아니다.

## 코드 분리

프로젝트 기준 `Drivers/BSP/` 아래에 둔다. 각 함수와 전송·GPIO·오류 처리 단위에 호출 조건, 값의 단위, 부작용과 실패 처리를 주석으로 설명했다.

| 파일 | 책임 |
|---|---|
| `src/bsp_lcd_test.c`, `inc/bsp_lcd_test.h` | 보존할 LCDTest, 초기화 순서, 25%/OFF 반복, 최초 실패 유지 및 진단 |
| `src/bsp_eve.c`, `inc/bsp_eve.h` | SPI1 8bit packet, PDN, 순정 EVE timing, raw list 기록·swap, ID/frame 읽기 |
| `src/bsp_lcd_panel.c`, `inc/bsp_lcd_panel.h` | PC13/PI11 reset/enable, SPI4 16bit legacy 프레이밍, LCD 상태 응답 확인 |
| `src/bsp_backlight.c`, `inc/bsp_backlight.h` | PI8/PC8 전원 순서, 생성 TIM5 초기화, PWM duty와 실패 시 off |
| `src/bsp_bringup.c` | 기존 HAL/RTOS 진단 수집을 1회 함수로 분리하여 LCDTest에서도 재사용 |

순서는 backlight 초기화(duty0) → panel reset/disable → EVE 초기화 → panel reset/명령/상태 검증 → 시험 list 표시 → backlight25%/OFF 반복이다. 각 드라이버는 소유한 핀만 설정한다. BL이 남긴 본체 전원 유지 GPIO를 전체 `MX_GPIO_Init()`으로 덮지 않는다.

패널은 `0x1100`을 SPI에 그대로 보내지 않는다. 순정 legacy framing에 맞춰 `0x2011`, `0x0000`, `0x4000`의 개별 PE4 프레임으로 보낸다. 상태 읽기도 같은 주소/operation 인코딩을 사용하고 `0xA5A5` dummy 전송으로 응답을 얻는다. 자세한 주소 근거는 [panel-backlight.md](panel-backlight.md)에 있다.

EVE의 공개 SPI/레지스터 자료와 오픈소스 구현을 대조했다. 순정 하드웨어의 타이밍·GPIO는 직접 확보한 V5.16에서 가져왔다. 공개 라이브러리의 소스나 일반 그래픽 프레임워크를 vendor하지 않았다. [참고 소스·라이선스·순정 주소](eve-reference.md)에 확인한 공식 Bridgetek 자료와 공개 GitHub commit을 기록했다.

## 빌드와 재생성

사용자의 04:26 GUI 재생성 결과를 검토했다. Core USER CODE와 BSP 소스는 보존됐지만 Debug의 링커가 기본 파일로 돌아갔다. `tools/sync_project.ps1`로 두 구성의 프로젝트 소유 링커/BSP 경로를 복원하며 `tools/build.ps1`가 이를 자동 실행하도록 수정했다. `.cproject`를 손상시킨 임시 fixture 복구와 반복 실행 멱등성도 확인했다.

- Debug/Release 모두 오류 0, 경고 0으로 빌드 완료.
- APP 벡터 `0x08010000`, MSP `0x20030000`, strong Reset_Handler와 strong StartDefaultTask, NOLOAD 진단 영역 검사 통과.
- 실제 장치에 기록한 것은 **Debug 43,832 bytes**, SHA256 `6b7faee2cae2817001ea615317de6b84712fa73e25d47a6ec548028bc567fde7`.
- Release는 **25,416 bytes**, SHA256 `85f7a74bf138584ca0b0e90f956803a8a8a1ed8c77e953a4098dc69101c3432b`. 빌드/ELF 검증만 수행했으며 실물에는 기록하지 않았다.

새 task weak 정책 이후의 별도 GUI 재생성 시험까지 수행한 것은 아니다. [재생성 검토](regeneration-review.md)에 이번에 실제 확인한 범위와 template/GCC 검증을 구분했다. FMC GPIO speed가 Cube에서 HIGH→VERY_HIGH로 생성되는 차이는 기록했으며, 이번 시험은 FMC를 호출하지 않으므로 다음 SDRAM 단계에서 별도 처리한다.

## 실제 장치 시험

실행 명령:

```powershell
.\tools\bringup.ps1 -Configuration Debug -LCDTest -SkipBuild -Cycles 2
```

시험 폴더: [2026-09-12-044001-209-Debug](../bringup-runs/2026-09-12-044001-209-Debug/summary.json). ST-LINK `[redacted ST-LINK serial]`, SWD100kHz. 최초 전체 FLASH 512KiB를 두 번 읽어 일치시킨 뒤, 검증된 APP BIN만 기록했다. APP readback 일치, 하위64KiB 원본 BL/config 보존도 확인했다. mass erase나 option-byte 변경은 수행하지 않았다.

| 관측 | 결과 |
|---|---|
| MCU / runtime | STM32F429IE 계열 512KiB, HCLK168MHz, APP VTOR08010000 |
| 순정 BL 인계 | software reset 2회 모두 SRAM VTOR 경유·strong APP reset·RTOS 실행 확인 |
| EVE ID / CPU reset | REG_ID=0x7C, REG_CPURESET=0 |
| EVE chip ID raw | `0x00011208` (subtype 해석 없이 원값 보존) |
| EVE 주파수 / pixel divider | REG_FREQUENCY=60,000,000, PCLK=3. REG_FREQUENCY는 명목/설정값이며 독립 주파수 실측이 아님 |
| 표시 list | 37 words 기록, 첫/끝 readback, DLSWAP=0 확인 |
| LCD 응답 | power-mode=0x009C, ready1, 초기화1회 |
| 백라이트 | TIM5 CEN/CC4E=1, PWM1 active HIGH, PSC1679/ARR99, CCR4=0 및25 각각 관측 |
| 백라이트 GPIO | PI0 AF2, PI8 HIGH, PC8 LOW |
| 오류 | LCD/EVE/panel/backlight result0, fault record0 |
| 태스크 메모리 | 최소 free heap61,312 bytes, 관측 stack free923 words(3,692bytes) |

각 reset 후 A→B 샘플:

| Reset | EVE frames | Blink transitions | 실제 CCR4 |
|---|---|---|---|
| 1 | 347 → 540 | 10 → 15 | 0 → 25 |
| 2 | 348 → 541 | 10 → 15 | 0 → 25 |

HAL/kernel tick과 태스크 heartbeat도 독립적으로 증가했다. SWD halt 중에는 짧은 표시/PWM 간격이 흔들릴 수 있으므로 실물 육안 확인은 시험 종료 후 실행 상태에서 받았다. 마지막에 debug watchdog-freeze 설정을 이전 값으로 복원하고 코어를 실행 상태로 두었다.

**사용자 육안 확인:** “창과 선이 보이고 깜빡임도 정상.” 이번 목표는 이 확인과 위 자동 검증을 합쳐 완료로 기록한다. 물리 전원 차단/재인가, 장시간 내구, Release 실물 실행은 이번 결과에 포함하지 않는다.
