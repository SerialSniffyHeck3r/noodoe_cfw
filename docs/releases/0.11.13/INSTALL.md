# NewDoe 0.11.13 beta

## 0.11.13: Live ODO, UART recovery and Bootstrap handoff

Use the matching **NewDoe-0.11.13.apk + NewDoe-CFW-0.11.13.zip**. Product must also be updated to **0.11.13** for the ODO/UART fixes. Devices with the same Gate (0.11.4 onward) use the existing CFW update route. Gate and B language resources are unchanged. The Bootstrap display fix applies to new stock installations with this ZIP; do not return to stock just to change that temporary screen.

From 0.11.13, ODO storage, correction and replacement with an older value are removed. ODO displays the current valid dashboard UART reading. Missing data is shown as unknown until reception resumes. The key-OFF display may retain this running session in RAM only. TRIP A/B, today/session trips, oil, belt, service records and settings remain preserved. The ODO check menu is removed; old correction records are neither read nor rewritten.

After the stock transfer has been verified, the app closes the connection and waits. Keep module power connected. Follow the stock screen: IGN OFF, wait for the NewDoe Bootstrap menu, then IGN ON. Tap **Check current status** in the app. On Noodoe choose **Install CFW** and answer **Reset Bluetooth?** (default **No**, preserves pairing). The app waits for this local choice on the same connection, then continues the existing preparation/verification/install flow without a third phone button. If already in the installation screen, it proceeds as soon as local permission and readiness are verified. The existing separate restore-data installation choice remains available; an existing selected restore policy is retained.

Transfer verified is not CFW installed. The final on-device confirmation/countdown and Product boot confirmation remain required. A connection failure or an uncertain commit does not authorize resending Bootstrap or silently starting a new installation. If pairing fails, use the existing optional Bluetooth recovery guide and check the device again.

The developer message keeps all original text and page controls. Its left/right placement now uses the correct EVE text alignment bits. Progress displays fresh acknowledged transfer bytes rather than an old device job; internal work is sampled every 250 ms and the screen refreshes every 200 ms. Repeated progress ticks no longer rebuild unchanged settings controls. These intervals are software scheduling targets, not a physical Bluetooth latency guarantee.

Software verification only; no physical LCD/RF/installation qualification this turn. Keep module power stable. The previously accepted old-code stock-return power-interruption limitation remains.

## 한국어

## 0.11.13: 현재 ODO 표시, UART 복구와 Bootstrap 수동 연결 확인

**NewDoe-0.11.13.apk와 NewDoe-CFW-0.11.13.zip**을 함께 사용하세요. Product도 **0.11.13**으로 업데이트해야 ODO·UART 수정이 적용됩니다. 같은 Gate를 쓰는 0.11.4 이상은 기존 CFW 업데이트 경로로 진행합니다. Gate와 B 언어팩은 그대로입니다. Bootstrap 표시 수정은 새 ZIP으로 순정에서 설치할 때 적용되므로, 그 화면만 바꾸려고 순정으로 돌아갈 필요는 없습니다.

0.11.13부터 ODO 자체 저장·보정과 이전 값 대체를 제거했습니다. 계기판 UART의 현재 유효값을 그대로 표시합니다. 수신이 끊기면 알 수 없음으로 표시하고 정상 수신 시 다시 표시합니다. 키 OFF 화면의 마지막 값은 이번 실행 중 RAM에만 남습니다. TRIP A/B·오늘/이번 주행·오일·벨트·정비 기록과 설정 보존은 유지합니다. ODO check 메뉴는 없어졌으며 예전 보정 기록은 읽거나 다시 쓰지 않습니다.

순정에서 Bootstrap 전송 검증이 끝나면 앱은 연결을 닫고 기다립니다. 상시 전원은 유지하고 순정 화면 안내에 따라 IGN을 끈 뒤, NewDoe Bootstrap 메뉴가 뜨면 IGN을 켜세요. 앱의 **현재 상태 확인**을 누르고 누도에서 **Install CFW**를 선택하세요. **Reset Bluetooth?**는 기본 **No**로 기존 페어링을 유지합니다. 앱은 같은 연결에서 이 선택을 기다렸다가 준비·검증·설치를 이어갑니다. 이제 세 번째 앱 버튼을 다시 누르지 않아도 됩니다. 이미 설치 화면에 있으면 실제 권한과 준비 상태를 확인한 뒤 곧바로 이어갑니다. 기존의 보관 데이터 복원 선택 경로는 유지하며, 이미 선택한 복원 정책도 보존합니다.

전송 검증 완료는 CFW 설치 완료가 아닙니다. 마지막 본체 설치 확인·카운트다운과 Product 정상 실행 확인은 그대로 거칩니다. 연결 실패나 미확정 설치 기록을 이유로 Bootstrap을 자동 재전송하거나 새 설치로 덮어쓰지 않습니다. 페어링이 안 되면 기존 선택형 Bluetooth 복구 안내를 따른 뒤 기기 상태를 다시 확인하세요.

개발자 메시지 원문과 페이지 조작은 그대로이며 EVE의 정렬 비트를 바로잡았습니다. 진행 표시에는 이전 기기 작업 값 대신 확인된 최신 전송량을 반영합니다. 내부 작업은 250ms 간격으로 조회하고 앱은 200ms 간격으로 표시합니다. 진행률 갱신만으로 변경 없는 설정 UI를 반복 구성하지 않습니다. 실제 Bluetooth 지연을 보장하는 수치는 아닙니다.

이번 검증은 소프트웨어 시험이며 실물 LCD·무선·설치는 시험하지 않았습니다. 모듈 전원을 유지하세요. 사용자가 앞서 수용한 구형 코드의 순정 복귀 도중 전원 중단 제한은 그대로 남아 있습니다.
