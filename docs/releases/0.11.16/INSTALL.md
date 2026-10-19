# NewDoe 0.11.16 beta — 설치 안내와 대시보드 / Guided setup and dashboard

**0.11.16 APK와 CFW ZIP을 함께 사용하세요. 이번에는 Gate도 바뀌므로 이전 버전에서 처음 넘어올 때 순정 복귀 → CFW 데이터 보관 → 재설치 → 보관 데이터 복원을 선택합니다.** 앱이 안내하는 경로를 따르세요. 이미 설치한 정상 B 0.11.2 언어팩은 그대로 사용합니다.

**Use the matching 0.11.16 APK and CFW ZIP. This version changes Gate: the first move from an older version requires returning to stock while keeping CFW data, reinstalling, and restoring retained data.** Follow the route shown by the app. Keep an existing healthy B 0.11.2 language pack.

- [한국어 그림 설명서](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/Guide.md)
- [English illustrated guide](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/Guide.en.md)

## 바뀐 점

- Bootstrap 시작: 3초간 중앙 워드마크만 표시한 뒤 실제 확인 항목·가로 진행바를 표시합니다.
- 앱 첫 실행 안내와 선택형 설치 마법사를 추가했습니다. 기존 설치·검증·설정 보존 흐름을 사용합니다.
- 설치 전 다섯 안전 그림을 확인하고, 진행 중에도 그림을 표시합니다. 새 Gate에서 직접 순정 복귀를 고르면 세 금지 그림을 O로 하나씩 확인합니다.
- Bluetooth가 꺼져 있으면 연결 시도를 멈추고 켜는 방법을 안내합니다.
- 정비 정보는 오일·벨트의 남은 km와 정기 점검의 남은 %를 세 개의 원형 호로 표시합니다.
- 키 OFF에서 마지막으로 받은 ODO를 RAM에 유지합니다. 키 ON에서는 새 수신값이 필요하고, 재부팅 후 예전 ODO를 복원하지 않습니다. 트립·정비 기록은 유지합니다.
- 처음 새로 설치할 때 배경 1은 검은 바탕의 NEWDOE 워드마크 사진입니다. 데이터 보존 재설치나 업데이트에서는 내 사진을 덮어쓰지 않습니다.
- 한국어·영어 그림 설명서와 앱 내 안내를 갱신했습니다.

## What changed

- Bootstrap shows only the centered wordmark for three seconds, then the actual check and a horizontal progress bar if checks are still running.
- First-launch guidance and an optional installation wizard wrap the existing installation, verification and settings-preservation flow.
- Five safety confirmations before installation, reminders during installation, and three fresh-O confirmations before manually requested stock recovery in the new Gate.
- Bluetooth OFF stops connection attempts and shows a settings shortcut.
- Three maintenance rings: oil/belt kilometres remaining and service percentage remaining.
- The last received ODO stays in RAM with the key OFF. Key ON requires fresh vehicle data; reboot does not restore a saved ODO. Trips and maintenance records remain intact.
- A fresh installation uses the NEWDOE wordmark as the ordinary Background 1 photo. Keep-data reinstalls and updates preserve existing photos.
- Updated Korean/English illustrated manuals and in-app guidance.

## 설치 / Installation

1. APK를 기존 앱 위에 설치하고 CFW ZIP은 압축을 풀지 말고 선택하세요. / Install the APK over the existing app and select the CFW ZIP without extracting it.
2. 이전 버전은 앱의 순정 복귀·데이터 보관 안내로 진행한 뒤 새 ZIP으로 재설치하세요. / From an older version, follow the keep-data stock return and reinstall route.
3. 순정에서 설치 도구를 처음 전송할 때는 키 ON이 필요합니다. Bootstrap 메뉴가 뜨면 현재 상태 확인을 누르고 누도에서 Install CFW를 선택하세요. / The first transfer from stock needs key ON. Once Bootstrap appears, check current status and choose Install CFW on the Noodoe.
4. CFW UPDATE가 보이면 앱에서 확인하고 설치 완료까지 기다리세요. 그다음 언어팩 안내를 따릅니다. / Confirm CFW UPDATE in the app and wait for completion before following the language prompt.

설치·순정 복귀 중 배터리와 누도 전선을 분리하거나 스타터를 누르지 마세요. 키 OFF에서 UP + O를 동시에 누르지 마세요. 시작 전 배터리가 11.5V 이상인지 확인하세요. 키 OFF가 가능한 단계에서는 OFF를 권장하며 화면 안내를 따르세요. **구버전 코드가 순정으로 복귀하는 도중 전원이 끊기면 별도 수리가 필요할 수 있습니다.**

Keep the battery and Noodoe wiring connected; do not use the starter or press UP + O together with the key OFF during installation/recovery. Check at least 11.5V before starting. Key OFF is recommended where allowed; follow the current screen. **Power loss while older code returns to stock may require separate repair.**
