# NewDoe 0.15.17

Install app build 167 over the existing app, then install the included firmware. Existing settings, pairing, maps and ride records are retained. Do not clear app data.

- After the existing ignition-OFF debounce, Ride Summary stays at normal brightness for 0.5 seconds. Slow effect then dims that same shaded summary over 3 seconds. Only once the lamp is dark does the display leave Summary and enter the configured standby/power-saving stages; their timers start at that point.
- Instant off shows Summary for 0.5 seconds, then turns the lamp off directly. The existing Power sequence > IGN OFF dimming setting and default Slow effect are retained.
- Ignition ON cancels the fade and restores brightness without a fade, preserving the existing wake/frame checks. Emergency power-off and update/recovery ownership remain unchanged.
- The curve, graphics, translations, ride recording, other settings and functionality are unchanged. This fixes the 0.15.16 sequence that started fading after switching away from Summary.

Software validation only; no new phone/vehicle test. The previously accepted legacy stock-return power-interruption limitation remains. Google Play signing migration is separate and unconfirmed.

## 한국어

앱 빌드 167을 기존 앱 위에 설치한 뒤 내장 펌웨어도 업데이트하세요. 설정·페어링·지도·주행 기록을 유지하며 앱 데이터를 지우지 않습니다.

- 기존 시동 OFF 유예 후 주행 요약을 0.5초 표시합니다. 슬로우 이펙트는 음영이 있는 요약 화면을 그대로 유지하며 3초 동안 감광합니다. 완전히 어두워진 뒤 요약 화면에서 설정된 대기·절전 단계로 넘어가며, 각 단계의 타이머도 그때 시작합니다.
- 바로 꺼짐은 요약 0.5초 후 즉시 소등합니다. 전원 순서 > 시동 OFF 감광 설정과 기본 슬로우 이펙트를 유지합니다.
- 감광 중 시동 ON은 감광을 취소하고 기존 화면 복귀 확인을 거쳐 밝기를 즉시 복구합니다. 긴급 전원 차단과 업데이트·복구의 전원 제어는 그대로입니다.
- 감광 곡선, 그래픽, 번역, 주행 기록, 다른 설정과 기능은 유지합니다. 0.15.16에서 요약 화면을 벗어난 뒤 감광하던 순서를 수정했습니다.

소프트웨어 검증이며 새 휴대폰·차량 실기기 시험은 없습니다. 기존에 수락한 구형 순정 복귀 구간의 전원 중단 한계와 Google Play 서명 전환 미확인 상태는 그대로입니다.
