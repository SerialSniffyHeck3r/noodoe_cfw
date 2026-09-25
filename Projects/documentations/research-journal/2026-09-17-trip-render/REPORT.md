# Trip rendering and stop threshold — 2026-09-17

최종 후보를 보드에 설치하고 전체 APP readback, 순정 하위64KiB 보존, 실제 UART 누적 시험과 GPU 캡처를 완료했다. 코드에서 재현되는 클리핑 결함은 수정했다. 다만 사용자가 목격한 순간적인 실물 scanout 왜곡 자체를 측정 장비로 재현하지 못했으므로, 모든 찢어짐의 원인이 확정되거나 사라졌다고 주장하지 않는다.

## 렌더링 변경과 근거

1. 원본 `lv_eve_scissor()`는 XY와 반대쪽 끝 좌표를 따로 캐시한다. 시작점만 움직이면 SIZE도 바뀌어야 하는데 이를 생략할 수 있다. SAVE/RESTORE 뒤의 C 캐시도 실제 clip과 어긋날 수 있다. 이전 adapter는 매 draw 앞에 가짜 offscreen clip을 넣어 이 문제를 우회했다.
2. 별도 `graphics_eve_clip.c`가 linker wrap으로 clip 연산 전체를 소유하도록 바꿨다. 정확한 inclusive XY/SIZE 두 명령을 발행하고, 기존 원본의1px 밖 확장을 제거한다. 인접 페이지 및 Moving/Stopped 반쪽 영역 사이에서 clip이 겹치는 경로를 없앤다. vendor 소스는 보존했다. 이전 dummy clip은 제거했다.
3. BEGIN은 SAVE_CONTEXT 대상이 아니다. 기존 primitive 동기화 계약을 보존하면서 cached RECTS setter와 명시적 BEGIN(RECTS)로 실제 상태를 맞춘다. 기존의 두 primitive 교란 대신 한 번의 명시적 기준을 사용한다. FULL refresh, DISPLAY/CMD_SWAP, 페이지 두 bank와 fade, 해상도·폰트·레이아웃은 유지했다.
4. 기존 command burst는 FIFO 여유를 확인하지 않고 보낼 수 있었다. REG_CMDB_WRITE에만 byte credit을 적용하고, 부족하면 CS를 닫아 공간을 다시 읽은 후 이어 보낸다. timeout/fault를 자동 reset이나 명령 누락으로 숨기지 않는다. 이 결함은 전송 계약상 실제 문제지만 이번 하드웨어 시험에서는 FIFO 고갈이 관측되지 않았다. 따라서 실물 찢어짐의 확정 원인으로 분류하지 않는다.

ARM/Unicorn 회귀 시험은 변경하지 않은 vendor C와 실제 adapter/clip C를 실행한다. O0/Os 각각15case·279assertions 통과. 움직이는 위쪽 경계와 SAVE/RESTORE 후 같은 clip 재요청을 포함한다. FIFO 시험은8192byte burst, 멈춘 소비자, 분할 헤더, 일반 레지스터 읽기, fault/timeout을 포함하며 O0/Os 각각20assertions 통과. GPU rasterizer나 물리 panel을 에뮬레이션한 시험은 아니다.

## 정차 시간

- 기본값: `speed < 5km/h`; 정확히5부터 주행 시간.
- 메뉴: Vehicle → Stop threshold,0..10km/h,1단위.0은 정확히0km/h만 정차로 계산한다.
- 저속 이동 거리는 계속 누적한다. 미수신·stale 시간은 unknown으로 남긴다. 설정 변경이 과거 누계를 소급 변경하지 않도록 이전 interval의 판정을 보존한다.
- 기존 설정 저장 수명과 동일한 RAM 세션 설정이다. 영구 저장 정책을 임의로 추가하지 않았다.
- ARM 시험:0..10설정×0..11속도,경계·범위 밖 거부·저속거리·설정변경·stale 검사. DATA_DEBUG0/1 및 O0/Os/Oz 통과. 설정 UI의 기본값·범위·표시·요청 검사도 통과.

실제 COM11/115200/8N1로 순정 CMD21 형식의13byte 프레임을 주입했다. SWD로 속도 모델을 직접 바꾼 시험이 아니다.

| UART 속도 | 정차 증가 ms | 주행 증가 ms | 거리 증가 mm | unknown 증가 ms |
|---:|---:|---:|---:|---:|
|0|5556|0|0|0|
|4|5558|0|6176|0|
|5|0|5537|7691|0|
|10|0|5590|15528|0|
|50|0|5616|78000|0|
|160|0|5537|246089|0|
|200|0|5542|307889|0|

## 실제 전환과 캡처

- 실제 ButtonEvents→UI 경로로 ENTER1회 뒤 UP/DOWN32회를 재생했다.31회 전환했다. 첫 UP은 기존 고장 난 위 버튼의 boot-held mask1 때문에 차단됐다. 입력 drop은0이었다. `exercise-v2`에 원자료가 있다.
- `exercise-v2/results.json`의 `keys_attempted`에는 도구 오류로 count 대신 request sequence가 기록됐다. 원자료를 덮지 않는다. 재생은32회 UP/DOWN(+ENTER1회)이며 `verification-summary.json`에 정정한다.
- 이전 DOWN120ms 중간 프레임의 DL은6088B, 최종 UP120ms 표본은4848B. 방향/표시 값이 다른 표본이므로 동일 pixel 부하에 대한 정확한 절감률이라고 주장하지 않는다. 최종 표본 FIFO wait/timeout/fault0, GPU/SPI 오류0.
- 최종 UP 중간 상태29.7FPS·CPU83.7%, 캡처 후 정상 트립29.9FPS·CPU70.8%. 연속 키 입력 중25.6FPS·CPU87.2%인 표본도 있었다. 모든 전환에서30FPS를 보장했다고 쓰지 않는다. 프레임 지연과 픽셀 왜곡은 서로 다른 현상이다.
- `final-capture/display.png`는 실제 EVE RGB565480×480.20stripe를1162ms에 SDRAM에 조립한 뒤113조각을CRC/generation검사하며 PC로 내려받았다. 다운로드 동안 정상 UI는 실행됐다. 앱은 실제UART73km/h를 수신하고 있었다.
- 실제 panel 전기적 scanout의 순간 장애는 정지한GPU snapshot으로 배제할 수 없다. 사용자 실물 재확인을 요청한 상태다.

## 설치·빌드·자원

최종 기준은 `candidate.elf`와 **`install-final/app.manifest.json`**이다. ELF SHA256 `afc8234f21ee1fe59a376be1666be9c7172627b3e5016da1c63d8c16a8789d96`; APP SHA256 `533453a2d49e51f6553c4310df345268fe5c62f092fc1a03dd7823f004e9b68f`. APP는0x08010000에서370212B. 전체 readback 일치, BL 보존, NOR/option byte 변경 없음.

Release/Debug 빌드와 예산 검사 통과. 기존 RWX LOAD segment linker warning은 남아 있다. Release flash free88540B, Debug73412B, SRAM35832B, CCM16320B. 힙/스택/큐 용량을 줄이지 않았다. Cube 재생성 복원 fixture에서 Product/Integrated/Graphics의 소스 및 C/C++ linker wrap 연결을 확인했다. 실제 GUI Cube 재생성 자체를 수행했다고 주장하지 않는다.

중간 `install-v2`는 build 종료 전 ELF 복사가 실패해 이전 후보를 재설치했다. 강제 종료하지 않고 안전하게 readback까지 끝낸 뒤, 최종 빌드 완료/해시를 확인해 `install-final`로 다시 설치했다. `install`/`install-v2`의 결과를 최종 후보의 증거로 혼동하지 않는다.

마지막에는 `drive-final`에서150초 주기의 정차→도심가속→주행→최대200km/h→감속→정차를100ms마다 실제 UART로 반복한다. 종료는 해당 폴더 `stop` 파일 생성이며 도구가 마지막0km/h를 보낸 후 포트를 닫는다. 현재 진행 상태는 `drive-final/status.json`을 확인한다.

캡처 후부터 마지막 주행 확인까지 유효RX프레임은6113→8638, ODO는36475→36478로 증가했다. 동시에 UART error는210→264, checksum error는2→7로 증가했다. overflow와 마지막HAL error는0, link_up/speed_valid는1이지만 배선/수신 경로가 무오류라고 주장하지 않는다. 이 오류의 전기적/타이밍 원인은 이번 렌더 수정으로 확정되지 않았다.
