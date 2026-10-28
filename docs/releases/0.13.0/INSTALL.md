# NewDoe app 0.13.0 beta

The Android app now includes verified device firmware **0.12.2** and optional B language resources **0.12.2**. Normal installation needs only the APK.

## 한국어

1. **NewDoe-0.13.0.apk**를 기존 앱 위에 설치하세요. 앱을 삭제하거나 데이터를 지우지 마세요.
2. Bluetooth를 켜고 기기를 선택한 뒤 **현재 상태 확인**을 누르세요. 필요한 설치 파일은 앱 안에 있습니다.
3. 확인된 기기에 맞춰 **설치 / 업데이트**를 누르고 안전 안내와 화면의 키 조작 안내를 따르세요. 순정에서 처음 설치하는 경우 단계별 설치 마법사를 선택할 수도 있습니다.
4. 펌웨어 설치가 확인된 다음 원하는 경우 **언어 자원 설치**를 선택하세요. B 언어팩도 앱 안에 들어 있으므로 따로 다운로드하지 않습니다. 설치하지 않으면 영어를 사용합니다. A는 영어만 지원합니다.

**이미 본체가 0.12.2라면 앱만 업데이트하면 됩니다.** 이번에는 기기 펌웨어·Gate·Bootstrap·언어팩의 내용이 바뀌지 않았습니다. 기존 사진과 설정은 그대로 유지합니다.

중단된 설치가 있으면 **현재 상태 확인**부터 하세요. 앱은 그 설치에 사용하던 원본 파일과 진행 기록을 보관하며, 앱이 업데이트돼도 새 파일로 바꿔 이어 쓰지 않습니다. 이전 파일이 없거나 기록을 확인할 수 없으면 안내에 따라 원래 파일을 준비하세요. 앱 삭제·데이터 삭제는 이 보관 자료도 지웁니다.

**전문가용 설정**에 기존 진단·복구·수동 ZIP 설치가 남아 있습니다. 수동 ZIP은 별도 주의 화면을 확인한 뒤 선택하고 설치합니다. 일반 설치에는 필요하지 않습니다. 오래된 Gate가 있는 기기는 [그림 설명서](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/Guide.md)의 **데이터 유지 순정 복귀 → 재설치** 경로를 따르세요.

설치·순정 복귀 중 배터리를 분리하거나 스타터를 누르지 마세요. 키 OFF에서 위+O를 동시에 누르지 마세요. 구형 펌웨어의 순정 복귀 도중 전원이 끊기면 자동 복구되지 않을 수 있습니다. 단계별 키 ON/OFF 안내를 따르세요.

## English

1. Install **NewDoe-0.13.0.apk** over your current app. Do not uninstall it or clear app data.
2. Enable Bluetooth, select the unit and tap **Check current status**. Required installation files are included.
3. Use **Install / Update** for the detected device and follow the safety and key ON/OFF instructions. A step-by-step guide is optional when starting from stock firmware.
4. After firmware confirmation, choose **Install language resources** if wanted. B resources are included too. Without them, use English; A-language units remain English-only.

**If your device already runs 0.12.2, update only the app.** Device firmware, Gate, Bootstrap and language contents are unchanged. Existing photos and settings are preserved.

For an interrupted installation, start with **Check current status**. The app retains its original package and progress records across app updates; it does not substitute a newer package mid-install. If the original package is missing or the records cannot be verified, follow the instructions to supply the matching file. Uninstalling the app or clearing its data deletes these retained files too.

**Expert settings** keeps the existing diagnostic/recovery tools and manual ZIP installation behind an additional caution screen. Normal installation does not need them. Older Gate devices follow the [illustrated guide](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/Guide.en.md)'s **keep-data stock return → reinstall** route.

Keep battery power connected during installation and stock recovery. Do not operate the starter or press UP+O together with the key OFF. Power loss during an already-installed legacy stock-return routine may prevent automatic recovery. Follow the app's key ON/OFF instructions at each step.

## Files

- `NewDoe-0.13.0.apk`: app with firmware and optional language resources included.
- `NewDoe-Illustrated-Manual-0.13.0.zip`: offline Korean/English illustrated guides.
- `NewDoe-CFW-0.12.2.zip` / `NewDoe-LANGUAGE-B-0.12.2.zip`: unchanged standalone copies for expert/manual use only.
- `LANGUAGE-LICENSES.txt`, `SHA256SUMS.txt`: licenses and file hashes.
- `NewDoe-0.13.0-relink-objects.zip`: replaceable map-library objects.
