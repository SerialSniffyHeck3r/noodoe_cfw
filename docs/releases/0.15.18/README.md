# NewDoe 0.15.18

Install app build 168 over the existing app, then update the included firmware. Existing settings, pairing, maps and ride records are retained. Do not clear app data.

- Ride Summary stays at normal brightness for 0.25 seconds, then Slow effect dims that same screen over 3 seconds. It accelerates gently for the first second and decelerates more slowly over the last two seconds. Only after the lamp is dark does Summary leave and the configured standby/power-saving timers start.
- Backlight PWM keeps its 500 Hz frequency and existing integer-percent brightness levels, with 80 times finer duty resolution during dimming. Fractional brightness survives display reconciliation and recovery. Brightness updates are serviced at 10 ms owner waits without increasing the display frame-rate target.
- Instant off shows Summary for 0.25 seconds and turns the lamp off directly. Ignition ON restores brightness without a fade after the existing wake/frame checks. Automatic/manual brightness, welcome light, emergency off and update/recovery ownership are retained.
- Shared backlight code is rebuilt into matching Product, preparation, Bootstrap and Diagnostic images. Gate, resources, fonts, icons and other application functionality remain unchanged.

Software validation only; no new phone/vehicle or optical low-light test. The accepted legacy stock-return power-interruption limitation remains. Google Play signing migration is separate and unconfirmed.

## 한국어

앱 빌드 168을 기존 앱 위에 설치한 뒤 내장 펌웨어도 업데이트하세요. 설정·페어링·지도·주행 기록을 유지하며 앱 데이터를 지우지 않습니다.

- 주행 요약을 0.25초 표시한 뒤 같은 화면에서 3초 동안 감광합니다. 첫 1초는 부드럽게 가속하고 뒤 2초는 더 완만하게 감속합니다. 완전히 어두워진 뒤 요약을 지우고 설정된 대기·절전 단계와 타이머를 시작합니다.
- PWM 500Hz와 기존 밝기 설정의 펄스 폭을 유지하며 감광 분해능을 80배 높였습니다. 밝기 재적용·복구에서도 소수 밝기를 유지합니다. 화면 목표 프레임률을 올리지 않고 밝기 처리를 10ms 대기로 서비스합니다.
- 바로 꺼짐은 요약 0.25초 후 즉시 소등합니다. 시동ON 복귀, 자동·수동 밝기, 웰컴 라이트, 긴급 소등과 업데이트·복구의 제어는 유지합니다.
- 공용 백라이트 변경을 포함하는 일반·준비·설치·진단 펌웨어를 함께 다시 빌드했습니다. Gate·리소스·폰트·아이콘과 다른 앱 기능은 유지합니다.

소프트웨어 검증이며 새 휴대폰·차량 시험과 저휘도 광량 실측은 없습니다. 기존에 수락한 구형 순정 복귀 구간의 전원 중단 한계와 Google Play 서명 전환 미확인 상태는 그대로입니다.
