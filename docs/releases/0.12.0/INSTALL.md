# NewDoe 0.12.0 beta

순정 F4 펌웨어에서 확인한 화면 초기화 분기를 추가하고, 장기 주차 후 완전 부팅할 때 언어 준비 과정의 중복 읽기를 줄였습니다. APK와 펌웨어 ZIP은 모두 **0.12.0**입니다.

- 화면: 순정 패널 선택값에 따라 기존 명령 또는 다른 순정 명령을 사용합니다. Gate·Bootstrap·Product·순정 복귀 도구에 함께 적용했습니다.
- 전원: 종료 처리가 실제 보드 리비전을 사용하도록 수정했습니다.
- 시작 대기: 같은 순정 글자를 크기별로 네 번 읽는 대신 한 번 읽어 재사용합니다. 준비 중에는 실제 단계와 진행률을 표시합니다. 언어·글꼴·메뉴는 유지합니다.
- 설치 도구: 화면 프로필 확인에 실패해도 저장소가 정상이면 기존 Bluetooth 키를 읽고 진단·복구 연결을 시도할 수 있습니다. 설치 허용 검사를 우회하지 않습니다.
- 앱: 기존 0.11.17·0.11.18 기기와의 주행 연결 호환성을 유지합니다. 이전 기기의 삭제 도구는 해당 기기가 요구하는 정확한 파일을 사용합니다.

## 설치

**이번 버전은 Gate가 변경됩니다. 기존 버전에서 처음 넘어올 때는 데이터 유지 순정 복귀 후 재설치가 필요합니다.**

1. `NewDoe-0.12.0.apk`를 기존 앱 위에 설치하세요. 앱 데이터를 지우지 마세요.
2. 주행 연동을 멈추고 `NewDoe-CFW-0.12.0.zip`을 선택하세요.
3. 앱 안내에 따라 **순정 복귀 → CFW 데이터 유지**를 선택하세요. 완전 삭제를 고르지 마세요.
4. 순정 화면이 실제로 나오면 **본체에서 순정으로 돌아왔어요 · 확인**을 누르세요.
5. 같은 ZIP으로 다시 설치하고 **보관 데이터 복원**을 고르세요. Bootstrap이 뜨면 폰에서 **현재 기기 확인**, 누도에서 **Install CFW**를 선택하세요.
6. 누도의 **CFW UPDATE** 화면을 앱에서 확인하고 설치 완료까지 기다리세요.
7. 기존 B 0.11.2 언어팩이 정상이라면 그대로 사용하세요. 이번 버전 때문에 다시 보낼 필요는 없습니다.

순정 복귀·설치 중 배터리와 누도 전원을 분리하지 마세요. 이미 설치된 구버전 코드의 순정 복귀 중 전원 차단 취약 구간은 그대로이며, 그때 전원이 끊기면 별도 복구 장비가 필요할 수 있습니다.

**이미 화면과 Bluetooth가 모두 응답하지 않는 기기에 새 APK만 설치한다고 복구되지는 않습니다.** 확인되지 않은 메뉴를 화면 없이 누르며 진행하지 말고 현재 증상을 알려 주세요.

[한국어 그림 설명서](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/Guide.md) · [English illustrated guide](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/Guide.en.md)

## English

NewDoe **0.12.0 beta** adds the alternate display command sequence found in stock F4 firmware, uses the actual board revision for power-off, and avoids rereading each stock glyph for every menu size. Startup preparation now shows its actual stage and progress. Existing language resources and menu appearance are retained.

The updated display path is included in Gate, Bootstrap, Product and the stock-return tools. Bootstrap can retain its saved Bluetooth keys for diagnostics when storage works but display qualification fails; installation checks remain enforced. The app still connects to the reviewed 0.11.17 and 0.11.18 firmware images before migration.

**This release changes Gate. Use keep-data stock return followed by reinstall when moving from an older release. There is no direct Gate OTA update.**

1. Install `NewDoe-0.12.0.apk` over the existing app and stop the riding connection.
2. Select `NewDoe-CFW-0.12.0.zip`. Follow **Return to stock → Keep CFW data**; do not choose complete removal.
3. Once the stock screen actually appears, tap **I have returned to stock · check**.
4. Install using the same ZIP and choose **Restore retained data**. Once Bootstrap appears, use **Check current status** on the phone and **Install CFW** on the device.
5. Confirm the device’s **CFW UPDATE** screen in the app, then wait for completion.
6. Keep a healthy installed B 0.11.2 language pack. Its contents have not changed.

Keep the battery and device power connected throughout stock return and installation. The accepted power-interruption limitation in already-installed legacy stock-return code remains; interruption there can require external programming equipment. A new APK alone cannot recover a unit whose screen and Bluetooth are both unresponsive.

## Validation and remaining uncertainty

The display sequences were traced in the available stock F4 firmware and checked in ARM software tests alongside boot/recovery, installation, power and language regressions. The reporting 2024 AK550 Premium unit’s panel selector, stock dump and physical test results are unavailable. This release addresses established code gaps; it does **not** establish the cause of that unit’s failure or qualify every 2024 hardware revision. Startup latency and parking current still need measurement on actual hardware.
