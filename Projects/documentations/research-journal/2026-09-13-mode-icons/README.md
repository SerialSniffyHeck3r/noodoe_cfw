# 모드 아이콘과 선택 애니메이션 — 2026-09-13

Google Material Icons Round의8개 아이콘을 중앙 상단에 배치했다. 기존 시계·속도 링·거리 footer·구분선은 유지한다. 표시줄은 중앙 영역 안의Y105..140을 사용하며 페이지 제목은Y145부터 시작한다. 트립·폰·음악의 내부 세로 간격만 조정해 표시줄과 겹치지 않게 했다.

| 페이지 | 아이콘 | 표현 |
|---|---|---|
| 기본 화면 | home | 홈 |
| 트립 컴퓨터 | analytics | 통계 그래프 |
| 휴대전화 | smartphone | 폰의 배터리·알림 |
| 음악 | music_note | 미디어 |
| OBD | car_repair | 차량 진단 |
| 폰 원격 제어 | settings_remote | 리모컨 |
| 폰 GPS 궤적 | route | 이동 경로 |
| 환경설정 | settings | 설정 |

비선택16px/회색0x526269, 선택32px/청록0x48B6D0. 실제 glyph 잉크는 원본의 여백·비율에 따라 이 nominal 크기보다 작다. 중심은 화면X114/150/186/222/258/294/330/366,Y123으로 고정한다. 확대해도 주변 아이콘 위치는 밀리지 않는다. 잠긴 설정은32px로 선택 위치를 나타내면서 회색을 유지한다.

240ms cubic smoothstep을 페이지 전환과 공유한다.120ms에는 이전/다음 아이콘이24px이고 선택 가중치는128/127이다. 아이콘의 크기와 회색→청록 색상을 함께 보간한다. 표시줄은 고정하고 중앙 페이지 내용은 기존28px 슬라이드·페이드로 이동한다. 같은 페이지의 트립/알림 하위 항목 전환은 아이콘 선택을 바꾸지 않는다.

## 구현

- Graphics/UI의 Product_ModeIcons: 기존에 고정한 MaterialIconsRound OTF에서 추출한24px A4 ROM8개. 총2304bytes. footer SERV/연료 아이콘 enum·폰트·배치는 변경하지 않았다.
- Product_ModeStrip: 한 개의 고정 LVGL 객체와8개 image draw task. 렌더러가 매번 font/bitmap을 만들지 않으며 선택 변경으로 LVGL 객체를 생성하지 않는다. 정상 LVGL draw task 경로를 통해 EVE에 전달한다.
- Graphics/Port/graphics_image: 좁은24px A4 이미지 경로만 EVE L4로 캐시·bilinear scale·tint한다. 기존32px RGB565 앨범 아트2배 확대 경로를 유지한다. 다른 회전/피벗/포맷은 기존 driver에 전달한다. LVGL vendor 원본은 수정하지 않았다.
- g_mode_strip: magic0x4D4F4431,seq,active,current,target,elapsed_ms,weights[8]. 주소는 설치한 Release ELF에서 조회한다. 화면 진단 행은 다시 추가하지 않았다.
- Core/IOC/APP state logic은 변경하지 않았다. 새 소스는 기존 상대 include와Product 소스 정책으로 Cube 재생성 후에도 복원한다.

원본과 변환 기록은 프로젝트 Graphics/Assets/Icons/mode-manifest.json,
MODE_ICON_NOTICES.txt,MODE_ICON_LICENSE.txt에 있다. 공식 원본은
[google/material-design-icons](https://github.com/google/material-design-icons),
고정 commit은40a7a292a79d9394157e1ea24f83d52d5e17c556이다.

## 빌드와 장치

Release403484bytes, 여유55268bytes. SHA256:
`67f13813e8f2cde8129e4e32c7a6e48fc4abaa5cdde261cac7f91defe7856f01`.
Debug458640bytes, 여유112bytes. 두 구성0errors, 기존 RWX LOAD segment 경고1개.

Debug의 추가 자원이 기존 여유524bytes를 초과해, 새아이콘을4bpp로 저장하고 Product의 Graphics/Port7개 소스에-Os/-g3를 적용했다. 이 제한된 포트의 줄 단위/지역변수 디버깅에는 최적화 영향이 있다. 다른 프로필과 앱·BSP 정책은 유지하고 services_build.py가 해당 목록을 복원한다. 빌드 로그는 build-debug.log/build-release.log에 있다.

[설치 결과](install/summary.json)는 pass다. APP0x08010000부터만 기록, CLI verify와 전체 재읽기 일치, 순정 하위64KiB BL/config 보존, 두 차례 reset 뒤 HAL/RTOS/heartbeat 진행을 확인했다. debug freeze 원래값0을 복원하고 core를 실행 상태로 돌렸다.

[ARM 전환 시험](model-tests.log): 실제 product_mode_strip.c와 page_transition.c를 O0/Os에서 실행, 각각4574assertions 통과. 확대·색·모드 인덱스·화면 경계·빠른 연속 선택·객체 재생성 방지를 검사한다. 이 시험의 LVGL 호출은 stub이며 실제 EVE 출력 증거와 구분한다.
[기존 배치 검사](layout-tests.log)와 [Cube 프로젝트 동기화 검사](layers-tests.log)도 통과했다.

## 실제 캡처

아래 PNG는 EVE에서 읽은480×480 프레임버퍼다. 시험 내용은 TEST DATA로 표시하며 실제 폰 연결/주행 데이터가 아니다. 날짜와 시각은 기존 보드 RTC 값을 그대로 썼다.

| 장면 | 파일 |
|---|---|
| 홈 선택 | [화면](home/capture/display.png) |
| 홈→트립120ms | [화면](half-trip/capture/display.png) |
| 트립 선택 | [화면](trip/capture/display.png) |
| 폰 선택 | [화면](phone/capture/display.png) |
| 음악 선택 | [화면](music/capture/display.png) |
| GPS 선택 | [화면](gps/capture/display.png) |
| 설정 선택·잠금 | [화면](settings/capture/display.png) |

[픽셀 검사](pixel-audit.json)는 이전 승인된 shell과 중앙 바깥 픽셀을 비교하고,
같은 아이콘이 작음→중간→큼으로 실제 변하는지, 누락된 아이콘은 없는지,
전환 중간 RAM 가중치와 FPS/CPU/EVE 진단, 마지막 프리뷰 취소를 검사한다.
각 캡처 디렉터리에 원본 데이터·CRC·RAM 진단·명령 로그를 보관한다.

최종 검사는 pass다.7장 모두 중앙 영역 밖 변경0픽셀(정상 시계 갱신 제외),
아이콘의 작음→중간→큼과 청록/회색 구분을 확인했다. 실제 캡처 시점은
29.8~30.4FPS,CPU56.6~71.6%,최소 LVGL heap 여유12528bytes,
최대 EVE display list4020bytes, SPI 오류/그래픽 오류/경고0이다.
프리뷰 취소 뒤g_page_preview.active_id=0,모드0/홈 선택으로 돌아왔으며
기기는 정상 실행 중이다. 동작 영상이나 물리 유리 화면의 육안 검증으로
확대 해석하지 않는다.
