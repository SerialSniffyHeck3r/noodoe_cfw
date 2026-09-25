# TI Bluetopia 적용 조사 — 2026-09-20

## 현재 판정

2026-09-21 후속: 사용자가 초기 SDK와 TI 담당자의 후속 수정 SDK를 설치했다. 수정본의 Cortex-M4/Thumb-2/hard-float 바이너리와 현재 GCC의 부분 링크를 확인했다. **하드웨어·컴파일러 측면에서 이식 가능한 후보지만, 현재 Product와 메모리 여유 조건을 유지하는 단순 교체 대상으로는 아직 적합 판정을 내릴 수 없다.**

최신 측정에서 FreeRTOS용 SPP 서버·인증·암호화의 코어 의존 코드만 73,694B다. 기존 BTstack 코어의 함수 귀속 추정은 32,640B이며, TI 수치에는 아직 프로젝트의 OS/UART/패치 어댑터가 없다. 따라서 이번 교체는 메모리 절감이 아니고 별도 공간 확보가 필요하다. SDK의 실제 설치 약관 전문도 확보하지 못했으므로 firmware-only 유료 배포 범위까지 확인됐다고 하지 않는다.

firmware 소스, 빌드 선택, MCU/NOR는 변경하지 않았다. 재현 가능한 ABI/용량 검사 도구와 결과, 이식 경계 및 상업 배포 확인 사항을 남겼다. 검증되지 않은 대체 함수를 Product에 연결하거나 실패를 성공으로 만드는 linker 옵션은 넣지 않았다.

## 수정본 설치와 검증 결과

- 초기 설치 경로: `C:\ti\Connectivity\CC256X BT\CC256x STM32 Bluetopia SDK\v5.1.1.1`
- 수정본 경로: `C:\ti\Connectivity\CC256X BT\CC256x STM32 Bluetopia SDK\v5.1.1.1_2`
- 수정 설치 파일: `6253.CC2564CSTBTBLESW-v5.1.1.1-windows-x64-installer.exe`, 38,872,976B
- 수정 설치 파일 SHA256: `5e1c4afa30059084ce59567a69ae04ed49abef8a8f25eb4b583fe5fda8dfb890`
- 출처: 아래 TI 담당자 게시물의 마지막 수정 첨부. 초기 파일과 표시 버전은 같으므로 경로/해시로 구분해야 한다.
- 초기 FreeRTOS M4.fp archive SHA256: `b793ca7d4ccba7bb91f260666f30a9661ef7a5c736784653adf4bae35b808bbb`. 실제 v5TE/FA626TE 객체이며 hard-float 링크 실패. `first-distribution-abi`에 원본과 오류를 보존했다.
- 수정 FreeRTOS archive SHA256: `1bd1bbbeef8aead4eefa4c8e0eac72be51316021a2a0932dd80830fc9eb2f19b`.
- 수정 NoOS archive SHA256: `489b7c961c4323b1465ce1e6c558e59006e79543eeeb4f0a220e21a2303e8d7f`.
- FreeRTOS/NoOS 각46개 ELF member를 직접 추출해 속성을 확인했다. 최종 근거는 `corrected-core-final`, GCC 부분 링크는 `corrected-abi-final`이다. 초기 `corrected-distribution-abi*`의 archive-header 읽기 오류와 `corrected-core-measurement`의 stdout 추출 결과는 member ABI 판정에 사용하지 않는다. Windows `ar p`의 바이너리 stdout 변환을 피하도록 최종 도구는 `ar x`로 파일에 직접 추출한다.
- 수정본은 GCC 디렉터리에 있으나 내부 compiler metadata는 ARM Compiler5.06이다. GCC와 EABI 부분 링크는 가능하다. TI 경계는 small enums/wchar32이므로 향후 어댑터에만 `-fshort-enums` 및 구조체 크기 검증을 적용해야 한다. 전체 프로젝트 enum 크기를 바꾸지 않는다.
- 부분 링크 성공은 전체 firmware 링크나 무선 실행 성공이 아니다. OS/UART/vendor hook을 미해결 심볼로 명시적으로 남겨 둔 평가다.

## 코어 용량 측정과 적용 판단

`measure_sdk_core.py`가 실제 SDK의 API를 `-u` root로 지정하고 GNU linker의 `-r --gc-sections`를 적용했다. 헤더의 generic SDP macro가 실제로 부르는 `SPP_Register_Raw_SDP_Record`를 사용한다. 평가용 가짜 OS/UART 구현은 없다. 산출물은 재배치 가능한 `.o`이며 실행/설치할 수 없다.

| SDK / 참조 집합 | 코어 text+rodata+data | 코어 BSS | 아직 미해결 심볼 |
|---|---:|---:|---:|
| FreeRTOS 초기화만 | 58,088B | 2,640B | 39 |
| FreeRTOS SPP 서버 + 인증/암호화 | **73,694B** | 2,640B | 39 |
| NoOS 초기화만 | 56,132B | 2,640B | 33 |
| NoOS SPP 서버 + 인증/암호화 | **71,738B** | 2,640B | 33 |

단일 폰의 SPP 용도로 선정한 API 집합이다. A2DP/AVRCP/GATT 프로필 라이브러리와 예제 UI를 붙이지 않았다. 내부 코어 초기화가 유지하는 공통 코드까지 강제로 지우거나 인증을 빼서 숫자를 맞추지 않았다. 실전 어댑터가 사용할 추가 API/콜백/타임아웃에 따라 최종 크기는 달라진다. 약1MiB인 `.a` 파일 크기를 MCU 플래시 사용량으로 계산하지 않았다.

기존 Product Release는341,272B이며 layout2의384KiB에서51,944B가 남는다. 기존 함수 귀속 추정에서 BTstack32,640B + Noodoe Bluetooth9,342B를 **모두 제거한다고 낙관적으로 가정**하고 위 TI 코어만 더해도 약372,984B/여유20,232B다. 여기에는 새 어댑터가 없고 기존 읽기전용 데이터의 귀속 변화와 LTO 변화도 반영하지 않았다. 따라서 이것은 최종 ELF 값 또는 엄밀한 상한/하한이 아니라 사전 용량 추정이다. 그 추정부터 Release64KiB 여유 목표에45,304B가 부족하며, 새 포트가 추가된다. 정확한 총량은 실제 대체 구현 후 전체 ELF로 판정해야 한다.

NoOS 코어로 바꿔도 이 측정에서는1,956B만 감소한다. 별도 cooperative scheduler/동기화 이식이 필요한 변경이며 현재 FreeRTOS 포트의 대체 완료가 아니다. SDK FreeRTOS 샘플은20KiB BTPS heap과 타이머512B/HCI dispatch3,584B 스택을 요구하는 기본값도 가진다. 이를 기존48KiB RTOS heap에서 무조건 추가 소비하거나 스택을 줄여 예산에 맞추지 않는다. 현 SDK 코어는 미리 컴파일돼 있으므로 앱의 `-Oz`로 코어 자체를 다시 최적화할 수 없다.

SDK에는 `Using the Flexible Build Library.pdf`가 있으나 FBL 스크립트/대체 객체는 없다. TI도 동일한 누락 질문에 STM 호스트 배포는 사전 컴파일 라이브러리이고 재컴파일용 소스를 제공하지 않는다고 답했다:
https://e2e.ti.com/support/wireless-connectivity/wi-fi-group/wifi/f/wi-fi-forum/1455926/cc2564c-how-to-access-flexible-build-library-for-stm32-for-cc2564c-we-want-to-use-the-core-and-gatt-library-in-stm32u5-in-stm32cube-ide

그 결과 지금 적용을 완료하려면 적어도 (1) 더 작은 공식 Classic/SPP 빌드 확보 또는 다른 부분의 실제 추가 공간 확보, (2) 정확한 SDK 배포 약관 확인, (3) 기존 세션/OTA/저장/절전 계약을 보존하는 포트 및 전체 빌드가 필요하다. 이 조건을 충족하지 않은 채 기본 스택을 교체하거나 설치하지 않았다. 외부 NOR는 현 연결에서 실행용 XIP가 아니며, SDRAM 실행으로 우회하는 방법도 이번 메모리 정책에 넣지 않는다.

## 확보한 자료와 설치 파일

- TI 제품: https://www.ti.com/tool/CC256XSTBTBLESW
- TI C-revision 제품: https://www.ti.com/tool/CC2564CSTBTBLESW
- 공식 제품 다운로드는 TI 로그인/수출 승인 경로로 연결됐다. 익명 HTTP 요청은 401을 반환했다. 로그인 우회나 사용자 정보 제출은 하지 않았다.
- 별도로 TI 담당자 RogelioD가 STM 호스트용 수정 배포본을 공개한 게시물을 확인했다:
  https://e2e.ti.com/support/wireless-connectivity/bluetooth-group/bluetooth/f/bluetooth-forum/1340511/cc2564cstbtblesw-acrcp-profile-version-going-to-v1-4-when-using-the-audiodemo-api-s
- 위 게시물에 명시적으로 게시된 첨부 파일을 다운로드했다:
  https://e2e.ti.com/cfs-file/__key/communityserver-discussions-components-files/538/CC2564CSTBTBLESW_2D00_v5.1.1.1_2D00_windows_2D00_x64_2D00_installer.exe
- 로컬 파일: `CC2564CSTBTBLESW-v5.1.1.1-windows-x64-installer.exe`
- 길이: 38,156,152 bytes
- SHA-256: `8b85afa17ff0004d6d14dd5e21c27f97a28b42b745230866ff98ef63ee1f2688`
- 이 해시는 로컬 획득본의 식별값이다. TI가 게시한 서명/해시와 대조했다는 뜻은 아니다. 이름이 같은 제품 페이지 설치 파일과 바이트 동일성도 확인하지 않았다.
- 최초 자동 `--help` 실행은 Windows 관리자 권한 요구로 거부됐다. 이후 사용자가 직접 초기/수정본을 설치했다. 에이전트가 UAC 우회/manifest 변경/GUI 자동화/약관 수락을 수행한 것은 아니다.

## 확인된 호환성 근거와 미확인 항목

| 항목 | 확인 내용 | 남은 확인 |
|---|---|---|
| MCU/SPP | TI 제품 페이지는 STM32F4와 Classic SPP, RTOS 환경을 지원한다고 명시 | Noodoe 보드 포트 및 정상 보드의 실제 무선 시험 |
| FreeRTOS | SWRU498B에 FreeRTOS/NoOS 예제가 안내됨 | 실제 SDK BTPSKRNL의 동기화·할당·태스크 소유권 |
| GCC | 수정본의 M4/Thumb2/hard-float member와 GCC 부분 링크 확인 | 전체 포트·구조체 ABI·최종 firmware 링크 |
| 칩 revision | 기존 서비스는 TI manufacturer 13, B ROM 0x1B90/C ROM 0x9A1A를 구분 | 선택 SDK의 B/C 초기화 지원과 정상 기판의 실제 ID |
| 상업 사용 | TI 제품은 royalty-free로 안내됨 | 해당 STM32 SDK 약관과 기존 타사 기기용 유료 firmware-only 배포 범위 |
| 용량 | 기존 Product Release341,272B, 수정 SDK의 선정 SPP 코어73,694B | 추가 공간 확보 및 실제 전체 Product ELF |

GCC 자료:
https://e2e.ti.com/cfs-file/__key/communityserver-discussions-components-files/538/CC256XSTMNoOS_5F00_Release_5F00_Notes-.pdf

FreeRTOS 자료:
https://www.ti.com/lit/ug/swru498b/swru498b.pdf

공개된 WL183x/CC256x 라이선스 예시는 royalty-free와 제품에 포함한 실행 코드 배포를 명시하지만, Licensee Product를 하드웨어+소프트웨어로 정의한다. 이 문서는 받은 STM32 SDK의 실제 약관을 대체하지 않으며, firmware-only 유료 배포 승인으로 취급하지 않는다:
https://e2e.ti.com/cfs-file/__key/communityserver-discussions-components-files/538/TI_5F00_BluetopiaPM_5F00_Software_5F00_License.pdf

설치된 `CC256XSTBTBLESW_5.1.1.1_manifest.html`은 core/headers/BTPSKRNL/HCITRANS 등을 TI Commercial로 분류하나 commercial 약관 전문을 싣지 않는다. 로열티 무료라는 제품 안내는 확인됐지만 자유로운 오픈소스 라이선스라는 뜻은 아니다. 번들된 오래된 FreeRTOS를 가져오지 않고 기존 Cube의 kernel을 유지하는 방향이며, SDK manifest의 옛 FreeRTOS 조건을 현재 프로젝트 전체에 적용됐다고 해석하지 않는다.

## 로컬 코드에서 확인한 이식 경계

프로젝트: `../../STM32CubeProjects/FuckNudo_Noodoe_CFW_Project`

- `Drivers/BSP/src/bsp_bt_hci.c`와 `inc/bsp_bt_hci.h`의 USART1 PA9/10/11/12, DMA2 S5/S7, RTS/CTS, PA8/PI1 초기화 계약을 재사용한다. RX DMA를 준비한 뒤 reset을 해제하는 10/150ms 순서를 SDK 보드 예제로 덮어쓰지 않는다.
- `Middlewares/Noodoe/Bluetooth/src/bluetooth_port.c`의 BTstack run-loop/UART adapter는 TI HCITRANS 연결로 바꿔야 한다. ISR은 완료 통지까지만 하고 프로토콜/저장/UI 작업을 하지 않는다. DMA 버퍼는 일반 SRAM에 유지한다.
- `bluetooth_service.c`의 HCI/GAP/RFCOMM/SDP callback, 타이머 및 스택 초기화는 TI API로 바꿔야 한다. 현재 8KiB 정적 owner 태스크, HealthService heartbeat, PowerService ACK, 비동기 요청/완료 구분은 보존 대상이다.
- `NoodoeBluetooth.h`의 공개 Start/Stop/SendSession/ReceiveSession/GetLinkState 계약과 세션 epoch 검사는 유지한다. 첫 이식 시험은 폰 한 대의 SPP로 한다. 현재 소스는 여전히 PHONE/ELM/PHONE2 세 역할이며, 역할 축소와 상위 메뉴 삭제가 이미 구현됐다고 보고하지 않는다.
- `bluetooth_keys.c`의 키 저장/세대/Import/Export 형식과 App_Logic/Settings의 영구 저장 계약을 유지한다. TI callback에는 명시적인 주소 순서 변환과 키 형식 검증이 필요하다.
- `Bluetooth_LinkSecure()`는 RuntimeInstaller의 승인 조건이다. TI의 실제 인증/암호화 상태를 세션에 연결해 제공해야 하며, 단순 SPP 연결 성공으로 true를 만들지 않는다.
- `Bluetooth_QuiesceTransport/ResumeTransport`는 RuntimeUpdate의 flash 작업 전 안전 계약이다. RTS hold/전송 drain/유한 대기/실패 시 중단을 그대로 제공해야 한다.
- `bluetooth_patch.c`의 Product 경로는 검증된 외장 NOODOE.RSC의 B/C 패치를 쓴다. SDK의 정적 patch 배열을 무조건 추가해 내부 플래시를 다시 소비하지 않는다. 기존 patch는 BTstack converter 산출물이므로 TI 소비 형식과 동일하다고 가정하지 않는다.
- `module.build.json`, `tools/services_build.py`, `tools/bootstrap_build.py`의 vendor 선택/include/link 경로를 함께 갱신해야 한다. Cube 생성 파일과 vendor 원본을 직접 수정하지 않는다. Product/Integrated/Graphics/Bootstrap의 의도한 의존성을 각각 확인한다.

## 실제 ELF 호출 규약

기준 ELF: `../2026-09-20-independent-recovery/builds/Product-Release/FuckNudo_Noodoe_CFW_Project.elf`

설치된 CubeIDE GNU 도구의 `arm-none-eabi-readelf -A`로 확인:

- ARM v7E-M / Thumb-2
- VFPv4-D16, single-precision hardware FP
- `Tag_ABI_VFP_args: VFP registers` (hard-float)
- `Tag_ABI_PCS_wchar_t: 4`
- `Tag_ABI_enum_size: small` (최종 ELF 집계 속성; 개별 C 경계 enum 크기는 추가 확인)

TI archive의 ABI·enum/구조체 배치·의존 심볼을 확인하기 전에는 단순히 .a 파일 이름에 GCC/M4가 있다는 이유로 호환 판정을 내리지 않는다. 링크 경고 억제나 무조건적인 ABI mismatch 무시로 통과시키지 않는다.

## 다음 구현의 구체적 경계

1. 동일 버전 Classic/SPP용 작은 공식 library가 있는지 및 firmware-only 유료 배포 조건을 확인한다. `TI_QUESTIONS_DRAFT.md`는 검토용 초안이며 전송하지 않았다.
2. 그 결과와 실제 추가 메모리 확보량에 따라 프로젝트 소유 BTPSKRNL/HCITRANS/BTPSVEND 어댑터를 적용한다. SDK 예제 보드 GPIO/clock/kernel을 가져오지 않는다.
3. 기존 공개 API는 유지하되 Bluetopia callback 실행 문맥과 BT owner 요청 큐를 구분한다. epoch별 RX/TX, 실제 인증/암호화 증거, persistence generation, quiesce 토큰, HealthService heartbeat를 모두 회귀 시험한다.
4. 전체 Product/Integrated/Graphics/Bootstrap 빌드 및 메모리 여유 기준을 통과한 다음 설치한다. 손상 donor의 HCI 실패를 새 스택의 정상 무선 동작으로 보고하지 않는다. 정상 보드에서 pairing/reconnect/SPP/OTA/standby를 별도 검증한다.

현재 검사 도구는 실제 SDK를 읽고 컴파일/부분 링크했다. Production용 stub 구현, 기본 스택 교체, 전체 Product 후보 ELF 생성, 실기기 설치는 하지 않았다.
