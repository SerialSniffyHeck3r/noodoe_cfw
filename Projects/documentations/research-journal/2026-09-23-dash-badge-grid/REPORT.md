# 시계 구분선·PH9 표시·GPS 격자 수정

## 적용 결과
- 시계 구분선의 생성 좌표와 매 프레임 Scene 적용 좌표가 달랐습니다. Scene이 예전127,51 끝점을 다시 넣어22px 링과 겹쳤습니다. 이제 두 경로가 speed_home_separator의131,55 /349,55를 사용합니다. 둥근3px 선 끝까지 포함해 링 안쪽에 약1.78px 간격을 둡니다. 시계 위치/크기는 그대로이며 링이 사라질 때 화면 끝으로 연장하는 동작도 유지합니다.
- PH9 LOW+IGN ON에서 왼쪽38,224의32px 짙은 파란 원과24px Google Material Icons Round speed 아이콘을 지속 표시합니다. HIGH로 돌아오면 숨깁니다. 기존 자산을 재사용하며 새 이미지 패키지는 필요 없습니다. 일반 버튼 차단과release 재무장, 설치/긴급 복구 우선순위는 유지하고 버튼 입력에 따른Switch in Dash Mode 토스트만 제거했습니다.
- GPS 투영/표시 폭312→336px, 좌우12px씩 확장했습니다. 사방32px에서 격자만8단계로 옅어집니다. 같은 불투명도의 연속 구간은 합쳐 그리므로 중앙 실선은 여러 조각으로 중복 렌더링하지 않습니다. 지도 좌표/방향/줌/보간/축척은 유지하며 궤적·화살표·사진에는 페이드가 적용되지 않습니다.

## 캡처에서 발견해 함께 수정한 문제
첫 표시 시험에서 원형 배경만 나오고 계기판 그림이 누락됐습니다. 둥근 이미지 스타일이 clip_radius를 설정하여 기존EVE 렌더러가 이미지 작업을 거부하는 문제였습니다. 드라이버를 바꾸지 않고 Graphics/UI의DRAW_MAIN_BEGIN에서 배경 원을 먼저 그린 뒤, radius0의 일반A4 이미지 경로로 아이콘을 그리도록 했습니다. 최종 캡처에서 아이콘 픽셀122개와 graphics last_error=0을 확인했습니다. 초기 시험의 바이너리·캡처는trial-1에 보존했습니다.

## 검증
- Product Release/Debug 및 이미지 경계/자산/메모리 검사 통과. Core/IOC/vendor/드라이버 변경 없음. 보호 대상6493파일의 바이트 동일성을 확인했습니다. 실제GUI 재생성은 이번 작업에서 수행하지 않았습니다.
- PH9 입력 정책 기존ARM C 시험: O0/Os/Oz 각각187 assertions 통과.
- 최종 Product를 벤치에 설치하고 내부 플래시 전체를 독립적으로 두 번 읽어 비교했습니다. 순정 하위64KiB와Gate64KiB, FAT 메타데이터를 보존했습니다. 비활성APP 은행과 기록만 범위 제한하여 갱신하고 이전 유효본을 유지했습니다. 기존 자산은 다시 기록하지 않았습니다.
- 실제EVE480x480 캡처로 시계 간격, GPS 다크/라이트, PH9 배지를 확인했습니다. PH9 LOW는 디버거에서GPIO 읽기 직후의CPU 표본 레지스터만8회 주입했습니다. 물리GPIO/핀모드/펌웨어에는 시험값을 기록하지 않았습니다. 실제 스위치를 움직인 시험으로 간주하지 않습니다. 마지막에 실제PH9 HIGH/allowed=1, 디버그 워치독freeze=0과 배지 숨김을 확인했습니다.
- GPS 다크29.8fps/CPU75.9%, 라이트29.6fps/CPU76.6%. 해당 표본의 마지막 렌더17~18ms, 부팅 이후 최대18ms, GPU display list7088~7108bytes. RTOS 최소 여유23792bytes, CCM guard 오류0, 폴트0, 렌더러 오류0. 단기 표본이며8시간 부하 시험이 아닙니다.
- 최종 정상 화면 캡처1684ms. 모든 표시용 RAM 상태와 테마를 복원했습니다.
- 기존6.9.0 APK의 실제Bundle importer에서 최종ZIP 허용. ZIP 내 변경은cfw.bin과manifest.properties뿐이고, cfw.bin의Gate 부분도 동일합니다. Bootstrap/Diagnostic/Uninstall/자산/순정 이미지는 이전 배포와 같습니다.

## 자원
| 자원 | 최종 여유 |
|---|---:|
| Release APP |65,828B|
| Debug APP |34,144B|
| 일반 SRAM Release |54,448B|
| CCM |16,320B|

Release 사용량327,388B로 이전 배포보다428B 증가했습니다. 기존 힙/스택/통신 큐 및 여유 기준을 줄이지 않았습니다.

## 배포
기존Companion6.9.0/code14 APK를 그대로 재사용합니다. 주행 연동을 수동 종료하고 새ZIP을 선택하여 일반CFW 업데이트를 실행합니다. APK를 다시 설치할 필요는 없습니다. 편의를 위해 동일 APK와 대응ZIP을 함께 게시하고GitHub Latest/README를 갱신합니다.

현재 벤치의BT/조도 불량은 그대로입니다. 이 수정의SWD/표시 시험은 실차·무선 설치·물리 스위치 전 조합 시험의 성공을 뜻하지 않습니다.

- APK SHA256: `5f3c541ece295c1f3bc761017b1ac9924731e1a8611aed53fc8be77b30c2b1ce`
- ZIP SHA256: `372c5282ff28cc6acd814c019effe3e29c763ef5d2a0b490680a7134032a97c6`
- Product384KiB SHA256: `7dff550f35c2415e395eb4ec87319c390c0eab0b97d1cd9a5aaf5c8f925318ca`

## 실기 캡처
![차단 모드 표시](after-gps-dash-badge.png)
![GPS 다크](after-gps.png)
![GPS 라이트](after-gps-light.png)
