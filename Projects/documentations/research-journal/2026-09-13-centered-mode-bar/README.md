# 중앙 선택 아이콘 표시줄 (2026-09-13)

8페이지의 Material Icons Round 자원을 유지하고, 선택 아이콘이 항상 화면 중앙에
오는 원형 표시줄로 변경했다. 정지 상태에서는5개가 보인다. 선택은32px/흰색,
비선택은16px/회색이며 별도 잠금 색상은 없다. 실제 설정 페이지의 속도 제한은
App에서 그대로 적용한다.

다음 모드로 넘어가면 전체 표시줄이 왼쪽으로48px 이동한다. 이전 아이콘은
중앙에서 왼쪽으로 줄어들고, 오른쪽 아이콘은 중앙으로 들어오며 커지고 밝아진다.
이동/크기/색은 `ProductModeStrip_Update(mode, now_ms)` 한 함수가240ms cubic
smoothstep으로 진행한다.7→0도 같은 한 칸이다. 직접 페이지 이동은 짧은 원형
경로를 선택하고 반 바퀴 동률은 정방향이다. 중간에 새 요청이 오면 최신 요청을
병합하며, 같은 모드의 하위 트립/알림 변경은 표시줄 전환을 재시작하지 않는다.

렌더링 viewport는 X120..359/Y105..140이며 나머지 중앙 본문/시계/링/footer 좌표는
유지한다. 양 끝의 부분 아이콘은 viewport로 잘리고, 원형 배열의 재배치는 화면
밖에서만 일어난다. 이동 중 최대6개를 그린다. 기존 A4 ROM 마스크2304bytes를
EVE L4 캐시에 올려 재사용한다. 전환 중 객체/bitmap 생성이나 장치 I/O 대기는 없다.

## 코드와 자동 검증

- 렌더러/API: 프로젝트 `Graphics/UI/src/product_mode_strip.c`,
  `Graphics/UI/inc/Product_ModeStrip.h`. 호출부는 `dashboard_pages_view.c`다.
- `g_mode_strip`: current/target/active/elapsed와8개 가중치, 절대 화면X중심을
  SRAM으로 조회한다. 활성 중심X240, 인접 중심 간격48이다.
- 실제 Cortex-M4 C를 Unicorn에서 `-O0`, `-Os` 각각 실행해27,760개 단언 통과.
  8모드 순회, 역방향7↔0, 빠른 요청 병합, 동일 모드, 미초기화/잘못된 값,
  uint32 tick wrap, 클리핑 범위, 단조 이동/확대와 흰색/회색을 검사했다.
  LVGL은 이 시험에서 stub이며 실제 EVE 렌더링 증거와 구분한다.
- 실제 CubeIDE1.18.1 Product Debug/Release 빌드 성공. 기존 RWX LOAD linker 경고
  1개는 남아 있다. Debug458672bytes/여유80bytes, Release403516bytes/여유55236bytes.
  Debug 여유는 매우 작으며 다음 기능 추가 시 다시 크기를 확인해야 한다.
- 기존 폰트/배치 경계 검사 통과. IOC/Core/HAL/vendor LVGL 원본 변경 없음.
  `sources/`, `sources.json`, 두 manifest와 빌드/모델/배치 로그를 보관했다.

## 설치 검증

Release SHA-256:
`fe0086afda4bae204ea7a9dab754636e5fc82e37ca25d42d4beeeca4eecf7a36`

APP0x08010000에 기록하고 전체 APP 독립 읽기 대조 및 두 번 reset의 HAL/RTOS
진척을 확인했다. 순정 BL/기기 데이터 하위64KiB는 전후 동일하다.
보존 SHA-256:
`f38ed839c4fbce74d4d8227fcf13764194f405c4860cda75c628fce66064b574`

`install/summary.json`은 pass다. 옵션/RDP/NOR/RTC 변경은 하지 않았다.
실기기 캡처는 `capture_carousel.py`로 SRAM 프리뷰만 요청하고, 종료 시 취소한다.
TEST DATA가 표시된 본문은 시험용이며 실제 차량/휴대전화 수신 데이터가 아니다.
PNG는 실제 EVE framebuffer이며 LCD 유리나 광원을 찍은 카메라 사진은 아니다.


## 실제 EVE 캡처 결과

`pixel-audit.json` 최종 pass. 선택 아이콘의 최고 픽셀은RGB255/255/255이며 다른
아이콘도 중립 회색이다(RGB565 양자화 차이만 존재). 홈→트립120ms에서 홈 중심
216/트립 중심264, 크기24/24를 확인했다. 정지하면 선택 중심240/크기32다.
설정7→홈0도 같은48px 한 칸 경로이며120ms 중심216/264다.

| 캡처 | FPS | CPU | LVGL heap 여유 | EVE DL |
|---|---:|---:|---:|---:|
|[home](home/capture/display.png)|29.8|55.6%|14924|1620|
|[half-trip](half-trip/capture/display.png)|29.9|70.8%|13572|3864|
|[trip](trip/capture/display.png)|29.8|68.9%|13156|3548|
|[settings](settings/capture/display.png)|30.5|59.7%|13156|2144|
|[half-wrap](half-wrap/capture/display.png)|29.9|60.6%|13156|2428|

다섯 캡처 모두 중앙 영역 밖 변경0픽셀(계속 동작하는 시계의 숫자 제외),
렌더링/SPI 오류0, 경고0이다. 관측 중 누적 frame_slots_missed는2였으므로
단 한 프레임도 늦지 않았다고 주장하지 않는다. 표는1초 측정창의 실제 값이다.
반복 렌더링·캐시 채움·SWD 캡처가 포함되며 전체 제품의 모든 부하를 보증하지 않는다.

`normal/diagnostics.json`에서 프리뷰active_id0, 실제 카드0, 렌더러/앱ready를
확인했다. 기기는 일반 홈 화면으로 실행 중이며 시험 중 실제 페이지/트립/폰
캐시/저장값을 변경하지 않았다.
