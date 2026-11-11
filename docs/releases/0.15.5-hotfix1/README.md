# NewDoe 0.15.5 hotfix1 — update connection handoff

**Android app 0.15.5, build 151 · included firmware 0.15.5**

## 한국어

주행 연동을 켜 두면 업데이트가 막히던 앱의 조건을 수정했습니다. 이제 별도로 주행 연동 중지 버튼을 누르지 않아도 기기 상태 확인과 업데이트를 시작할 수 있습니다.

1. 기존 앱 위에 이 APK를 설치하세요. 앱 데이터와 페어링은 유지하세요.
2. 기기 페이지에서 현재 상태를 확인하고 업데이트를 선택하세요. 상태 확인은 펌웨어를 설치하지 않습니다.
3. 경고 5장을 각각 3초 이상 읽고 확인하세요. 경고 도중 취소하면 진행 중인 주행 연동을 유지합니다.
4. 마지막 확인이 끝나면 앱이 주행 작업을 정리한 다음 설치 연결로 자동 전환합니다. 이전 연결의 늦은 응답이나 중복 누름이 새 설치 작업을 시작하지 않도록 했습니다.

**이미 본체가 0.15.5이면 APK만 갱신하면 됩니다.** 내장 펌웨어, 언어별 설치 ZIP, Gate는 이전 0.15.5와 바이트 단위로 동일합니다. 구형 본체에는 기존 절차로 0.15.5를 설치합니다. 다른 설치·복구·NOR 백업이 진행 중이면 먼저 끝내세요. 차량 정차·호환성·저장소·미완료 작업 확인은 유지합니다.

소프트웨어 회귀 시험과 APK 서명·내장 파일 해시를 검증했습니다. 실제 차량 Bluetooth 전환은 이번에 시험하지 않았습니다. 기존 순정 복귀 구간의 전원 차단 제한은 해결된 것이 아닙니다.

## English

This app hotfix removes the requirement to manually stop riding sync before checking or updating the device. The saved automatic-riding preference no longer blocks an update when no riding socket is connected.

Install the APK over the existing app without clearing data or pairing. Check the device, select Update and confirm all five warnings (three seconds per card). Cancelling a warning leaves an ongoing riding connection intact. After the final confirmation, the app closes and drains the riding session before the same worker opens the installer connection. Old replies and repeated taps cannot start a second installation.

**If your device already runs 0.15.5, update only the APK.** All embedded firmware, language installer ZIPs and Gate bytes are unchanged. Older devices still use the existing 0.15.5 installation flow. An ongoing installer, recovery or NOR backup is not interrupted. Existing stationary, identity, storage and unresolved-operation checks remain.

Software regressions, APK signature continuity and embedded-file hashes were checked. No physical vehicle Bluetooth handoff was tested. The accepted legacy stock-return power-interruption limitation remains unchanged.

Validation: 118 tests passed, 3 optional firmware-fixture tests skipped. APK assembly succeeded. Full-project lint still reports 1,135 existing translation/API diagnostics; comparison with the previous source found no new errors from this hotfix. These existing diagnostics were not suppressed or repaired in this scoped update.

[한국어 설명서](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/Guide.md#update) · [English guide](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/Guide.en.md#update)
