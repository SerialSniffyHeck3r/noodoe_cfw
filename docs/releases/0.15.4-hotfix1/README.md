# NewDoe 0.15.4 Hotfix 1 — notification connection patch

**APK 0.15.4 / Android build 145. Embedded firmware: unchanged 0.15.4.**

## 한국어

CFW 0.15.3 → 0.15.4 업데이트 이후 본체는 정상인데 폰에 `CONTENT / 0x74 / IllegalArgumentException`이 표시되는 보고에 대응합니다. 기존 코드는 외부 알림 아이콘을 불러오는 실패는 처리했지만 실제 그리기 실패는 처리하지 않아 주행 연결을 종료할 수 있었습니다.

- 소프트웨어 Canvas에 그릴 수 없는 하드웨어 비트맵은 소프트웨어 복사본으로 변환합니다.
- 아이콘이 계속 실패하면 앱 아이콘, 기본 알림 아이콘 순서로 대체합니다. 제목·본문과 이후 알림 전송은 유지합니다.
- Java 오류의 마지막 통신 명령과 실제 발생 위치를 구분할 수 있도록 코드 위치를 추가합니다. 알림 본문·발신자·예외 메시지는 기록하지 않습니다.

**이미 본체 0.15.4가 정상 부팅했다면 새 APK만 기존 앱 위에 설치하고 다시 연결하세요.** 앱 데이터 삭제, 재페어링, 순정 복귀나 펌웨어 재설치가 필요하지 않습니다. 이미 진행 중인 작업의 기록·선택·후보 해시는 그대로 보존합니다. 본체가 아직 0.15.3이면 패치 APK의 기존 정상 업데이트 흐름을 이용합니다.

APK와 함께 제공하는 기본 설치 ZIP, 언어별 펌웨어·폰트·준비용 Product, Gate 및 NOR 자원은 기존 0.15.4와 동일합니다. 본체 플래시 사용량이나 기능은 바뀌지 않습니다. 일반 사용자는 APK를 사용하고 ZIP은 전문가용으로만 사용하세요.

## English

This app-only hotfix addresses reports of `CONTENT / 0x74 / IllegalArgumentException` after a 0.15.3 → 0.15.4 update while the dashboard appears normal. The old notification path caught icon-loading failures but allowed drawing failures to terminate the riding connection.

Hardware-backed bitmap icons are copied for software rendering. An icon that still cannot be drawn falls back to the app icon and then the default notification icon. Message text and subsequent notification delivery continue. Other Java failures now include a bounded code location, never notification contents or free-form exception messages.

**If the dashboard already boots 0.15.4 normally, install this APK over the existing app and reconnect.** Do not clear app data, remove pairing or return to stock for this patch. Interrupted-install records and exact candidate checks remain intact. Devices still on 0.15.3 use the existing normal update workflow.

## Verification scope

The old failure was reproduced by injecting an icon drawing exception immediately after notification command 0x74. The patched path completes notification transfer and continues with later messages. Rendering and protocol/connection tests are software-based. The field report did not include a stack trace, so this is a reproduced matching failure path, not a hardware-confirmed identification of that report's exact cause. The host graphics emulator cannot create a real GPU-backed bitmap; that platform boundary is fault-injected.

Firmware assets are byte-identical to 0.15.4; prior ARM/EVE and 45-variant checks are retained, not claimed as new hardware tests. No phone/vehicle was available for this hotfix. The previously accepted legacy stock-return power-interruption limitation remains unchanged.

## Downloads

Use `NewDoe-0.15.4-hotfix1.apk`. The 0.15.4 installer ZIPs are included for matching expert use. The updated Korean/English illustrated manual covers installation, OpenWeather and BRouter. See `verification.json` and `SHA256SUMS.txt` for checks and exact hashes. Existing licenses and matching replaceable-library objects are included.
