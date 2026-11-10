# NewDoe 0.15.4 Hotfix 4 — Android build 148

## 한국어

**content / 0x9D / IllegalArgumentException / VisualTransfer**로 주행 연결이 종료되는 앱 버그를 수정했습니다. 날씨 패킷을 256바이트로 줄인 뒤 전송 검사에는 과거의 2,648바이트 조건이 남아 있었습니다. 날씨 OFF·API 키 미등록 상태에서도 표시를 지우는 256바이트 패킷을 보내므로 같은 예외가 발생할 수 있었습니다.

전송 검사가 날씨 생성기의 공통 크기 상수를 사용하도록 수정했습니다. 정상 예보와 표시 지우기 패킷을 받으며, 잘못된 크기는 계속 거부합니다. 화면에 표시된 0x9D는 직전 통신 명령이고, 해당 예외는 무선 전송 전에 앱의 날씨 패킷 검사에서 발생했습니다.

새 APK를 기존 앱 위에 설치하고 다시 연결하세요. **본체가 이미 0.15.4로 정상 부팅하면 APK만 갱신하면 됩니다.** 이 문제를 해결하기 위한 재페어링·앱 데이터 삭제·순정 복귀·날씨 API 키 등록은 필요하지 않습니다. 본체가 구버전이면 기존 정상 업데이트 안내를 따르세요. 이전 미완료 설치 결과는 별도로 확인해야 합니다.

## English

This fixes a phone-side failure reported as **content / 0x9D / IllegalArgumentException / VisualTransfer**. Weather encoding had changed to a 256-byte packet while visual admission still required the obsolete 2,648-byte length. The clear-display packet sent when weather is disabled or no API key is configured could fail as well.

Visual admission now shares the weather encoder's size constant. Current forecasts and clear packets pass; invalid lengths remain rejected. The displayed 0x9D was the preceding wire command, not the source of this Java exception, which occurred before weather transmission.

Install the new APK over the existing app and reconnect. **A dashboard already booting 0.15.4 normally needs only the APK.** Re-pairing, app-data deletion, stock recovery and registering a weather API key are unnecessary for this fix. Older dashboards still use the normal update flow. Earlier unfinished installation results must be checked separately.

## Verification and unchanged behavior

The unpatched code reproduces the exact `VisualTransfer.<init>:16` exception. Five new integration tests fail before the fix; they cover disabled weather, missing API key, current/expired forecasts and wrong lengths. Updated tests run the actual WeatherFeed → VisualTransfer → NDCP packet path and confirm the clear packet is not repeatedly resent. Mocked forecasts and transport are used; no API key or live weather service is required for these tests.

All earlier hotfixes are included. Firmware, Gate, Bootstrap, fonts and installer ZIPs are byte-identical to 0.15.4. Weather fetch timing, API requirements, RAM-only weather storage and visual layout are unchanged. The accepted legacy stock-return power-interruption limitation remains unchanged. No new physical phone/vehicle test is claimed.

Matching APK, installation ZIPs, Korean/English illustrated manuals, licenses, relink objects and SHA-256 hashes are included. Source remains in the private repository.
