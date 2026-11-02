# NewDoe 0.13.1 beta

The APK includes firmware **0.13.1** and optional B language resources **0.12.2**.

## 한국어

지도 화면의 목적지 거리·방향 표시와 앱의 목적지 설정을 제거했습니다. 지도·현재 위치·주행 궤적·확대/축소·나침반, 트립과 ODO는 그대로 사용할 수 있습니다.

1. **NewDoe-0.13.1.apk**를 기존 앱 위에 설치하세요. 앱을 삭제하거나 데이터를 지우지 마세요.
2. Bluetooth를 켜고 기기를 선택한 뒤 **현재 상태 확인 → 업데이트**를 누르세요. 필요한 파일은 앱 안에 있습니다.
3. 안전 안내와 단계별 키 ON/OFF 안내에 따라 본체 펌웨어도 **0.13.1**로 업데이트하세요. 앱만 바꾸면 본체의 기존 화면 코드는 바뀌지 않습니다.
4. B 언어팩 **0.12.2**를 이미 설치했다면 다시 설치할 필요가 없습니다. 처음 설치하는 경우 펌웨어 설치 확인 후 원하는 때 언어 자원을 설치할 수 있습니다.

0.12.0·0.12.1·0.12.2는 일반 CFW 업데이트를 사용합니다. Gate·Bootstrap과 기존 사진·설정은 유지됩니다. 구형 Gate 기기는 [그림 설명서](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/Guide.md)의 **데이터 유지 순정 복귀 → 재설치** 경로를 따르세요.

중단된 설치가 있다면 **현재 상태 확인**부터 하세요. 앱은 그 시도에 사용하던 원래 파일을 유지합니다. 수동 ZIP 설치는 **전문가용 설정**에 있습니다.

설치·순정 복귀 중 배터리를 분리하거나 스타터를 누르지 마세요. 키 OFF에서 위+O를 동시에 누르지 마세요. 구형 펌웨어의 순정 복귀 도중 전원이 끊기면 자동 복구되지 않을 수 있습니다.

## English

Removed destination distance/direction from the device map and destination selection from the app. Maps, current position, travelled track, zoom, compass, trip counters and ODO remain available.

1. Install **NewDoe-0.13.1.apk** over your current app. Keep app data and pairing.
2. Enable Bluetooth, select the unit and tap **Check current status → Update**. Required files are included.
3. Follow the safety and key ON/OFF instructions to update the device firmware to **0.13.1** too. Updating only the APK does not replace the device's display code.
4. Already-installed B language resources **0.12.2** need no reinstall. New users may install the included optional language resources after firmware confirmation.

Use a normal CFW update from 0.12.0, 0.12.1 or 0.12.2. Gate, Bootstrap, photos and settings are retained. Older Gate devices follow the [illustrated guide](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/Guide.en.md)'s **keep-data stock return → reinstall** route.

For an interrupted installation, start with **Check current status**. The original attempt package remains retained. Manual ZIP installation is available in **Expert settings**.

Keep battery power connected during installation and stock recovery. Do not operate the starter or press UP+O together with the key OFF. Power loss during an already-installed legacy stock-return routine may prevent automatic recovery.

## Files

- `NewDoe-0.13.1.apk`: app with firmware and optional languages included.
- `NewDoe-Illustrated-Manual-0.13.1.zip`: Korean/English illustrated offline guides.
- `NewDoe-CFW-0.13.1.zip`, `NewDoe-LANGUAGE-B-0.12.2.zip`: standalone copies for expert/manual use.
- `LANGUAGE-LICENSES.txt`, `SHA256SUMS.txt`: licenses and hashes.
- `NewDoe-0.13.1-relink-objects.zip`: replaceable map-library objects.
