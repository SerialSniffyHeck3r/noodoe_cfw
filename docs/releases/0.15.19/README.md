# NewDoe 0.15.19

Install app build 169 over the existing app, then update the included firmware. Settings, pairing, maps and ride records are retained; do not clear app data.

- Manual brightness changes ease through fine PWM levels over 0.6 seconds. Automatic brightness also rises and falls continuously over 1.8 seconds, instead of two-percent steps. Repeated target requests do not restart the transition; a new target starts from the current level.
- Automatic brightness accepts brighter surroundings after 1.2 seconds of stability and darker surroundings after 2.5 seconds. Brief shadows and repeated threshold crossings do not make the display hunt. Existing calibration, manual fallback, bias and UART lighting values are retained.
- Screen-off wake waits another 0.5 seconds after a fresh frame is presented before lighting the screen and starting the ring animation. The ring stays frozen during this delay. Ignition reversal and stale frame acknowledgements cannot complete a different wake.
- Turning the light on remains immediate after the wake checks. Existing 0.25-second Summary hold, 3-second shutdown fade, Instant off, emergency off and update/recovery ownership remain. Welcome light retains its one-second breathing pattern with fine PWM levels. An ignition reversal during dimming restores the normal brightness immediately.
- PWM remains 500 Hz with the existing maximum duty and gate sequencing. No image, font, icon, language resource, memory reservation or compiler policy was reduced.

Software validation only; no new phone/vehicle or optical measurement. Existing lint findings remain separately recorded. The accepted legacy stock-return power-interruption limitation remains. Google Play signing migration is separate and unconfirmed.

## 한국어

앱 빌드 169를 기존 앱 위에 설치한 뒤 내장 펌웨어도 업데이트하세요. 설정·페어링·지도·주행 기록을 유지하며 앱 데이터를 지우지 않습니다.

- 수동 밝기는 0.6초, 자동 밝기는 1.8초에 걸쳐 미세 PWM 값으로 부드럽게 올라가고 내려갑니다. 같은 목표를 반복 요청해도 전환을 다시 시작하지 않고, 목표를 바꾸면 현재 밝기에서 이어집니다.
- 자동 밝기는 밝아진 주변광이 1.2초, 어두워진 주변광이 2.5초 유지돼야 반영합니다. 순간적인 그림자와 조도 경계의 반복 변화로 밝기가 흔들리지 않도록 했습니다. 기존 보정·수동 대체값·오프셋·UART 조도 값은 유지합니다.
- 화면 OFF에서 복귀할 때 새 프레임 표시 확인 후 0.5초를 더 기다리고 점등과 링 애니메이션을 시작합니다. 대기 중 링은 움직이지 않으며, 이전 시동 상태의 화면 응답으로 새 복귀를 완료하지 않습니다.
- 켜질 때는 복귀 확인 후 즉시 점등합니다. 요약 0.25초·종료 감광 3초·바로 꺼짐·긴급 소등·업데이트 및 복구 제어는 유지합니다. 웰컴 라이트도 기존 1초 주기에 미세 PWM 값을 사용합니다. 감광 중 시동 ON은 정상 밝기로 즉시 복귀합니다.
- PWM 500Hz와 최대 듀티·전원 제어 순서를 유지합니다. 그림·폰트·아이콘·언어 리소스·예약 메모리·컴파일러 정책을 줄이지 않았습니다.

소프트웨어 검증이며 새로운 휴대폰·차량 시험과 광량 실측은 없습니다. 기존 lint 항목은 별도로 기록했습니다. 기존에 수락한 구형 순정 복귀 구간의 전원 중단 한계와 Google Play 서명 전환 미확인 상태는 그대로입니다.
