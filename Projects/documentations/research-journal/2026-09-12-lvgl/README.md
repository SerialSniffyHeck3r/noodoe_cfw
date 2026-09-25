# LVGL/EVE 통합 기록 — 2026-09-12

프로젝트 구현은 `../../STM32CubeProjects/FuckNudo_Noodoe_CFW_Project/Graphics/README.md`, 시험 목록은 그 아래 `UI/TESTS.md`를 본다. GUI나 Computer Use는 사용하지 않았다.

## 구현 범위

LVGL 9.5.0 원본과 EVE generation2 렌더러를 사용한다. BSP가 수행한 패널·SPI·전원 초기화를 인계하는 별도 display adapter, 오류 제한시간, 캐시 수명 관리, 물리 버튼 encoder/observer, 공개 API를 추가했다. 원본 LVGL 파일은 수정하지 않았다. EVE clip cache는 포트 dispatch wrapper에서 보정한다.

main의 태스크는 `LCDTest();` 한 줄을 유지한다. 이전 raw 화면 시험은 LCDTestLegacy에 보존했다. 새 시험은 41개 항목을 순회하며 단순 위젯 실행30개와 명시적인 미지원11개를 구분한다. UART·BT·파일 배경·한글 폰트·상용 완성 UI를 구현했다고 주장하지 않는다.

## 최초 하드웨어 실행

증거: `../bringup-runs/2026-09-12-105142-290-Debug`.

- Debug 이미지442,628byte, SHA256 `a7179998fa48b493761b6c3d50533d0b756bea4d1be9657cd510edf98cedc1e0`.
- 쓰기 전 전체 백업A/B 일치. APP 쓰기·프로그래머 verify·독립 readback 일치.
- 순정 하위64KiB SHA256 `f38ed839c4fbce74d4d8227fcf13764194f405c4860cda75c628fce66064b574` 보존.
- 첫 dashboard와 controls 프레임 제출79회까지 관측. EVE60MHz, PCLK3, SPI오류0, 경고0, DL2,888byte, 렌더 최대25ms(이 구간에서만 측정).
- 전체 시험은 **실패**: 화면0→1 전환 부근 FreeRTOS stack-overflow hook `0x301` 기록. 기존4KiB 태스크 스택의 마지막 정상 여유130word. GPU는 fault0/idle FIFO였고 LVGL heap은약11.5KiB 사용이었다. 이 초기 실행을 최종 통합 PASS로 취급하지 않는다.

이 결과에 따라 defaultTask 스택을12KiB(3072words)로 늘리고 IOC/재생성 동기화/검사를 함께 갱신했다. `.su`의 재귀 렌더 프레임은 깊이당약624byte이며 menu의 깊이를 고려해 여유를 뒀다. 엄밀한 정적 최대 스택 크기 증명은 아니다.

## 12KiB 재시험

증거: `../bringup-runs/2026-09-12-110538-953-Debug`. APP 검증·하위64KiB 보존·reset 이후 태스크/tick·첫 두 case 진행은 PASS했다. 두 번째 표본의 스택 여유8,108byte, fault0, GPU/SPI 오류0이었다. 이 짧은 재시험도 전체41항목 순회를 대신하지 않는다.

## 소프트웨어 검증

- 입력/수명: `tools/tests/graphics_input_host`, 실제 입력 소스와 실제 Shutdown 함수의 ARM 실행, O0/O2 각각10사례·40검사 PASS.
- EVE cache: `tools/tests/eve_state_host`, 원본 lv_eve.c와 실제 adapter 경로 ARM 실행, O0/Os 각각8사례·55검사 PASS.
- SWD 판정: `tools/tests/test_graphics_checks.py`, 17개 회귀검사 PASS. 렌더 완료와 SKIPPED를 분리하고 누적 결과 배열을 읽는다. Cortex-M4의 CONTROL=2와 FPCA가 추가된6은 모두 privileged PSP Thread이므로 정상으로 허용하며, 비특권/MSP/예약 비트/IRQ mask는 계속 거부한다.
- 스택 재생성 복원: `tools/tests/task_stack_contract`, 실제 sync fixture13검사 PASS. 1024word 생성형태 복원, 다른 Core byte 보존, 중복/미지정 형태 거부, 멱등성을 확인했다.
- LVGL 빌드 등록: `registration-validation.json`, 별도 fixture에서 source/include/define/링커/최적화 설정을 잃은 상태의 복원 확인. 실제 사용자 GUI 재생성은 이번 변경 뒤 수행하지 않았다.

## 최종 빌드 및 설치

- Debug: 442,836byte, SHA256 `bcd3d814c64f0357f5019cc7585cd7b4b2e2fb1d7a0b97089990fbe7266e2445`.
- Release: 416,824byte, SHA256 `c4f7b0dcc78fc11bfb578b74fe9b1c18dc5236dfc5f73de3648310d8dae33769`.
- 두 구성의 빌드·APP 범위/벡터 검증 통과. 실물에는 Debug를 설치했으며 Release는 빌드 검증만 했다.
- 설치 증거: `../bringup-runs/2026-09-12-111533-762-Debug`, 요약 로그 `bringup-final.log`.
- 쓰기 전 전체 백업 A/B 일치, APP programmer verify·독립 readback 일치, 순정 하위64KiB 보존, reset 이후 HAL tick/RTOS 진행 검사 PASS.

`probe-final-full-cycle`은 case25까지 정상 렌더와 오류0을 읽었지만, 기존 검사기가 CONTROL=6의 FPCA를 예상하지 않아 중단됐다. 장치 fault가 아니라 호스트 판정 오류이며 기록은 남겨 둔다. 검사 조건 수정 뒤 장치를 다시 쓰거나 reset하지 않고 `probe-final-complete`에서 누적41개 결과를 확인한다. 프레임 제출 및 GPU 동작 확인은 실제 LCD의 육안 품질 합격과 구분한다.

## 전체 방문 관측과 측정 도구 보정

`probe-final-complete/sample-001-try1.json`과 `sample-002-try5.json`의 누적 배열에서30개 RENDERED 및11개 SKIPPED를 모두 읽었다. 두 표본은 약542초/554초 active tick이며 렌더 횟수5,887→6,016, 시험 화면26→28로 진행했다. GPU/SPI 오류, LVGL 경고, 할당 실패, 입력 큐 유실은 모두0이었다. 최저 잔여 task stack은1,559word(6,236byte), FreeRTOS 최소 잔여 heap53,128byte, LVGL heap peak15,548byte, GPU RAM_G21,700byte였다. 이 실행 구간의 최대 render25ms/Graphics process47ms는 특정 시험의 관측값이며 모든 UI의 최악 시간 보장은 아니다.

이 관측의 CLI 전체 판정은 tick 비교 때문에 실패로 남아 있다. 두 번째 안정 표본을 얻기 위해 busy 상태의4개 표본을 다시 읽었으며, HAL−kernel 차이는 실제 capture 순서로8→9→10→12→13→14ms였다. 각 halt/read/run 구간의 차이는1,1,2,1,1ms로 기존±2ms 기준 안에 있지만, accepted 표본만 비교한 기존 probe가 누적6ms를 한 번의 측정으로 취급했다. 별도 probe는 모든 실제 capture의 tick/evidence를 보존하고 각 인접 구간에 동일±2ms 검사를 적용하도록 고친다. 일반 bringup의 기준과 펌웨어는 변경하지 않는다. SWD로 CPU를 멈추는 이 검사는 무중단 실시간 clock 정확도 시험이 아니다.

## 추가 요청:30fps·CPU 표시·원형 배치

사용자가 실물에서 느린 frame rate를 보고했다. 최초 시험은 합성 값100ms 갱신 때문에 일반 페이지 약80frames/8초(10fps), LVGL 자체 spinner는226frames/8초(약28fps)였다. 서비스33ms 및 절대 시간 누산30Hz로 바꾸고, 정적 화면도 다시 그리는 시험 모드와1초 실측 FPS/RTOS 비유휴율 HUD를 추가했다. 세부 계약은 프로젝트 `Graphics/PERFORMANCE.md`에 있다.

- 새 Debug:445,836byte, SHA256 `4629739619cf7c73be799d32224870448880871c77e197cab978dbc09cd1388d`.
- 새 Release:418,732byte, SHA256 `cbe90dfaccb21e8aab417e2817f723ba825c9a7c1f0e006caac7310b2d135114`.
- 두 구성0 errors/0 warnings 및 APP 계약 통과. CPU/pacer/원형 geometry의 실제 C ARM 실행은 O0/Os 각각146검사 PASS.
- 위에 기록한 이전 Debug 실측은 새 펌웨어의30fps 성능 증거가 아니다.

### SWD 읽기 불일치와 보존

100kHz의 두 시도 `115717-540-Release`, `115911-988-Release`는 쓰기 전 BL 비교에서 중단됐다. 첫 dump의0x1CC0 부근은4byte 밀린 모양이었고 전체64KiB 및 해당block 재읽기는 원본과 일치했다. GUI는 실행 중이었지만 사용자가 Disconnected임을 확인했다. GUI 동시 접근을 확정 원인으로 주장하지 않는다.

50kHz 시도 `120259-242-Release`는 BL 일치 후 전체A/B의0x57000 page에서882byte 차이를 발견해 **쓰기 전에** 중단됐다. `tools/verified_flash.py`는 불일치한4KiB page만 두 번 새로 읽어 두 값이 같고 원래 관측 중 하나/정확한 기준과도 같을 때만 별도 verified 파일로 조합한다. 원래 raw 파일을 덮거나 기준 byte로 채우지 않으며, 재확인도 다르면 중단한다. 범위/실제 재읽기/불일치 거부7개 단위검사 PASS.

`continue_30fps_install.py`는 그 정확한 실패 기록을 이어간다. 기존A/B page 확인 후 현재 장치 전체512KiB도 다시 대조하고 APP를 기록한다. 현재 전체 확인 중0x4C000/0x7A000 page도 별도 두 읽기로 확인했다. 모든 원시 파일과 수정 경위 JSON을 남긴다. **복구에 사용할 파일은 `before-full-verified-a.bin`/`before-full-verified-b.bin`이며, 실패 기록의 원시A/B는 비교 증거다.** 이 동작은 검증된 byte의 재관측이며 flash 내용 복원/수정 동작이 아니다. 실제 APP 기록은 이후의 별도 programmer 명령뿐이다.

### 30fps Release 설치 및 무정지 관측 완료

`../bringup-runs/2026-09-12-121307-947823-Release-continued/summary.json` PASS. APP programmer verify와 독립 readback, 순정 하위64KiB 보존, 실제 BL reset 경로를 확인했다. 초기 두 표본의 FPS29.8/30.0, CPU43.4/37.9%, 구간 평균29.936fps였다.

이후 `probe-30fps-round`의 APP identity read가 CPU를71.798초 멈춘 채 실행되어 사용자가 화면 정지를 보고했다. 반복 probe를 중단했으며 마지막 명령은 이미 `-run`을 완료했다. 변경 전/후 debug-freeze는 모두0x1800이었다. 사용자도 이후 "어 제대로 나와"라고 확인했다. 이 정지를 앱 crash나 GUI 동시 접속 탓으로 결론내리지 않는다.

`observe_running.py`와 `live-30fps-round/result.json`은 이미 검증된 같은 세션의 APP manifest를 사용해 작은 RAM 진단만 HOTPLUG으로 읽었다. halt/reset/레지스터 쓰기가 없고 읽기 전후 DHCSR.S_HALT=0이다. 약439초 active tick까지 누적30개 RENDERED와11개 SKIPPED, GPU/SPI/할당 오류0을 확인했다. 마지막 두 구간 평균은약29.98fps이며 최저 잔여 stack6,336byte, LVGL heap peak15,908byte, RAM_G21,750byte다. maximum Graphics_Process74ms에는 case 전환도 포함되고 render 최대는23ms였다. 연속 샘플에서 같은1초 performance 구조가 일치한 calendar 표본은30.1fps/CPU71.3%다. 다른 live 표본은 시간 차이가 나는 비원자적 관측으로 보존하며 순간 성능 보장으로 쓰지 않는다.

### 사용자 지정 원형 영역 및 호 시험

원본 사진 `8f3c6ec2bb08c71586b00c691a8192a7.jpg`의SHA256과 원경계fit는 `viewport/VIEWPORT.md`, `viewport/reference-analysis.json`에 있다. 중심(239.5,239.5), 반경239.5를 공통 `Graphics_Viewport.h`에 고정했다. 바깥 draw task를 생략하고 최종120byte EVE stencil 제외로 바깥을 고정 검정으로 만든다. 이 고정 래스터 명령을 "바깥pixel주사중단"으로 표현하지 않는다.

case0은 별도 `Graphics_ArcSweep.c/h`가 구현한 반경220px,270도 호다. 4초 채움/4초 비움의 smoothstep과 정수속도에 독립적인0..10000 값을 사용한다. 기본 부팅은 case0 고정이며 DOWN으로 기존41개 순회를 재개한다. main은LCDTest(); 한 줄을 유지한다.

- Release:420,592byte, SHA256 `9a60e970fe3db7aff519be130c0fa21b20ee47e2be5187f4c2edf925a7d398b0`.
- Debug:449,976byte, SHA256 `6f399b3da393502ae3fea61ee92c2297b50972eca9b572d3e8ce362f3b569775`.
- 두 구성0errors/0warnings 및 APP 범위/벡터 검증 PASS.
- 실제 C의 ARM/Unicorn 검사: CPU/pacer/원형기하 O0/Os각309, EVE state/culling/고정제외 O0/Os각12사례159검사, 호수명/실패정리/easing/wrap/HUD O0/Os각1,701검사 PASS.
- 설치 진행 증거: `../bringup-runs/2026-09-12-124758-807-Release`, `bringup-arc-viewport.log`. 이전30fps 실측을 새 호/마스크 이미지의 검증 결과로 대체하지 않는다.
