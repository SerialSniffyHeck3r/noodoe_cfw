# NewDoe 0.15.4 Hotfix 3 — Android build 147

## 한국어

최신 진단에서 확인한 업데이트 준비·연결 상태 표시 문제를 수정했습니다. Bluetooth 연결 뒤 별도로 수행하던 순정 폰트 확인도 진단에 기록하고, 정상 응답을 받으면 “마지막 응답” 시간을 갱신합니다. NOR 폰트 확인과 저장 언어 읽기를 연결 대기와 구분해 표시합니다. 확인 단계에서 중단되면 이번 작업은 읽기 전용이었음을 명시하며, 이전 미완료 설치를 성공 처리하지 않습니다.

완료된 구버전 설치 파일이 선택돼 있을 때 “설치 계속 / 언어 확인”이 구버전 확인만 반복하던 경로도 수정했습니다. 진행 중인 작업이 없으면 최신 패키지를 준비하고 사용자의 설치 선택을 기다립니다. 미완료 작업과 전문가가 직접 선택한 ZIP은 기존 정책을 유지합니다.

1. 새 APK를 기존 앱 위에 설치하세요. 앱 데이터나 페어링을 지우지 마세요.
2. **현재 기기 상태 확인**을 누르고, 준비된 언어·업데이트 내용을 확인하세요.
3. 본체가 0.12.1 등 구버전이면 표시되는 정상 업데이트를 진행하세요. 본체가 이미 0.15.4이면 APK만 갱신하면 됩니다.
4. 여전히 확인에 실패하면 진단 ZIP을 저장하세요. APK 빌드 번호와 두 번째 확인 단계, 응답하지 않은 명령도 기록됩니다.

## English

The second connection used for stock-font inspection now participates in diagnostics and response timing. The app distinguishes a connected NOR/profile read from a Bluetooth connection attempt. A failed read-only inspection does not claim that firmware installation failed or that an earlier unfinished attempt succeeded.

Continuing a completed old package selection now prepares the current package instead of repeatedly confirming the old firmware. Installation still requires a separate user choice. Genuine unfinished attempts retain their exact package and journal; expert-selected ZIPs keep their existing policy.

Install the APK over the existing app, use **Check current device status**, review the language and update, then start the normal update for an older dashboard. If the dashboard already runs 0.15.4, only the APK needs updating. Do not clear app data or pairing. If inspection still fails, save diagnostics; they now include the app build, second inspection stage and unanswered command.

## Scope and limits

The private diagnostic archive shows successful secure Bluetooth connections and repeated confirmation of the old 0.12.1 package. It did not record the second inspection phase, so it cannot establish the physical cause of the screenshot showing 73 seconds without a response. This patch fixes the confirmed code defects; it does not claim to resolve every Bluetooth fault. No new physical phone/vehicle verification was available.

Firmware, Gate, Bootstrap, fonts and installer ZIPs remain byte-identical to 0.15.4. Previous notification and legacy-profile fixes are included. Exact firmware matching, Gate compatibility, setting snapshots and recovery guards remain enabled. The previously accepted legacy stock-return power-interruption limitation remains unchanged.

Matching APK, expert installation ZIPs, illustrated Korean/English manuals, licenses, relink objects and hashes accompany this release. Diagnostics and source remain private.
