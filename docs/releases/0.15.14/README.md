# NewDoe 0.15.14

## 0.15.14 — Guidance notifications and offline map recovery

Install app build 164 over the existing app, then use **Device → Check current status → Update** to install the included 0.15.14 firmware. Both parts are needed for these fixes. Existing settings, pairing, maps and ride records are retained; do not clear app data or re-import working maps as a first step.

- Guidance no longer expires because the notification text has not changed for 30 seconds while stopped. The app checks the current ongoing notification; ending it or losing notification access stops external guidance. No background reconciliation timer runs when external guidance is inactive, apart from one pending event check.
- At an intersection, Maps may replace a distance such as `50 公尺` with a maneuver instruction. NewDoe keeps the supplied arrow, road and trip summary. Only an unavailable next-turn distance becomes `--`; it is not replaced by zero or the destination distance.
- Destination distance, duration and arrival are read independently. Localized metadata labels and parentheses/slashes are accepted. Rounded copies in expanded notifications no longer invalidate the standard summary. An omitted field retains its last received value only within the same ongoing guidance session; NewDoe does not calculate external values.
- Standard and promoted/ProgressStyle notifications use the supplied title, expanded title, road and summary fields. Generic Android progress numbers and notification timestamps are not treated as navigation distance or ETA.
- If Google Maps guidance still does not appear on Android 16 or later, open **Navigate → Google Maps notification settings**. Try turning off **Live Updates** or **Show live info**, while leaving navigation notifications enabled. The labels vary by phone. This is a compatibility workaround reported by a user, not a requirement to turn off all notifications or install Wear.
- Offline MAP rendering now reads the page from the correct part of its packed status. Leaving and returning to MAP no longer cancels raster work because the transfer-generation counter changed.

The app retains the 0.15.13-hotfix1 reconnect, background cleanup and restart protections. Software tests reproduce these conditions with synthetic notifications and production ARM drawing code. The original reports did not include raw notification payloads; actual Google Maps/One UI versions and physical vehicle rendering still need confirmation. The previously accepted legacy stock-return power-interruption limitation remains; Gate and the resident bootloader are unchanged.


## 0.15.14 — 외부 길 안내와 오프라인 지도 복구

앱 빌드 164를 기존 앱 위에 설치한 뒤 **기기 → 현재 상태 확인 → 업데이트**에서 내장 펌웨어 0.15.14도 설치하세요. 이번 수정에는 앱과 펌웨어가 모두 필요합니다. 설정·페어링·지도·주행 기록을 유지합니다. 먼저 앱 데이터를 지우거나 정상 지도 파일을 다시 가져올 필요는 없습니다.

- 정차 중 알림 내용이 30초간 바뀌지 않아도 안내를 대기로 바꾸지 않습니다. 현재 안내 알림이 유지되는지 확인하며, 안내 종료나 알림 접근 해제 시 외부 안내를 종료합니다. 외부 안내가 없으면 이벤트 뒤 남은 한 차례 확인을 제외하고 확인 타이머도 중지합니다.
- 회전 직전 Google Maps가 `50 公尺` 같은 거리 대신 회전 문구를 보내면 제공된 화살표·도로명·목적지 요약을 유지합니다. 다음 회전 거리만 없으면 그 숫자만 `--`로 표시하며, 0이나 목적지 거리로 대신하지 않습니다.
- 목적지 잔여거리·남은 시간·도착시간을 각각 해석합니다. 현지어 설명과 괄호·슬래시 구분을 처리하고, 펼친 알림의 반올림 값이 다르다고 전체 거리를 버리지 않습니다. 일부 값이 빠진 갱신은 같은 안내 알림에서 받은 마지막 값만 유지합니다. 외부 값을 임의 계산하지 않습니다.
- 일반 알림과 Live Updates/ProgressStyle 알림에서 제공되는 제목·확장 제목·도로명·요약을 읽습니다. Android의 일반 진행률 숫자나 알림 생성 시각을 거리·ETA로 간주하지 않습니다.
- Android 16 이상에서 Google Maps 안내가 계속 보이지 않으면 **내비게이션 → Google Maps 알림 설정**에서 **실시간 업데이트(Live Updates)** 또는 **실시간 정보 표시(Show live info)**를 꺼 보세요. 길 안내 알림은 켜 두세요. 휴대폰마다 이름이 다릅니다. 사용자 보고에 따른 호환성 우회 방법이며, 전체 알림을 끄거나 Wear를 설치할 필요는 없습니다.
- 오프라인 MAP 상태값에서 현재 페이지와 전송 세대 번호를 구분하도록 수정했습니다. MAP을 나갔다 다시 들어오면 지도 그리기가 반복 취소될 수 있던 문제를 해결합니다.

0.15.13-hotfix1의 재접속·백그라운드 정리·반복 재시작 보호를 유지합니다. 합성 알림과 실제 ARM 그리기 코드로 소프트웨어 시험을 수행합니다. 원본 알림 자료는 제공되지 않아 실제 Google Maps·One UI 버전과 차량 화면에서의 확인은 별도로 필요합니다. 기존에 수락한 구형 순정 복귀 구간의 전원 중단 한계는 남아 있으며 Gate·상주 부트로더는 변경하지 않습니다.

