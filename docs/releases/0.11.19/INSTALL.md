# NewDoe app 0.11.19 / CFW 0.11.18

**0.11.18 앱에서 0.11.17 본체에 연결할 때 앱이 연결을 끊던 문제를 수정했습니다.** 앱을 먼저 업데이트하세요.

- 앱 0.11.19는 본체 0.11.17과 0.11.18에 모두 연결됩니다.
- 본체가 이미 0.11.18이면 APK만 설치하면 됩니다. 펌웨어·언어팩 재전송은 필요 없습니다.
- 본체가 0.11.17이면 새 앱으로 연결한 뒤, 주유 감지 수정이 포함된 기존 `NewDoe-CFW-0.11.18.zip`으로 일반 CFW 업데이트를 진행하세요.
- 이번 수정에 순정 복귀, Bluetooth 페어링 삭제, 앱 데이터 삭제는 필요 없습니다.

## 따라 하기

1. `NewDoe-0.11.19.apk`를 기존 앱 위에 설치합니다.
2. 앱을 열고 기존 누도를 선택한 뒤 HOME의 연결 버튼을 한 번 누릅니다. 이전 오류로 자동 연결이 멈췄다면 이 동작으로 다시 시작합니다.
3. 본체가 0.11.17인 경우 설치/상태 화면에서 `NewDoe-CFW-0.11.18.zip`을 선택하고 일반 CFW 업데이트를 진행합니다. 먼저 주행 연동을 멈춰야 한다는 안내가 나오면 상단 정지 버튼을 누르세요.
4. 안전 안내를 확인하고 전송·설치·재부팅을 기다립니다. 누도에 `CFW UPDATE`가 나타나면 앱에서 완료를 확인합니다.

연결이 계속 안 되면 표시된 오류 문구와 진단 파일을 함께 전달해 주세요. Bluetooth 자체의 페어링 오류와 앱의 버전 검사 오류는 다른 문제입니다.

## English

**App 0.11.18 could close a working Bluetooth connection to a device still running firmware 0.11.17. Install app 0.11.19 first.**

The new app accepts both reviewed, protocol-compatible firmware images. Installation confirmation still requires the exact firmware selected in the ZIP; an old running image cannot be reported as a completed update.

1. Install `NewDoe-0.11.19.apk` over the existing app, keeping its data and pairing.
2. Select your existing Noodoe and tap the Home connection button once to resume a connection stopped by the previous version check.
3. Already running device firmware 0.11.18? The APK is all you need. Otherwise, use the unchanged `NewDoe-CFW-0.11.18.zip` through normal CFW update. Stop riding linkage first if prompted.
4. Follow the safety screens and confirm the device’s `CFW UPDATE` screen after restart.

No stock return, re-pairing, language retransmission or device settings reset is needed for this fix. Gate, Bootstrap, Product and language bytes are unchanged from the 0.11.18 release. The previous legacy stock-return power-interruption limitation is unchanged; this app update does not enter that path.

[한국어 그림 설명서](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/Guide.md) · [English illustrated guide](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/Guide.en.md)

## 확인 범위 / Verification scope

0.11.17의 실제 배포 해시를 넣은 응답으로 기존 앱의 거절을 재현하고 수정 후 연결 성공을 검사했습니다. 시험부팅·미지원 이미지·역할·통신 형식 검사는 유지되며 구버전이 새 설치 완료로 처리되지 않는 회귀 검사도 수행합니다. 해당 사용자의 휴대폰과 차량에서 무선 연결을 직접 재현한 것은 아닙니다.

The old refusal was reproduced with the actual released 0.11.17 image identity, then tested through the full riding handshake. Trial, role, schema and unknown-image guards remain, as does exact-candidate update confirmation. This is software validation, not a physical Bluetooth test on the reporting device.
