## 최신 홈/전원 배경 (2026-09-16)

- IGN OFF 뒤 첫1000ms는 밝기까지 포함한 직전 프레임을 그대로 유지한다. 배경 shade/사진 GPU 쓰기, 화면 재구성, 절전 정책 변경, 세션 종료를 하지 않는다. 대기 종료 시 SESSION_END를 한 번 확정하고 주행 snapshot을 만든다. 그때부터 선택 사진 전환·20% 밝기로240ms fade·현재 위치에서 링400ms exit·요약24px rise/240ms fade를 함께 시작한다. 링 scanout을 추가로 기다리지 않는다. 1초 이내 ON 복귀는 같은 주행 세션/배경을 유지한다. 요약은 확정 후5초이며 전환 완료 후 AWAKE 프레임을 유지한다.
- 이미지 휘도 기반 자동 조절은 제거했다. ON 정보 화면20%, 음악32%, 홈 사진100%에 사용자 중앙 밝기 설정을 적용한다. OFF 전환/대기와 Ride Summary는20%다. 첫1초 동안은 기존 밝기를 전혀 변경하지 않으며20% 요청은 종료 확정 이후에만 시작한다. 음악앨범은IGN ON 음악에만 사용한다. 강제OFF사진 선택은 저장된 wallpaper enabled 설정을 변경하지 않는다.
- 홈 UP/DOWN은 사진만/날짜/날짜+UART속도3단계 순환한다. 날짜는RTC Gregorian 결과의영문 월약어·자연 일·요일, 예 Sep. 16 Wed다. 사용자의Thu는포맷 예시이며2026-09-16을Thu로하드코딩하지않는다. 숫자는D-DIN64 고정폭,km/h·mph는Lato24 고정위치,미수신은---다. DATA_DEBUG 시작카드는홈0이다. 실제UART/날짜는placeholder로대체하지않는다.
- 홈 아이콘 strip은 주카테고리 변경5초 후240msfade로숨긴다. 하위모드·속도값 갱신은타이머를리셋하지않으며 다른카테고리는다시표시한다. shell가시성과idlealpha를분리한다.
- Ride Summary의Dist./Time/OIL은동일X114·168·338 고정prefix/value/unit칸이다. 값D-DIN48,설명Lato20,잉크하단246/302/358. 거리자연소수1자리,시간H:MM,OIL자연정수%,미확인은--.24px상승/240msfade는그룹전체공유한다.
- 사진은순정album을보존한별도root WALL0.JPG..WALL2.JPG가우선이다. PhotoImport ABI2의slot|0x100은해당override를create-only로생성한다. 완전JPEGdecode검증은출력tile을버려현재pixels를절대로수정하지않고,읽기대조후명시적재부팅에서만활성화한다. 기존파일덮어쓰기/삭제/포맷은하지않는다. tools/wallpaper_install.py는전체백업+검증journal의현재기준·ARM정확쓰기계획·실기기모든변경영역preimage/readback을필수로한다. 추가사진교체는새로운버전저장정책이필요하며현재한번생성경로를overwrite로완화하지않는다.

# 중앙8페이지 계약 (2026-09-13)

속도 링·시계·거리 footer는 고정 shell이다. 모든 페이지와 상위 메뉴는 x72..407/y105..384 안에서만 그린다. UART/FPS/CPU 진단 라벨은 생성하지 않으며 g_product_ui, g_graphics, g_graphics_performance 및 UART 서비스 진단은 RAM에 유지한다.

중앙 상단X120..359/Y97..136은8모드 원형 아이콘 표시줄이다. 정지 시 선택 아이콘과
양옆 두 개씩 총5개가 보인다. 선택은 중앙X240/Y117/36px/흰색, 나머지는22px/회색이다.
다음 선택 시 전체 표시줄이 왼쪽으로48px 이동하며 오른쪽 아이콘이 중앙으로 들어온다.
ProductModeStrip_Update(mode, now_ms) 하나가240ms cubic 보간으로 위치·크기·색을
동시에 진행한다.7→0도 한 칸이며 직접 이동도 정방향으로만 순환한다. 하위 트립/
알림 탐색은 같은 아이콘을 유지한다. 내용 제목은Y145부터이며 기존 섹션 전환과
동일 시계를 사용한다. 잠긴 설정도 선택 아이콘은 흰색이지만 실제 설정 페이지의
회색/진입 금지는 유지한다. g_mode_strip에 선택·목표·진행 시간·8개 선택 가중치와
실제 화면X중심을 남긴다. 일반 화면에 진단 숫자는 추가하지 않는다.

Product Debug는 새 product_mode_strip 및 Graphics/Port의7개 기존 소스를 EVE
vendor와 같은-Os/-g3로 빌드한다. 신규 자원 추가 전 Debug 여유가524bytes였기
때문이다. Release 및 다른 프로필의 최적화 정책은 그대로며, 해당 포트의 줄 단위
디버깅/지역 변수에는 최적화 영향이 있다. 고정 목록은 services_build.py가 복원한다.

| 번호 | 모드 | 내부 입력/데이터 |
|---|---|---|
|1|빈 화면|중앙 아래에 UTC+09 Gregorian 날짜/요일만 표시|
|2|트립|UP/DOWN 짧게: A/B/오늘/주유 후. ENTER 길게: A/B 리셋 확인|
|3|휴대전화|배터리/충전/최신순 알림. UP/DOWN으로 이전/다음. >50km/h 또는 속도 미확인 시 최신1개로 고정|
|4|음악|앨범 아트·곡·아티스트·재생상태·경과/전체 시간·진행 막대|
|5|OBD|항상 존재. 미연결 시 NOT CONNECTED. UP/DOWN으로8개 PID|
|6|폰 리모트|ENTER 길게 활성화,3초 길게 종료. 실제 송신 effect 어댑터는 아직 UNSUPPORTED|
|7|폰 GPS|primary phone의 기존 GNSS 캐시로 north-up breadcrumb. 외장 GPS는 사용하지 않음|
|8|설정|ENTER 길게 진입. >=3km/h 또는 속도 미확인 시 회색/진입 불가|

ENTER 짧게는1→8→1 순회. 연결/해제가 페이지를 없애거나 현재 화면을 바꾸지 않는다. UP/DOWN 길게는 기존 거리 footer를 변경하며 RESV 잠금은 유지한다. 부팅 때 잡힌 버튼/중복 SHORT/해제 이벤트 억제는 기존 입력 reducer를 사용한다. UP 하드웨어 고장을 이유로 핀 이름을 바꾸지 않는다.

## 재사용 전환

Graphics/UI/Page_Transition는 장치/앱을 모르는 순수 시간 모델이다.240ms cubic smoothstep,28px 이동, 두 고정 슬롯, 최신 요청 병합. 나가는 전체 섹션은 왼쪽으로/alpha255→0, 들어오는 섹션은 오른쪽에서/alpha0→255. Dashboard_PagesView는 문자/막대/선/이미지 각각에 alpha를 적용한다. EVE가 지원하지 않는 전체 객체 opacity layer를 만들지 않는다. 신규 페이지는 DashboardPage 모델과 Bind 배치만 추가하고 전환 엔진을 재작성하지 않는다. 탐색 도중 heap 할당/객체 재생성을 하지 않는다. 일반 시간 갱신은 전환을 다시 시작하지 않는다.

## 계산과 유효성

Trip_Computer는 네 독립 레코드를 가지고 IGN ON+신선한 속도만 적분한다. 속도[km/h]×시간[ms]×5/18=거리[mm], 정수 잔여를 보존한다. ODO를 이 추산값으로 덮어쓰지 않는다. moving+stopped가 총 측정시간이고 평균은 정차를 포함한다. >1500ms 간격/끊김은 unknown_ms/partial로 기록한다. RTC 날짜가 바뀌면 TODAY만 초기화하며 날짜 미확인은 TODAY의 확정값으로 표시하지 않는다. 순정 UART 연료0x50/0x51은0/1칸만 검증돼 있으므로2..5칸을 임의로 매핑하지 않는다. 정규화된0..5칸 값에서3초 연속 안정된2칸 이상 상승 시 refuel 레코드를 한 번 초기화한다. 확정된 첫 주유 전에는 주유 후 거리를 모르는 값으로 표시한다.

현재 레코드는 RAM이며 재부팅 시 초기화된다. 'Measured this boot'는 영구 저장 완료를 의미하지 않는다. 저장소 게이트/순정 FAT/NOR 영역은 이번 UI 작업으로 변경하거나 포맷하지 않았다. 기존 OilUsageService 영구저장 계약은 유지한다.

## 상위 앱 API와 실제 연결 범위

ProductUI_PhoneToken(slot)으로 현재 연결 세대 토큰을 얻고 ProductUI_PublishPhone/PublishMusic(slot,token,snapshot,now_ms)에 전달한다. 두 폰의 캐시/토큰을 분리하고 끊긴 이전 세대 데이터는 거부한다. ProductUI_SelectPhone으로 표시 소스를 선택한다. task-context의 bounded copy이며 장치 I/O/대기를 숨기지 않는다. 알림6개/폰, UTF-8 미지원 글리프는 ?로 축약, 고정박스 안에서만 표시. 현재 폰트는 ASCII subset이다. 배터리/알림120초, 음악15초 freshness. 음악 thumbnail은 RGB565 little-endian32×32, 표시64×64. companion이 축소한 썸네일을 넘기며 이미지 변경은 기존 EVE RAM_G 블록에 갱신해 cache를 누수시키지 않는다.

실제 손상된 donor BT 연결과 companion payload 디코더/송신 어댑터 완료를 UI 수신 API의 존재와 혼동하지 않는다. 음악/리모트 effect는 기존 명시적 UNSUPPORTED 처리이며 실제 폰 제어 성공으로 보고하지 않는다. GPS는 기존 Runtime_GetGnss의 PHONE source만 받는다.48점 경로, dateline wrap/cos(latitude) 보정,10초 이상 끊김은 새 segment, 가득 차면 이전 점을 절반 간격으로 압축한다.

## 검증/디버그

g_page_preview는 GRAPHICS_DEV_VIEWPORT에서만 처리되는48byte SRAM mailbox다. request_id를 마지막에 쓰며 card0..7,selection0..7,flags1=TEST DATA,flags2=전환120ms에서 freeze,ttl1000..180000ms. card=UINT32_MAX 취소. 실제 버튼도 즉시 취소한다. 실제 UiState/트립/캐시/저장값을 수정하지 않는다. g_dashboard_view는 전환/프레임/alpha를 읽는 진단이다. 프리뷰를 실제 수신 데이터라고 보고하지 않는다.

Cube 재생성은 tools/services_build.py가 새 렌더러 소스 선택과 Product Debug의 새 기능 파일 -Os/-g3를 복원한다. Core의 단일 LCDTest 호출, IOC, vendor LVGL 원본은 수정하지 않는다. tools/tests/dashboard_pages/run.py는 실제 Cortex-M4 C를 O0/Os에서 실행하고 전환·주유·적분·threshold·두 폰·GPS·프리뷰를 시험한다.

최종 실기기 캡처는 Reversing/analysis/2026-09-13-eight-pages/README.md에 있다.29.8~30.6FPS, CPU53.9~69.4%, 중앙 밖 변경0픽셀(시계 시간 갱신 제외), 전환120ms의14px/alpha127 및 음악 썸네일 표시를 확인했다. 프리뷰는 취소했고 사용자 페이지 선택을 보존했다. 시험값을 실제 폰/차량 수신값으로 해석하지 않는다.


실기기1차 검증에서 트립21FPS가 관측되어 변경 없는 DashboardPage는 정확한 byte 비교로 Bind를 생략하도록 수정했다. outgoing 슬롯은 frozen이고 incoming/current 슬롯 한 개만 갱신한다. 같은 구조체에 시간값이 없으므로30FPS와 무관한 owner task 반복이 스타일 갱신을 발생시키지 않는다. EVE의2배 확대32×32 RGB565 썸네일은 clip 원점 대신 이미지 원점과64×64 bitmap coverage를 사용하는 프로젝트 전용 linker wrapper를 적용했다. 다른 회전/피벗/크기/색/마스크 경로는 upstream으로 전달한다. Product Debug에는 새 Graphics/Port/src/graphics_image.c도-Os/-g3로 포함한다. GUI 재생성 뒤 Product에서만 이 wrapper flag가 복원된다. caller now_ms는 MCU의 동일 단조 시계이며 폰 wall-clock을 넘기지 않는다.


중앙 선택 아이콘 후속 검증은 Reversing/analysis/2026-09-13-centered-mode-bar에
보관한다. 실제 EVE의 정지/120ms/7→0 캡처에서X240 선택,48px 간격, 흰색/회색을
확인했다.29.8~30.5FPS, 오류/경고0, 시계 숫자 갱신 외 중앙 밖 변경0픽셀.
ProductModeStrip_Update 하나가 위치/크기/색을 함께 진행하며 프리뷰는 취소했다.


## XMB형 탐색 후속

ENTER 짧게는 다음 카테고리만 선택한다. UP/DOWN은 항목을 양방향으로 이동한다.
두 축과 아이콘은 PageTransition_Ease의240ms Slow–Fast–Slow를 공유한다. 같은
카테고리는 세로, 다른 카테고리는 가로로 전환한다. 본문 viewport는X72..407/
Y145..384로 별도 분리해 위로 빠지는 제목도 아이콘 바를 침범하지 못한다.
트립 시간은H:MM, 그 아래 파랑 Moving/옅은 노란 흰색 Stopped의 알약 비율 바,
거리, 위 화살표 최고속도 정수/원사선 평균속도 소수1자리다. 별도 데이터 생산자와
PagePreview version2 계약은 DATA_PATHS.md가 위의 옛 flags1 설명을 대체한다.
# Trip layout refinement,2026-09-14

The renderer no longer displays the trip quality/status note (Partial / telemetry
gap, Waiting, etc.). TripComputer still retains validity and gap information; no
unknown field becomes a fabricated zero. The source selector remains independent
of the page selector. Speed and odometer always come through the real UART path.

The central viewport now ends atY384, with all four corners inside the speed ring.
Moving duration uses D-DIN40, distance/max/average D-DIN36, labels/units Lato20.
Numeric font reference bottoms align atY215/313/376. Digit boxes keep fixed
placement and width even when the current string loses rounded6/8 overshoot.
Never shift the baseline according to the current value (97.6/97.7/97.8 regression).
The old caption area is part of the trip's expanded layout. Clock/footer/divider
geometry is unchanged.

TRIP A/B entry shows a3-second `Hold O to reset` toast. See POPUPS.md for the
nonblocking API, timeout/priority rules and separate renderer ownership.
