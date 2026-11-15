# NewDoe 0.15.16

Install app build 166 over the existing app, then install the included firmware. Existing settings, pairing, offline maps and ride records are retained. Do not clear app data.

- External navigation: when the same maneuver temporarily loses its distance below 100 m, retain its last values for at most 15 seconds. New guidance, cancellation and disconnect supersede it immediately. Text and arrow positions do not move; no distance is invented.
- HOME notification strips: short notices expire after 5 seconds, scrolling notices after at most 10 seconds, even unread. Notifications arriving during exposure or the following 3 seconds are not replayed in HOME; notification history remains available.
- Power sequence → IGN OFF dimming: choose Instant off or Slow effect. Slow effect is the default: a 3-second logarithmic fade, slow at first and faster near darkness. Ignition ON restores brightness without a fade after the existing fresh-frame check. Emergency power-off and update/recovery behavior remain unchanged.
- Vehicle → Trip decimals: choose 0.0 (default) or 0.00 for the dedicated trip page and TRIP 1/2 footer on other pages. Every HOME trip field, including its footer, always remains 0.0. Both km and mi use the existing fresh-speed distance accumulator; changing display precision never resets totals or improves sensor accuracy.
- Ordinary device settings menus return immediately to HOME when fresh UART speed exceeds 15 km/h. There is no 30-second countdown. Initial setup is exempt; an accepted update/storage operation is not cancelled.
- Exact UI/debug strings reuse the existing verified NOR text arena to fit flash. Menu offset/length records use a compact representation; the reader also accepts the previous format. Every translation, font/icon asset, compiler policy, Gate and memory reservation is retained.

Software validation only; no new phone/vehicle test. The previously accepted legacy stock-return power-interruption limitation remains. Google Play signing migration is separate and unconfirmed.

## 한국어

앱 빌드 166을 기존 앱 위에 설치한 뒤 내장 펌웨어도 업데이트하세요. 설정·페어링·오프라인 지도·주행 기록을 유지하며 앱 데이터를 지우지 않습니다.

- 100m 미만에서 같은 내비 안내의 거리 정보가 잠시 사라지면 마지막 값을 최대 15초 유지합니다. 새 안내·안내 종료·연결 해제는 즉시 반영합니다. 화살표와 문구 위치는 그대로이며 거리를 추정하지 않습니다.
- 홈 알림은 짧으면 5초, 긴 스크롤도 최대 10초 후 지웁니다. 표시 중과 종료 후 3초 사이에 들어온 알림은 홈에 다시 띄우지 않습니다. 알림 기록은 유지합니다.
- 전원 순서 → 시동 OFF 감광: 바로 꺼짐 / 슬로우 이펙트를 선택합니다. 기본값은 처음에는 천천히, 끝으로 갈수록 빠르게 꺼지는 3초 감광입니다. 시동 ON은 감광 없이 기존 새 프레임 확인 뒤 밝기를 복구합니다.
- 차량 → 트립 소수 자릿수: 0.0 / 0.00을 선택합니다. 트립 전용 화면과 다른 페이지의 TRIP 1·2 하단 표시에 적용하며, 홈 화면의 모든 트립 표시는 하단 TRIP까지 항상 0.0입니다. km·mi 모두 기존 유효 속도 누적값을 사용하고 설정 변경으로 기록이 초기화되지 않습니다. 표시 자릿수와 센서 정확도는 별개입니다.
- 일반 설정 메뉴는 유효한 UART 속도가 15km/h를 넘으면 30초 대기 없이 홈으로 돌아갑니다. 초기 설정 마법사는 예외이며 이미 접수한 업데이트·저장 작업은 취소하지 않습니다.
- 기존 문구를 NOR 텍스트 리소스로 옮기고 문구의 위치·길이 목록을 작게 저장해 용량을 확보했습니다. 이전 목록 형식도 읽을 수 있습니다. 번역·폰트·아이콘·컴파일 정책·Gate·메모리 예약은 유지합니다.

소프트웨어 검증이며 새 휴대폰·차량 실기기 시험은 없습니다. 기존에 수락한 구형 순정 복귀 구간의 전원 중단 한계와 Google Play 서명 전환 미확인 상태는 그대로입니다.
