# 중앙8페이지 구현·실기기 검증 — 2026-09-13

요청한 고정8페이지와 공통 중앙 섹션 전환을 구현해 Noodoe에 설치했다. UART/FPS/CPU 화면 행은 제거했고 관련 서비스와 RAM 진단은 유지했다. 시계·속도 링·거리 footer·구분선은 승인된 배치 그대로다. 화면 자료는 EVE에서 읽은 실제480×480 프레임버퍼이며 카메라/물리 패널 관찰을 뜻하지 않는다.

## UI와 입력

ENTER 짧게: 빈 날짜 → 트립 → 휴대전화 → 음악 → OBD → 폰 리모트 → 폰 GPS → 설정 → 빈 날짜. OBD가 연결되지 않아도 페이지를 건너뛰지 않는다.

- 트립: UP/DOWN 짧게로 A/B/오늘/주유 후 선택. 이동/정차 시간, 총 시간, 수직 비율 막대, 최고·평균 속도, 거리. A/B에서 ENTER 길게는 취소가 기본인 리셋 확인이다.
- 알림: 폰별 배터리와 최신순 알림, UP/DOWN 탐색. 정확히50km/h는 탐색 가능하며 초과하면 최신1개로 고정한다.
- 음악: 앨범 썸네일, 제목, 아티스트, 재생 상태, 시간, 진행 막대.
- OBD: 미연결 시 NOT CONNECTED, 연결된 유효 PID는 기존 OBD 캐시에서 표시한다.
- 리모트: 기존 명시적 폰 연결/활성화 상태를 표시한다. 실제 폰으로의 송신 성공을 주장하지 않는다.
- GPS: 폰 GNSS의48점 north-up 궤적. 외장 GPS 장비를 추가하지 않았다.
- 설정: ENTER 길게 진입,3km/h 이상 회색/진입 금지. 속도 미확인은 알림·설정 모두 잠근다.

UP/DOWN 길게의 기존 footer 탐색과 저연료 잠금은 유지한다. 실제 ENTER 핀은 PA15이며 고장난 UP 매핑을 바꾸지 않았다.

## 구조와 전환

앱 정책/계산/캐시는 [App_Logic/UI](../../STM32CubeProjects/FuckNudo_Noodoe_CFW_Project/App_Logic/UI/PAGES.md), LVGL 배치와 전환은 Graphics/UI에 있다. Core task의 LCDTest 단일 호출, IOC, HAL/LVGL vendor 원본은 변경하지 않았다.

Page_Transition는240ms cubic smoothstep과28px 이동을 공유한다. 기존 섹션 전체가 왼쪽으로 이동하며 희미해지고 새 섹션은 오른쪽에서 들어오며 선명해진다. 두 고정 슬롯을 재사용하고 중앙 x72..407/y105..375에 clip한다. 문자/선/막대/이미지별 alpha로 처리해 EVE 미지원 opacity layer를 만들지 않는다. 트립 하위 모드와 상위 메뉴도 같은 전환을 쓴다.

변경 없는 표시 모델의 반복 Bind를 생략해 초기 트립 화면의 약21FPS 문제를 수정했다.32×32 RGB565 앨범 아트의2배 확대는 프로젝트 전용 EVE linker wrapper로 bitmap 원점/coverage를 교정했다. 다른 이미지 경로는 원본 구현으로 전달한다. 새 썸네일은 동일 RAM_G 블록에 갱신한다.

## 실제 검증

- [최종 설치 결과](install-final/summary.json): APP0x08010000부터만 기록, CLI verify 및 독립 전체 재읽기 일치, 순정 BL/config 하위64KiB 보존. 두 차례 재부팅 후 HAL/RTOS/heartbeat 진행, fault 없음. DBGMCU freeze 원래값0 복구, MCU 실행 상태.
- Release400060bytes, 여유58692bytes, SHA256 `e099822cafd88d71456e283a59fabf4f25abaaa36df4360097ae5ff15bc18bb2`.
- Debug458228bytes, 여유524bytes. 두 구성0errors, 기존 RWX LOAD segment 경고1개. Debug 기능별 최적화 예외는 PAGES.md/빌드 동기화에 기록돼 있다. [빌드 manifest](builds.json).
- 실제 Cortex-M4 모델 시험: 새 페이지/적분/주유/폰 캐시/지도/전환/프리뷰7종×874assertions를 O0와 Os에서 통과. [시험 로그](page-tests.log).
- 기존 UI19종×110605assertions + 속도416assertions를 O0/Os에서 통과. [회귀 로그](state-tests.log).
- 실제 입력 소비 경로11종×74assertions를 O0/O2에서 통과. [입력 결과](input-tests/results.json).
- 프로젝트 동기화/상대 include/APP linker/프로필/멱등성/Core·IOC 보존과 기존 폰트·배치 검사를 통과. [계층 검사](layers-test.log), [배치 검사](assets-test.log).
- [실제 캡처 픽셀 검사와 페이지별 성능](pixel-audit.json): 중앙 밖 변경/앨범 아트/120ms 전환 위치·alpha/프리뷰 해제를 검사한다.

최종 픽셀 검사는 통과했다. 실제 시계 숫자 갱신을 제외하면 중앙 밖 바뀐 픽셀은10장 모두0개다. 전환120ms의 제목은 오른쪽14px/실측 명도49.8%이며 RAM alpha127과 일치한다. 앨범 썸네일은1024색이 실제 렌더링됐다. 페이지별29.8~30.6FPS, CPU53.9~69.4%, LVGL heap 여유12720bytes, 가장 큰 EVE display list3444bytes였다. 프리뷰 취소 후29.9FPS/오류0/active_id0으로 정상 실행하며 사용자가 선택한 음악 페이지의 실제 미연결 안내로 돌아갔다.

초기 설치의 CLI verify 오류는 독립 전체 재읽기 두 파일 모두 기대 이미지와 완전히 일치하여 당시 오검출로 분류했다. 이후 수정본인 install-final은 CLI verify까지 통과했다. 이전21FPS와 앨범 아트 누락은 blank/trip/music 디렉터리의 초기 증거이며 최종 결과와 구분한다. final-music은 버튼 입력이 프리뷰를 취소해 음악 캡처로 채택하지 않았고 music-verified로 다시 검증했다. 그 뒤 입력 횟수116이 유지된 캡처만 사용했다.

## 채택한 화면

| 페이지/장면 | 실제 프레임버퍼 | 데이터 성격 |
|---|---|---|
| 빈 날짜 | [display.png](final-date/capture/display.png) | 실제 RTC 날짜 |
| 트립 A | [display.png](final-trip/capture/display.png) | TEST DATA 표기된 RAM 프리뷰 |
| 휴대전화 | [display.png](final-phone/capture/display.png) | TEST DATA 표기된 RAM 프리뷰 |
| 음악·앨범 아트 | [display.png](music-verified/capture/display.png) | TEST DATA 표기된 RAM 프리뷰 |
| OBD | [display.png](final-obd/capture/display.png) | 실제 미연결 상태 |
| 폰 리모트 | [display.png](final-remote/capture/display.png) | 실제 미연결 상태 |
| 폰 GPS | [display.png](final-gps/capture/display.png) | TEST DATA 표기된 RAM 프리뷰 |
| 설정 | [display.png](final-settings/capture/display.png) | 실제 속도 미확인 잠금 |
| 전환 중간120ms | [display.png](final-fade/capture/display.png) | 개발 전환 freeze, 실제 렌더링 |
| 프리뷰 해제 후 | [display.png](final-normal/capture/display.png) | 실제 사용자가 선택한 페이지 복귀 |

## RAM 관찰

주소를 고정해서 쓰지 않고 설치된 Release ELF의 symbol을 사용한다. g_product_ui는 앱 상태/입력/관측 effect, g_product_trips는4개 계측 레코드, g_graphics/g_graphics_performance는 SPI/EVE/heap/FPS/CPU, 기존 UART 서비스 진단은 통신 상태를 담는다. g_dashboard_view의 alpha/elapsed_ms는 active=1일 때 진행 중 전환을 뜻한다. active=0에서 alpha는 숨겨진 슬롯의 이전 값이다.

g_page_preview는 개발 빌드의 SRAM mailbox다. TTL과 명시적 취소가 있으며 실제 버튼도 즉시 취소한다. 프리뷰는 제품 UiState·트립·폰 데이터·저장소를 바꾸지 않는다. 마지막에는 cancel로 시험 표시를 해제하며 사용자가 선택한 실제 페이지를 보존한다. observe 스크립트/주소/원본 SRAM과 CRC 검증 자료는 각 캡처 디렉터리에 남겼다.

## 남아 있는 실제 연동 한계

1. 트립 A/B/오늘/주유 후 레코드는 현재 RAM 계측이다. 재부팅하면 초기화되며 영구 이력 저장은 이번 구현에 포함하지 않았다. 기존 오일 사용 시간 저장은 유지한다.
2. 주유 감지는 정규화된 연료 칸이2칸 이상 상승하고3초 안정되면 한 번 발생한다. 순정 UART 원시값은0/1칸만 확인돼 있어2..5칸 매핑을 추측하지 않았다. 실제 주유 자동 리셋은 해당 매핑 검증이 더 필요하다.
3. 두 폰의 generation token/메타데이터 수신 API와 GPS 연결점은 마련했다. 손상된 BT donor에서 실제 무선 수신·멀티 연결·폰 리모트 성공을 검증한 것은 아니다. companion payload 디코더와 송신 어댑터가 연결돼야 실제 데이터가 표시된다.
4. 폰트는 기존 승인된 ASCII subset이다. 아직 포함되지 않은 한글/이모지는 대체 문자로 표시하며 지원한다고 주장하지 않는다.

날짜 캡처의2016.5.10은 현재 보드 RTC에서 읽은 값이다. 이번 화면 작업에서 RTC를 PC 시간으로 설정하지 않았다.

최종 설치 이후 바뀐 코드는 오해를 부르던 OBD gating/거리 API 설명의 주석뿐이다. 실행 로직은 검증된 이미지와 같다.
