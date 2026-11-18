# NewDoe 0.15.20

Install app build 170 over the existing app, then update the included firmware. Keep app data: settings, pairing, maps and ride records are retained.

- Fix the ignition-off Welcome-light staircase. Fine PWM was applied only when the UI composed a frame (about 30 Hz). It now follows the existing 5 ms graphics-owner tick, independently of frame composition. No new timer interrupt, task or higher display frame rate is added.
- Keep the existing one-second 5–90% breathing curve, Welcome ring appearance, first-presented-frame check, Bluetooth eligibility and parking deadlines. Brightness cannot start before the screen is ready.
- Manual/automatic brightness smoothing, the 0.25-second Summary plus 3-second shutdown fade, instant ON, additional 0.5-second ring-wake wait, emergency off and update/recovery ownership are retained.
- No changes to Gate, language resources, fonts, images, PWM frequency, compiler policy or heap/stack reservations.

The regression reproduces frozen PWM without new frame composition in the old implementation and verifies continued fine-duty changes in the fixed implementation. Software validation only; no new physical phone, vehicle or optical measurement. Existing lint findings remain; the accepted legacy stock-return power-interruption limitation and separate, unconfirmed Google Play signing migration remain unchanged.

## 한국어

앱 빌드 170을 기존 앱 위에 설치하고 내장 펌웨어도 업데이트하세요. 앱 데이터를 지우지 않으며 설정·페어링·지도·주행 기록을 유지합니다.

- 시동 OFF 웰컴라이트의 계단 현상을 만드는 경로를 수정했습니다. 기존에는 화면을 그리는 약 30Hz 주기에만 미세 PWM을 적용했습니다. 이제 화면 그리기와 별개로 기존 그래픽 담당 작업의 5ms 주기에서 밝기를 갱신합니다. 새 타이머 인터럽트나 작업을 추가하거나 화면 프레임률을 높이지 않습니다.
- 기존 1초 주기·5~90% 밝기 곡선, 웰컴 링 표시, 첫 프레임 확인, Bluetooth 접근 조건과 절전 제한 시간을 유지합니다. 화면이 준비되기 전에는 점등하지 않습니다.
- 수동·자동 밝기 전환, 요약 0.25초 후 종료 감광 3초, 즉시 점등, 링 시작 전 0.5초 추가 대기, 긴급 소등과 업데이트·복구 제어를 유지합니다.
- Gate·언어 리소스·폰트·그림·PWM 주파수·컴파일러 정책·힙과 스택 예약은 변경하지 않았습니다.

화면을 다시 그리지 않는 조건에서 기존 코드의 PWM 정지를 재현하고, 수정 후에도 밝기가 계속 변하는 것을 소프트웨어로 검증했습니다. 새로운 휴대폰·실차·광량 실측은 없습니다. 기존 lint 항목, 수락한 구형 순정 복귀 구간의 전원 중단 한계 및 별도 Google Play 서명 전환 미확인 상태는 그대로입니다.
