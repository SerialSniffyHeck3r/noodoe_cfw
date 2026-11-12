# NewDoe 0.15.10 beta

App build 158 and firmware 0.15.10 are a matched pair. Install the APK over the existing NewDoe app, without clearing its data. In Device, select the matching included package and review the proposed firmware update. Existing APK signing identity and package name are retained. This GitHub release does not complete the separate Google Play signing-key migration.

## Daily use

- The app connects automatically to the selected Noodoe. There is no riding-link switch. Updates and settings operations hand the existing connection over to the appropriate task, then reconnect. Do not operate the phone while moving.
- HOME has one/two-line information, speed, the selected clock, navigation and weather. The duplicate HOME map is removed; use the MAP page. In the one/two-line modes, hold O to switch manual/automatic information. Hold UP/DOWN to select information; short presses select the HOME view. Weather retains its forecast controls.
- MAP always includes the information strip. During navigation, it alternates remaining distance/travel time and remaining distance/arrival time every ten seconds; otherwise it uses the ordinary AUTO information cycle.
- Only the current TBT instruction is displayed. External Google Maps/OsmAnd road names remain supported when supplied. Mini TBT warning flashes only its distance digits yellow. Time separators blink without moving the numerals; minute/second marks sit near the digit tops and colons remain centered.
- Navigation zoom uses speed and distance to the turn, while retaining bounded map detail. The phone map shows current position even before a route is started.
- Changed weather data is delivered to Noodoe without waiting for the weather page. Network fetches still follow the weather-page, one-hour/30 km policy. The weather area shows a loading indicator and update age below the blended forecast bands.
- Media artwork remains cached across page changes. Artwork loading uses the shared loading icon. No active media displays the power icon and Media Off. Audio and message text rendering preferences are independent.
- The phone summary shows elapsed call time during a call. Trip distance is larger; sunrise/sunset use warm/cool colors. Auto-brightness quick adjustment changes the automatic level.

## Removed controls and clearer states

The footer has only ODO and fuel reserve. Alternate ignition-off pictures, the ignition-off badge, rider-name welcome, the second TBT instruction, and motion-driven page return restrictions are removed. Ignition off keeps a gray ring and does not count as stopped trip time. Settings that are no longer exposed can retain compatibility bytes in old data; they no longer activate the removed functions.

Low fuel is based on a fresh valid UART sample after ignition, rather than replaying the previous session warning. A stable rise from zero to one bar for five seconds clears reserve and returns to ODO. The low-fuel warning is static with only the fuel icon blinking. Vehicles without usable fuel data can disable fuel recognition in settings.

The setup wizard clears its old controls before showing the localized restart screen. Installation and font-preparation progress have stronger contrast. Font startup reads are batched. Gate, its recovery ABI and the NOR file layout are unchanged.

## Diagnostic firmware and return

After a successful normal installation/update, the app prepares the Diagnostic image in the existing inactive firmware slot. No new NOR file or FAT allocation is required. If preparation fails, normal firmware remains installed and the app reports that diagnostics are not ready; it does not silently claim success.

Once prepared, select Diagnostic firmware in the unit's system settings, or use the app's diagnostic entry. The current normal image and its exact hash are retained. The existing Gate installs the diagnostic image. Its screen uses UP/DOWN to select and O to run an item. The default item is **Return to normal firmware**.

- Return to normal firmware verifies the saved normal image before handing it to the unchanged Gate. It returns to that same normal version and language, preserving settings, trips, maintenance, photos and pairing.
- NOR backup is available only in Diagnostic. Select it on Noodoe with ignition off; the connected app receives the request and saves the backup to the phone. Log export can also be requested on Noodoe. Interrupted requests are reported and are not blindly replayed.
- Bluetooth start, pairing, controller reset, light-sensor checks and live status remain diagnostic tools. Back to stock is a separate explicitly selected recovery action, not the normal return button.
- Returning does not restore stale phone-side settings snapshots. RAM-only music, navigation and weather are supplied again when the phone reconnects.

Keep permanent power connected during firmware switching. Software reset/power-cut injection covers the existing Gate copy path and verified return. A cut before Diagnostic commits its return intent can leave the existing Gate waiting for recovery. This release does not claim universal power-loss qualification, or repair the historical stock-return interruption limitation.

## Validation and files

The release includes the matching APK, default B-English installer, A/B language installer archive, Korean/English illustrated manual, licenses, LGPL relink objects, software-verification record and SHA-256 checksums. Most users should select language/profile through the app; standalone ZIPs are for the expert path.

Validation uses actual ARM firmware logic and LVGL/EVE display lists, Android regression tests, matching embedded package hashes, and unchanged Gate comparison. It is software verification, not a claim of a physical LCD, Bluetooth or road test. External-app TBT remains experimental and uses only fields those apps actually supply.

---

# 한국어 안내

앱 빌드 158과 펌웨어 0.15.10은 한 쌍입니다. 기존 앱을 삭제하거나 데이터를 지우지 말고 APK를 덮어 설치한 뒤, 기기 화면에서 내장 패키지와 예정 작업을 확인하고 업데이트하세요. 기존 APK 서명과 패키지명은 유지합니다. Google Play 서명 이전은 이 GitHub 배포와 별개입니다.

- 앱은 선택된 기기에 자동 연결합니다. 주행 연동 스위치는 없으며 업데이트·설정 작업이 연결을 순서대로 인계받습니다.
- HOME 지도는 제거하고 MAP으로 통합했습니다. MAP 하단에는 안내 중 ETA 두 항목을 10초마다, 안내 밖에서는 기존 AUTO 정보를 표시합니다.
- HOME 1줄/2줄 화면에서 O 길게로 수동/자동을 바꾸고 UP/DOWN 길게로 정보를 선택합니다. 짧게 누르는 화면 전환과 날씨 예보 조작은 유지합니다.
- 두 번째 TBT는 제거했습니다. 미니 TBT는 거리 숫자만 노란색으로 깜빡입니다. 시간 숫자의 위치는 고정하고 구분기호만 깜빡이며, 작은따옴표·큰따옴표는 숫자 윗부분에, 콜론은 세로 중앙에 배치했습니다.
- 날씨 변경분 전송, 로딩 표시, 하단 갱신 시각과 부드러운 예보 색상, 앨범 아트 캐시·로딩·Media Off, 음악/알림별 렌더링 설정, 통화 시간, 트립 숫자 크기와 밝기 자동 레벨 조작을 반영했습니다.
- 하단 ODO/RESERVE만 남기고 시동 OFF 전용 배경·배지, 라이더 환영 문구, 속도에 따른 화면 강제 복귀를 없앴습니다. 시동 OFF는 정차 시간으로 누적하지 않습니다.
- 연료 경고는 시동 후 새 유효 UART 표본으로 판단합니다. 0칸에서 1칸으로 5초간 안정되면 RESERVE를 해제합니다. 초기 설정 완료 화면의 잔상과 설치 진행바도 수정했습니다.

정상 업데이트 후 앱은 기존 비활성 펌웨어 슬롯에 진단 펌웨어를 준비합니다. 준비가 끝나면 본체 시스템 설정이나 앱으로 진입할 수 있습니다. 진단 화면의 기본 항목 **일반 펌웨어로 돌아가기**를 선택하면 진단 전과 동일한 일반 펌웨어·언어로 돌아오며 설정·트립·정비·사진·페어링을 보존합니다. 본체에서 NOR 백업이나 로그 전송을 선택하면 연결된 앱이 받습니다. NOR 전체 백업은 진단 펌웨어에서만 제공하며 시동 OFF 상태가 필요합니다.

전환 중 상시 전원을 유지하세요. 진단 펌웨어가 복귀 기록을 확정하기 전 전원이 끊기면 기존 Gate 복구 대기 상태가 될 수 있습니다. 기존 순정 복귀 구간의 전원 차단 한계도 해결했다고 주장하지 않습니다. 이번 검증은 소프트웨어·에뮬레이션 기준이며 실차·무선·실제 LCD 시험은 별도입니다.
