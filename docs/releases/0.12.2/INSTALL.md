# NewDoe 0.12.2 beta

Fixes first-install `0x00050005 / CFWPIC.DAT` failures and adds a read-only KYMCO photo source alongside the existing three editable NewDoe slots.

## 한국어

- 앱과 설치 ZIP을 **둘 다 0.12.2**로 사용하세요. 처음 설치하면 사진 1에 NEWDOE 기본 이미지가 들어갑니다. 기존 사용자의 사진은 유지됩니다.
- **0.12.0·0.12.1 → 0.12.2:** 기존 앱 위에 APK 설치 → 상태 페이지에서 CFW ZIP 선택 → 현재 기기 확인 → 앱 안내에 따라 일반 CFW 업데이트. 순정 복귀나 페어링 삭제는 필요하지 않습니다.
- 더 오래된 Gate는 설명서의 **데이터 유지 순정 복귀 → 재설치** 경로를 따르세요. 직접 Gate OTA는 하지 않습니다.
- 사진 전송 끝에 `0x00050005 / CFWPIC.DAT`로 멈췄다면 새 APK에서 새 ZIP을 선택하고 **현재 기기 확인**을 누르세요. 표시되는 이어가기 절차를 따르세요. 순정 앨범이나 앱 데이터를 지우지 마세요.
- 기기 시스템 설정의 **Photo source / 사진 출처**에서 `NewDoe` 또는 `KYMCO`를 고른 뒤 사진 1·2·3을 선택하세요. 폰에서도 **기기의 현재 설정 보기 / 변경**에서 바꿀 수 있습니다.
- `KYMCO`는 이미 있는 순정 사진을 읽기만 합니다. 사진 전송은 항상 NewDoe 슬롯에 저장됩니다. 선택한 순정 사진이 비었거나 읽을 수 없으면 해당 번호로 안내하며 원본은 건드리지 않습니다.
- 새 사진 설정 번역은 **B 언어팩 0.12.2**에 들어 있습니다. 펌웨어 확정 후 앱의 언어팩 안내를 따르세요. A의 영어 동작은 유지됩니다.

설치·순정 복귀 중 배터리 단자를 분리하거나 스타터를 누르지 마세요. 키 OFF에서 위+O를 동시에 누르지 마세요. 구형 펌웨어의 순정 복귀 도중 전원이 끊기면 자동 복구되지 않을 수 있습니다. 키 ON/OFF는 단계별 앱 안내를 따르세요.

[한국어 그림 설명서](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/Guide.md)

## English

- Use **both the 0.12.2 APK and CFW ZIP**. Fresh installations include the NEWDOE image in photo 1. Existing users' photos are preserved.
- **From 0.12.0 or 0.12.1:** install the APK over your current app, select the new CFW ZIP on the status page, check the current device and follow the normal CFW update. No stock return or pairing deletion is needed.
- Older Gate versions use the illustrated guide's **keep-data stock return → reinstall** path. There is no direct Gate OTA.
- If first installation stopped at `0x00050005 / CFWPIC.DAT`, select the new ZIP in the new app, choose **Check current device**, and follow the offered resume path. Do not erase the stock album or clear app data.
- In device system settings choose **Photo source → NewDoe / KYMCO**, then photo 1, 2 or 3. The same settings are available through **View / change current device settings** on the phone.
- `KYMCO` only reads existing stock photos. Photo uploads always write to NewDoe slots. Empty or unreadable images produce a slot-specific message; original stock files are not changed.
- Install **B language resources 0.12.2** after firmware confirmation for the translated photo settings. A continues to operate in English.

Keep battery power connected during installation/stock return. Do not operate the starter or press UP+O together with the key OFF. Power loss during an already-installed legacy stock-return routine may prevent automatic recovery. Follow each step's key ON/OFF instructions in the app.

[English illustrated guide](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/Guide.en.md)

## Downloads

- `NewDoe-0.12.2.apk`: Android app.
- `NewDoe-CFW-0.12.2.zip`: device installation package; select it without extracting.
- `NewDoe-LANGUAGE-B-0.12.2.zip`: B menu language resources.
- `NewDoe-Illustrated-Manual-0.12.2.zip`: offline Korean/English illustrated guides.
- `LANGUAGE-LICENSES.txt`, `SHA256SUMS.txt`: licenses and file hashes.
- `NewDoe-0.12.2-relink-objects.zip`: replaceable map-library objects.

The affected 2024 AK550 revision has not supplied its stock firmware dump; this release does not establish support for previously unidentified hardware. Use the app's existing compatibility check.
