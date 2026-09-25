# 2026-09-12 IOC / 코드 생성 복구 진행 기록

## 현재 상태

SDK 설치, IOC 보완, 사용자의 CubeIDE GUI 코드 생성, 실제 CubeIDE GCC 빌드를 완료했다. 사용자가 커스텀 source/include 설정을 넣은 뒤 다시 Generate Code를 실행했고, 03:06:14에 생성된 설정에서 경로 보존과 Debug/Release 빌드를 확인했다.

초기에 별도로 실행한 IDE 내장 CubeMX는 EWARM으로 잘못 생성했다. 그 결과는 보관용으로 분리했으며, 현재 프로젝트는 사용자의 IDE 내 생성으로 만들어진 `.cproject`, GCC startup/링커 및 GCC FreeRTOS port를 사용한다.

## 직접 확인한 최초 원인

- `STM32CubeProjects/.metadata/.ide.log`의 2026-09-12 02:30:51 기록에 펌웨어 패키지 다운로드 시 myST 로그인이 필요하다는 메시지가 있었다.
- 기본 SDK 저장소에는 광고 PNG만 있었으며 요청한 CubeF4 1.28.3 소스가 없었다.
- IOC의 MCU, SWD, TIM6 HAL timebase, CMSIS-RTOS v2 선택 자체가 이 다운로드 실패를 일으킨 것은 아니다.

## SDK 설치

ST 공식 저장소의 `v1.28.3`에서 이 프로젝트가 사용하는 CMSIS, STM32F4 HAL 및 FreeRTOS를 설치했다.

- 출처: https://github.com/STMicroelectronics/STM32CubeF4/tree/v1.28.3
- 설치 위치: `<local-user>/STM32Cube/Repository/STM32Cube_FW_F4_V1.28.3`
- 부모 커밋: `94cae6e83f00e276a11957e7833c01ac3d0bd7af`
- CMSIS device: `3c77349ce04c8af401454cc51f85ea9a50e34fc1`
- HAL: `b6f0ed3829f3829eb358a2e7417d80bba1a42db7`

필요한 구성요소만 받은 sparse checkout이다. 앞으로 USB/FatFs 등 다른 미들웨어를 선택하면 해당 구성요소를 추가로 받아야 할 수 있다. 패키지 버전 정보를 임의로 변경하지 않았다.

## IOC 변경

원본은 `before.ioc`에 보존했다.

- 기본 태스크 스택: 128 words에서 1024 words, 즉 4096 bytes로 증가.
- FreeRTOS heap_4 명시, total heap 65536 bytes 설정.
- stack overflow 검사 2 및 malloc failed hook 활성화. 생성된 hook 본문은 아직 구현되지 않았다.
- 펌웨어 패키지를 1.28.3으로 유지하도록 `LastFirmware=false` 설정.
- `TargetToolchain=STM32CubeIDE`, `UnderRoot=true`로 복원 확인.

스택/힙 변경은 초기 개발용 용량 선택이며 코드 생성 실패의 원인 수정과는 별개다. C 런타임 heap 0x200과 메인 stack 0x400은 변경하지 않았다.

## EWARM 결과의 처리

IDE 밖에서 내장 `STM32CubeMX.jar -q`를 실행했을 때 `project toolchain STM32CubeIDE`가 OK를 반환했지만 EWARM 프로젝트를 생성했다. 로그는 `generation-attempt-1.log`, `generation-attempt-2.log`에 보존했다. 이 실행 방법을 성공한 CubeIDE 생성 절차로 재사용하지 않는다.

잘못 생성한 EWARM 디렉터리는 원래 프로젝트에서 `EWARM-unintended-generation`으로 옮겨 보존했다. 당시 CLI 스크립트도 `attempted-standalone-generation.mx.txt`로 이름을 바꾸었다.

## 실제 CubeIDE 생성 및 계층 설정 검증

사용자가 CubeIDE에서 Generate Code와 첫 빌드를 수행했다. 이후 원하는 폴더 구조를 다음처럼 반영했다.

- `Drivers/BSP/inc`, `Drivers/BSP/src`: 사용자 생성 위치 유지. `Drivers` 소스 루트에 포함된다.
- `App_Logic/inc`, `App_Logic/src`: 앱 코드 위치. 별도 `App_Logic` 소스 루트를 Debug/Release에 등록했다.
- Debug/Release의 C 컴파일러 및 assembler include 목록에 `../Drivers/BSP/inc`, `../App_Logic/inc`를 추가했다.
- `Core`는 Cube 생성 초기화/IRQ/RTOS 연결 코드로 유지했다. 보드나 앱의 가짜 동작을 추가하지 않았다.
- 비공개 프로젝트, 주석의 구어체/욕설 보존, GUI 자동화 금지, CubeMX 재생성 시 코드 보존 요구를 프로젝트 `AGENTS.md`에 기록했다.

재생성 전 Debug/Release 빌드는 각각 오류 0, 경고 0이었다. 사용자에게 프로젝트 설정을 다시 읽고 Generate Code를 실행하도록 요청했고, 03:06:14 재생성 결과에서도 두 구성의 커스텀 경로와 source root가 모두 보존됐다. 같은 CubeIDE headless builder로 재생성 후 두 구성을 다시 빌드해 오류 0, 경고 0을 확인했다.

검증 로그: `build-after-regeneration.log`. ELF size의 text/data/bss는 Debug 17932/16/70592 bytes, Release 11188/16/70584 bytes다. 이는 ELF size 분류값이며 각각 전체 파일 크기를 뜻하지 않는다.

현재 설정의 재생성 보존은 확인됐지만, 아직 작성하지 않은 BSP/앱 코드의 재생성 호환성을 선행 검증했다는 뜻은 아니다. 이후 코드 생성 관련 변경도 같은 절차로 검증한다.

`tools/build.ps1`은 설치된 CubeIDE의 managed headless builder를 별도 analysis workspace에서 실행한다. 더 이상 Computer Use를 사용하지 않는다. GUI 코드 생성은 사용자가 담당한다.

현재 생성된 코드는 Noodoe에 실행 검증한 펌웨어가 아니다. 순정 부트로더를 유지하려면 별도로 APP 시작 주소 0x08010000, VTOR 및 부트로더 인계 조건을 맞춰야 한다. 이번 작업에서는 장치 연결, 플래시 쓰기, 지우기, 리셋을 수행하지 않았다.
