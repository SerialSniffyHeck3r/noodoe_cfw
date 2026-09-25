# Noodoe CubeIDE 프로젝트

STM32F429IETx / STM32CubeIDE 1.18.1 / STM32CubeF4 1.28.3 / FreeRTOS CMSIS-RTOS v2.

**2026-09-12 후속 상태: LVGL 9.5.0/EVE 포트와 기능별 순회 시험을 추가했다.** 168MHz 생성 clock, SPI1/EVE, SPI4/패널, TIM5/백라이트를 사용한다. main.c USER CODE의 실제 태스크 본문은 `LCDTest();` 한 줄이다. 새 API와 시험 조작법은 [Graphics 문서](Graphics/README.md), 이전 raw 화면 검증은 [LCD bring-up 기록](../../analysis/2026-09-12-lcd-bringup/README.md)을 본다.

초기화 함수는 공개 함수로 생성하지만 호출은 BSP가 필요한 순서로 소유한다. 이번 화면 시험은 FMC/외장 NOR/BT/USB 등 나머지 장치를 초기화하지 않는다. 매핑 자체의 출처는 [전체 매핑 기록](../../analysis/2026-09-12-full-ioc-map/README.md)을 따른다.

`LCDTest`는 보존할 시험이다. 기본은 LVGL 위젯을 EVE 명령으로 렌더링하는 순회 시험이며 백라이트는 **25% 고정**이다. 기존 제목 띠·테두리·교차선·버튼 칸과 **25% ON 500ms / OFF 500ms** 시험은 `LCDTestLegacy()`로 보존했다. MCU 전체 화면 framebuffer는 사용하지 않는다. 추후 사용하지 않을 때는 main의 호출만 교체하고 `bsp_lcd_test.*` 파일은 삭제하지 않는다.

앞 단계에서 확인한 실행 범위는 **순정 BL에서 APP 진입, HAL TIM6 tick, RTOS 태스크의 주기적 실행**이다. 현재 앱 로직은 `App_Logic`에, 통신·저장 서비스는 `Middlewares/Noodoe`에 둔다. [계층 안내](App_Logic/README.md)를 따른다. 화면·UART·BT 업데이트 기능은 아직 이 실행 검증의 성공 조건이 아니다.

## 코드와 메모리 배치

| 위치 | 역할 |
|---|---|
| Core/ | Cube 생성 초기화·IRQ·RTOS. USER CODE 블록에서 BSP를 연결 |
| Drivers/BSP/src/bsp_reset.s | 생성 weak startup을 대신하는 strong Reset_Handler |
| Drivers/BSP/src/bsp_boot.c | C 초기화 전 BL 상태 정리 및 main 진입 검증 |
| Drivers/BSP/src/bsp_bringup.c | HAL tick·RTOS heartbeat·heap/stack 관측 |
| Drivers/BSP/src/bsp_fault.c | fault 원인과 레지스터 기록 후 디버거 대기 |
| Linker/Noodoe_APP.ld | APP 전용 링크와 NOLOAD 진단 영역 |
| Drivers/STM32F4xx_HAL_Driver, Drivers/CMSIS, Middlewares | 생성·패키지 원본. 전용 구현을 섞지 않음 |
| App_Logic/ | UI 상태·제품 실행·설정·명령·오일 사용 시간 |
| Middlewares/Third_Party/LVGL | 버전을 고정한 LVGL 원본과 출처 |
| Graphics/Port | LVGL 설정·EVE 전송·입력·공개 Graphics API |
| Graphics/UI | 기능별 시험 화면과 공개 시험 제어/조회 API |

APP FLASH는 **0x08010000..0x0807FFFF, 448 KiB**다. 하위 64 KiB의 순정 BL·설치 메타·기기 데이터를 보존한다. 생성된 STM32F429IETX_FLASH.ld는 여전히 남아 있지만 현재 APP 빌드에서 선택할 파일은 Linker/Noodoe_APP.ld다. 파일 이름만 보고 판단하지 않고 ELF의 실제 벡터·load 주소를 확인해야 한다.

RAM은 0x20000000의 192 KiB이며 초기 MSP는 0x20030000이다. boot/fault 기록은 .noinit NOLOAD 영역으로 분리한다. 이것은 C 초기화로 지우지 않는다는 뜻이며 전원 차단 후 영구 보존을 보장하지 않는다.

## 빌드

프로젝트 폴더에서 실행한다.

```powershell
.\tools\build.ps1 -Configuration Debug
.\tools\build.ps1 -Configuration Release
```

설치된 CubeIDE managed builder, ARM GCC, make와 별도 headless workspace를 사용한다. 커스텀 include는 ../Drivers/BSP/inc이며 Debug/Release 출력 디렉터리를 기준으로 한다. Drivers 소스 루트가 BSP의 C/assembly를 포함하므로 BSP 루트를 중복 등록하지 않는다. 헤더는 `#include "bsp_boot.h"`처럼 사용한다.

링커의 `Linker Script (-T)`는 두 구성 모두 `../Linker/Noodoe_APP.ld`다. GUI에서 중첩 workspace 매크로가 빈 경로로 해석되던 문제를 피하기 위해 실제 상대 경로를 사용한다. 외부에서 수정한 `.cproject` 설정이 열린 IDE에 반영되지 않으면 프로젝트를 Close Project → Open Project 한 뒤 Project → Clean 후 빌드한다. 생성 makefile의 실패 타깃 정의 자체는 정상이며, ELF 의존성에 그 실패 타깃이 연결돼 있는지가 오류 판단 기준이다.

빌드 스크립트는 `sync_project.ps1`로 재생성 때 덮인 프로젝트 소유 링커/BSP 경로를 복원하고, `check_project.ps1`로 생성 clock·링크·include·USER CODE 연결을 확인한다. 빌드 후 `validate_image.py`로 실제 ELF의 주소와 strong startup/task를 검사한다. 통과하면 Debug 또는 Release 아래에 `app.bin`과 `app.manifest.json`을 생성한다. 빌드 자체는 장치 쓰기·erase·reset을 수행하지 않는다.

## 자동 기록과 실행 검사

ST-LINK를 다른 프로그램에서 해제한 뒤 다음 명령을 실행한다. 현재 프로브의 serial이 기본값이며, 장비를 교체하면 `-Serial`로 정확한 serial을 지정한다.

```powershell
# 빌드 → 이미지 검사 → 원본 A/B 백업 → APP 기록/읽기 비교 → BL 보존 → 리셋 3회
.\tools\bringup.ps1 -Configuration Debug

# LCDTest가 선택된 현재 APP: 위 검사 + EVE/LCD/PWM/프레임 진행 검증
.\tools\bringup.ps1 -Configuration Debug -LCDTest

# 같은 ELF가 이미 기록된 경우, 재기록 없이 실제 장치 실행만 다시 검사
.\tools\bringup.ps1 -Configuration Debug -TestOnly -SkipBuild
```

산출물은 `Reversing/analysis/bringup-runs/날짜-구성/`에 쌓인다. `summary.json`의 result가 pass여야 통과이며, 성공 메시지 한 줄만으로 flash 보존이나 RTOS 동작을 추정하지 않는다. 기본 동작은 APP 기록과 소프트웨어 system reset을 포함한다. 전체 erase와 option-byte 쓰기는 없다. 실패하면 해당 상태와 로그를 남기고 중단한다.

100 kHz에서 코어를 멈춰 읽은 A/B가 일치해야 기록을 허용한다. Hot Plug는 관측에 사용하고, flash 기록은 NORMAL/SWrst 접속을 사용한다. 이 보드에서는 원래 RTOS 상태를 유지한 Hot Plug 기록이 sector 4 erase에서 실패했지만 NORMAL/SWrst 기록은 성공했다. 정확한 실패 원인을 그 사실만으로 확정하지 않는다.

2026-09-12 Debug/Release 실물 시험에서 각각 순정 BL → APP → HAL TIM6/FreeRTOS heartbeat와 소프트웨어 reset 3회 통과를 확인했다. 하위 64 KiB가 순정 백업과 일치함도 비교했다. 최종 장치는 Debug 이미지를 기록하고 1회 더 검증한 뒤 실행 상태로 남겼다. 자세한 결과는 `Reversing/analysis/2026-09-12-bringup/`의 시험 기록을 기준으로 판단한다. 물리 전원 차단/재인가 시험을 소프트웨어 reset 시험으로 대신했다고 주장하지 않는다.

## 재생성 보존과 검증 범위

생성 startup의 weak Reset_Handler와 벡터 파일은 수정하지 않고 BSP의 strong 정의를 링크한다. 사용자가 2026-09-12 수행한 GUI 재생성에서 Core USER 연결과 BSP 파일은 보존됐지만 Debug 링커가 기본 파일로 돌아간 것을 발견했다. 이를 복원하고 `sync_project.ps1`로 빌드 전 자동 재적용하도록 만들었다. 이후 실제 Debug/Release 빌드와 ELF 검사로 APP 주소·strong reset/task·NOLOAD 배치를 확인했다.

이번에 추가한 defaultTask `As weak`와 main strong task는 Cube 템플릿 및 실제 GCC 링크로 검증했다. 그 추가 설정 이후의 새 GUI 재생성 시험까지 완료한 것으로 확대하지 않는다. 향후 재생성 후 먼저 `tools/build.ps1`를 실행하거나 `tools/sync_project.ps1` 후 IDE 빌드한다. IOC 매핑이 맞아도 Cube가 바꾼 프로젝트 링커를 그대로 사용해서는 안 된다.

다음 재생성 후에는 Debug/Release 모두에서 전용 링커, strong Reset_Handler, APP 벡터 0x08010000, NOLOAD 진단 영역, Core USER 연결, App_Logic 소스/include 복원 및 실제 빌드를 확인한다. 정적 검증 도구는 잘못된 산출물의 사용을 막는 보조 수단이며 GUI 재생성 시험 자체를 대신하지 않는다.

함수별 실행 계약, 진단 해석과 제한은 [BSP 안내](Drivers/BSP/README.md)에 있다. 비공개 방침·주석 규칙·작업 범위는 [AGENTS.md](AGENTS.md)를 따른다.
