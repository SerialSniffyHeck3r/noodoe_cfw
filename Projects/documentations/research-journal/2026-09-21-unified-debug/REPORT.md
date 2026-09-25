# Debug 정책 통일

2026-09-21. 사용자가 승인한 대로 Debug를 **`-Os -g3`, LTO 비활성**으로 통일했다. 기능, 오류 검사, 워치독, 힙·스택, 파티션과 메모리 합격 기준은 변경하지 않았다. 이번 작업은 빌드 설정 변경이며 장치에 설치하지 않았다.

## 변경

- Product/Integrated/Graphics의 Debug C/C++ 기본 설정을 한 곳에서 관리한다: 프로젝트 `tools/build_optimization.py`.
- 기존 Debug의 프로젝트 소유 **파일별 예외 149개와 폴더별 예외 6개를 모두 제거**했다. 새 파일도 기본 설정을 상속한다.
- Debug의 사진/UI 일부에만 적용되던 LTO도 제거했다. 컴파일과 링크에 `-fno-lto`를 명시한다. 최종 Product Debug 빌드의 C 컴파일 명령 390개가 모두 `-Os -g3 -fno-lto`이고 숨은 최적화/LTO 옵션은 없었다.
- Cube 동기화의 LVGL 폴더 설정 복제 및 vendor 폴더별 설정 복제를 없앴다. `sync_project.ps1` → `services_build.py` → 공통 정책 순서로 복원한다. 생성 C와 IOC는 수정하지 않았다.
- 의도적으로 사용자가 추가한 파일별 Og/O0 디버깅 설정은 보존한다. 기본 정책은 바꾸지 않고 해당 파일만 잠시 사용하며, 매번 메모리 예산 검사를 통과해야 한다. Other flags에 숨긴 최적화 및 Debug LTO는 거부한다.
- Release의 기존 Oz/LTO 및 FreeRTOS naked asm/EVE wrap 비-LTO 경계는 유지했다. 재빌드한 Product Release BIN은 변경 전 BIN과 **바이트 단위로 동일**하다.

## Product 결과

| 항목 | 이전 Debug | 통일 Debug | Release |
|---|---:|---:|---:|
| APP 크기 | 367,432B | **349,460B** | 316,888B |
| 플래시 여유 | 25,784B | **43,756B** | 76,328B |
| 일반 SRAM 여유 | 63,856B | 63,936B | 65,384B |
| CCM 여유 | 16,320B | 16,320B | 16,320B |

Debug는 **17,972B 절감**, 필요한 32KiB 여유보다 10,988B 더 남는다. FreeRTOS 힙 48KiB, LVGL CCM 풀 48KiB 및 보호값을 유지했다. 디버그 심볼은 PC의 ELF에 그대로 남고 MCU 플래시를 차지하지 않는다. 최적화에 따라 일부 지역변수와 한 줄씩 실행하는 동작은 달라질 수 있다.

## 검증

- 빌드 정책 단위시험 9개: 옛 설정 이관, C/C++ 기본값, 멱등성, 수동 Og 유지, 숨은 최적화/LTO 및 잔존 소유 예외 거부, Release 보존, LTO 경계 및 stack report 검사.
- 격리된 재생성 fixture에서 실제 PowerShell 동기화를 실행해 세 프로필 왕복, 반복 실행의 바이트 멱등성, USER CODE/IOC 보존을 확인했다. 이번에 실제 GUI Generate Code를 추가 수행했다고 해석하지 않는다.
- 워치독/Health 실제 ARM 모의시험: O0/Os 각각 206 assertions 통과. C 초기화 전 시작, LSI 실패, IRQ 급식 거부 및 유한 lease 등을 검사했다.
- Gate Product 업데이트 실제 ARM 모의시험: 72개 실행 통과. 실제 NOR/BT/리셋/전원 차단 실험과는 구분한다.
- Product Debug/Release 최종 ELF가 주소 배치·메모리 예산·폐기 기능 심볼 검사를 통과했다. 추가 프로필 결과 및 해시는 `verification-summary.json`에 기록한다.
- 같은 정책의 Integrated Debug도 APP 395,156B/여유 63,596B, Graphics Debug도 APP 426,752B/여유 32,000B로 빌드 및 해당 프로필 검사를 통과했다. Graphics의 기존 미사용 `ign` 변수 경고와 RAM 실행 코드의 기존 RWX segment 경고는 남아 있다. 이 작업에서 경고를 숨기는 옵션은 추가하지 않았다.

## 기존 Bootstrap 설치 묶음 차단 해제

새 Debug ELF로 이전의 6,984B 예산 부족이 해결되어 `installer/installer.zip`을 생성했다. Bootstrap·Gate·순정 복구본·자산과 실제 Product Release/Debug ELF를 패키저가 검증했다. Android APK에 사용한 실제 컴파일된 importer로 이 ZIP을 읽고, Gate+Product 배치와 해시 및 **벤치용 묶음의 무선 설치 거부**를 확인했다.

이 묶음의 대상은 승인된 V5.16 / BL0.14 / sr0601에 대한 **bench-only** 근거다. 실제 실차 BL0.15 / SR0701의 호환성이나 Bluetooth 설치 검증이 새로 완료된 것은 아니다. 메모리 문제 해결과 실차 설치 허용은 별개다. 앱·펌웨어를 장치에 전송하지 않았다.

이전 `2026-09-21-bootstrap-ux` 보고서의 Debug 예산 차단은 변경 전 기록으로 보존하며, 최신 메모리 판정은 이 보고서를 따른다.
