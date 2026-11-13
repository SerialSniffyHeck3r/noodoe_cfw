# NewDoe 0.15.12 beta

## 0.15.12 — MAP 전환·음악 로딩·정보 우선순위

앱 빌드 161과 같은 버전의 내장 펌웨어를 함께 사용합니다. 기존 앱 위에 설치하며 데이터 삭제는 필요하지 않습니다.

- MAP을 나가면 지도 표시와 본체 지도 캐시를 무효화하고 화면 교체가 완료된 뒤 다음 화면으로 전환합니다. MAP 밖에서는 지도 타일과 내장 안내 경로 좌표를 전송하지 않습니다. TBT 안내는 계속됩니다.
- 음악 화면은 TBT 다음으로 앨범 아트와 음악 글자를 먼저 받습니다. MAP 화면은 TBT와 지도 전송을 먼저 처리합니다. 이미 시작한 전송과 제어·GPS 명령의 처리 순서는 보호됩니다.
- 다음 곡 준비 중에는 로딩 아이콘을 표시합니다. 같은 곡으로 음악 화면을 다시 열면 보관된 그림과 글자를 재사용합니다. 재생 세션이 사라지면 짧은 전환 유예 뒤 Media Off를 표시합니다.
- 한 줄 AUTO는 **알림 → 통화 → 음악 → ETA** 순으로 우선 표시합니다. 해당 정보가 모두 없으면 일반 항목을 10초마다 순환합니다. 위/아래 길게로 잠시 다른 항목을 봐도 20초 후 활성 우선 정보로 돌아옵니다.
- 두 줄은 음악 재생 중 음악을 유지합니다. 음악이 없으면 아래는 선택한 TRIP, 위는 한 줄 AUTO의 우선순위를 사용합니다. 수동으로 선택한 일반 정보와 안내 중 MAP/TBT의 ETA 두 장 순환은 유지합니다.
- TRIP의 km/mi는 숫자 아래쪽 기준선에 맞췄습니다.
- 날씨에서 O를 길게 누르면 한 화면에 세 시간씩 표시합니다. 위/아래로 **+1~3 / +4~6 / +7~9 / +10~12시간** 네 화면을 넘기고 O로 돌아옵니다. 추가 API 조회 없이 이미 받은 예보를 사용합니다. Basic API의 3시간 자료는 실제 제공 시각으로 표시합니다.

- 설정 화면에서 언어 상태를 읽을 때 잘못된 명령 형식으로 요청이 반복 거절되던 문제를 수정했습니다. 읽기 실패는 설정 변경 실패와 구분하며, 설정 저장 명령은 자동 재전송하지 않습니다.
- 기기 화면은 확인된 실제 펌웨어 해시와 선택한 설치 파일을 비교합니다. 같으면 이미 설치됨, 다르면 업데이트를 표시하며, 아직 확인하지 않았다면 기기 상태 확인을 안내합니다. 미완료 설치·복구 안내가 우선합니다.
- 앱 오른쪽 위는 조작 버튼 대신 연결 상태만 표시합니다. 알림 영역에는 움직이는 업로드 화살표 대신 고정 NewDoe 아이콘을 사용합니다.
- 알림을 받을 앱 선택 화면 위에 **모두 선택 / 모두 해제**가 있습니다. 개별 선택도 가능하며 **저장**을 눌러야 적용됩니다. **취소**하면 이전 선택을 유지합니다. 이후 새로 설치한 앱은 따로 선택해 주세요.
- 나침반 방향은 본체에 설치된 언어를 따릅니다. 영어는 N/E/S/W, 대만 번체는 北/東/南/西를 사용하며, 대만의 TRIP A/B는 **行程 A/B**로 표시합니다.

Bluetooth 재연결·차량 접근 감지 정책은 이번 변경에서 제외했습니다. 집에서 반복 접속을 시도하는 문제를 해결했다고 주장하지 않습니다. Gate, NOR 파일 배치, 사용자 저장 데이터와 설치·복구 경로는 유지합니다.


본체 이미지는 실제 ARM/LVGL/EVE 코드의 소프트웨어 렌더링 예시입니다. 실차 LCD·무선 통신·소비전력 검증은 별도입니다.

## 0.15.12 — MAP transitions, music loading and information priority

Use app build 161 with the matching included firmware. Install over the existing app without clearing its data.

- Leaving MAP invalidates its display and device map cache, then waits for the display swap before entering the next view. Map tiles and native route coordinates are sent only while MAP is open. TBT guidance continues elsewhere.
- In Music, TBT is followed by artwork and music text. In MAP, TBT and map transfers have priority. Accepted transfers and control/GPS messages retain their ownership and processing guarantees.
- Track handoffs show loading. Returning to the same track reuses cached artwork/text. If the playback session disappears, a short handoff grace period precedes Media Off.
- One-line AUTO prioritizes **notification → call → music → ETA**. With none active, ordinary information rotates every ten seconds. Long UP/DOWN temporarily browses; active priority information returns after twenty seconds.
- Two-line HOME keeps music while playing. Otherwise the lower row shows the selected TRIP and the upper row uses one-line AUTO priority. Manual selections and the navigation-only ETA rotation in MAP/full TBT are retained.
- TRIP km/mi now align with the bottom of the distance digits.
- Hold O in Weather for three hourly columns per screen. UP/DOWN cycles **+1–3 / +4–6 / +7–9 / +10–12 hours**; O returns. This uses the already received forecast without an extra API query. Basic API data retains its actual three-hour source times.

- Fixed malformed language-status requests that repeatedly produced “Device rejected the request” in Settings. Read-only query failures are separate from setting changes; uncertain writes are never automatically resent.
- The device screen compares the observed firmware hash with the selected installation package. It shows already installed for a match, Update for a difference, and a status check when unknown. Unfinished installation/recovery notices take precedence.
- The top-right app area is a connection indicator, with no start/stop button. The notification area uses a static NewDoe icon instead of the animated upload arrow.
- The notification-app chooser now has **Select all / Deselect all**. Individual choices remain available. **Save** applies the draft; **Cancel** keeps the previous selection. Apps installed later must be selected separately.
- Compass directions follow the installed device language: N/E/S/W in English and 北/東/南/西 in Taiwanese Chinese. Taiwanese TRIP A/B labels now use **行程 A/B**.

Bluetooth reconnection and proximity detection changes are excluded from this release. The repeated connection attempts reported away from the vehicle are not claimed fixed. Gate, NOR file layout, saved user data and installation/recovery paths are unchanged.


Instrument images are software renders of the actual ARM/LVGL/EVE code. Physical LCD/radio/power validation remains separate.


Use the APK for normal installation. Choose the matching device/language package through the app; the expert ZIP is for a confirmed compatible device. Never uninstall the app to update it. The existing legacy stock-return power-interruption limitation is not fixed by this release. Keep vehicle battery power stable throughout installation. No physical hardware qualification is claimed.
