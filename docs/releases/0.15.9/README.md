# NewDoe 0.15.9 beta

App / firmware 0.15.9 · Android build 157 · Android 7.0 (API 24) or later

## 한국어

APK를 기존 앱 위에 설치하고 **기기 → 현재 상태 확인 → 설치 계속 / 언어 확인**으로 앱과 일치하는 본체 펌웨어·자원을 업데이트하세요. 기존 APK와 같은 패키지·서명으로 제공하며 앱 삭제는 필요 없습니다. 일반 설치에서는 별도 ZIP을 선택하지 않습니다. 이전에 끝나지 않은 작업이 있으면 실제 결과부터 확인하세요. 전문가용 ZIP은 실제 A/B 자원과 설치 언어에 맞아야 합니다.

- 통화 중 상단 전화 아이콘이 **1Hz: 0.5초 켜짐 / 0.5초 꺼짐**으로 깜빡입니다. 통화 종료 후 기존 Bluetooth·배터리 표시로 돌아갑니다.
- 전체 TBT의 두 번째 안내 칸은 비웁니다. 목적지까지 남은 거리·시간은 기존 하단 정보 줄에만 표시합니다. 현재 회전 카드에는 Google Maps·OsmAnd가 보내준 도로명을 거리 아래에 표시합니다. BRouter 경로 파일에는 도로명이 없어 해당 줄은 비워 둡니다. 미니 안내는 유지합니다.
- 안내 중 전체 TBT, HOME 지도 보기와 MAP의 하단 줄은 **남은 거리＋남은 소요시간**, **남은 거리＋도착 예정시간**을 10초마다 순환합니다. 이때 O 길게 누르기로 일반 정보 선택을 바꾸지 않습니다. 다른 HOME 보기와 안내 종료 후에는 기존 정보 순환을 사용합니다.
- 예: `102.4 km / 00'34"`는 34초, `102.4 km / 3:52'`는 3시간 52분입니다. 숫자의 위치는 고정하고 남은 시간의 `:`·`'`·`"`만 깜빡입니다. `102.4 km / 15:42`의 도착시간은 깜빡이지 않습니다. 숫자는 기존 하단 줄과 같은 D-DIN 서체·크기를 사용합니다.
- 내장 BRouter는 경로 엔진의 소요시간과 경로 진행량을 이용해 남은 시간을 갱신하고 휴대폰 현지 시각 기준 ETA를 계산합니다. 실시간 교통상황을 반영한 값은 아닙니다.
- 실험적 Google Maps 연동은 현재 안내 알림에 실제로 들어 있는 목적지 거리·소요시간·ETA만 사용합니다. Wear 앱은 필요하지 않습니다. 알림 형식·언어·Maps 버전에 따라 일부 값이 없거나 읽히지 않을 수 있으며, 확인되지 않은 값도 `--.- km / --:--`처럼 단위와 형식을 유지합니다(마일 설정은 `--.- mi`). 다음 회전까지 거리를 목적지 거리로 사용하거나 외부 ETA를 임의 계산하지 않습니다. OsmAnd의 추가 시간 정보는 이번 변경에서 새로 지원하지 않습니다.
- 재탐색·연결 대기·오래된 안내에서는 시간 값을 현재 안내처럼 표시하지 않습니다. 새 데이터 형식은 기존 NAV_DATA 전송에 넣고 기능 협상으로 구형 형식을 구분합니다.

기존 Gate·복구·Bluetooth 전송 방식·NOR 파일 이름과 배치는 유지합니다. 작은 시계 자원은 기존 자원 파일 내부에 저장하고 주행 중 표시 정보는 RAM에 보관합니다. 설정·TRIP·정비·사진·페어링을 정리 목적으로 지우지 않습니다.

실제 ARM 코드와 LVGL/EVE로 화면 및 전송 처리를 검증했습니다. 인천시청 지도·경로를 이용한 소프트웨어 표시 시험을 포함하며, 실제 휴대폰 Google Maps 알림·무선 연결·차량 LCD 확인은 이번에 하지 못했습니다. **기존 순정 복귀 구간의 전원 차단 제한은 여전히 남아 있습니다.** Play 배포용 서명키 이전을 완료했다고 주장하는 릴리스도 아닙니다.

[한국어 그림 설명서](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/Guide.md)

## English

Install the APK over the existing app, then use **Device → Check current status → Continue installation / check language** to install its matching firmware and resources. The existing APK package and signing certificate are preserved; uninstalling the app is unnecessary. Normal installation does not require selecting a ZIP. Reconcile unfinished operations first. Expert ZIPs must match the actual A/B resource profile and chosen menu language.

- During a call, the top phone icon blinks at **1 Hz: 0.5 seconds on / 0.5 seconds off**. The existing Bluetooth/battery indicator returns after the call.
- The second full-TBT card stays empty. Trip summary information uses the existing bottom strip only; the current maneuver card shows road names supplied by Google Maps/OsmAnd below the turn distance. BRouter route files contain no road names, so that line remains empty. Mini guidance is retained.
- During guidance, full TBT, HOME map and MAP alternate **remaining distance + travel time** and **remaining distance + arrival time** every 10 seconds. Long O does not modify the saved ordinary information selection there. Other HOME views and guidance-off behavior retain their ordinary information cycle.
- `102.4 km / 00'34"` means 34 seconds; `102.4 km / 3:52'` means 3 hours 52 minutes. Digits keep fixed positions; only duration separators blink. Arrival time, such as `102.4 km / 15:42`, stays steady. Numerals use the existing strip's D-DIN font and size.
- Built-in BRouter uses route timing and progress to update remaining duration; ETA uses the phone's local clock. This is not a live-traffic estimate.
- Experimental Google Maps integration reads only destination distance, duration and ETA actually supplied by the current guidance notification. No Wear app is required. Availability depends on notification layout, language and Maps version. Missing or unrecognized values retain the `--.- km / --:--` format (`--.- mi` in miles); turn distance is not substituted for destination distance, and external ETA is never guessed. This update does not add OsmAnd trip-time extraction.
- Recalculation, stale guidance and connection waiting clear the time display. The extension uses the existing NAV_DATA transfer with capability negotiation for older formats.

Gate, recovery, Bluetooth transport and NOR file names/layout remain unchanged. The small clock asset fits inside the existing resource file; live guidance remains in RAM. No cleanup of settings, trips, maintenance, photos or pairing is performed.

Verification executes actual ARM and LVGL/EVE code, including a software map/route scenario near Incheon City Hall. No live phone Google Maps notification, radio or vehicle LCD test was performed for this update. **The accepted legacy stock-return power-interruption limitation remains.** This release does not establish that the separate Play signing-key migration is complete.

[English illustrated guide](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/Guide.en.md)
