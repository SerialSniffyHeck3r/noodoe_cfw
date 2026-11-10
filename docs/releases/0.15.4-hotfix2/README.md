# NewDoe 0.15.4 Hotfix 2 — legacy stock resource identification

**APK 0.15.4 / Android build 146. Embedded firmware remains 0.15.4. Includes Hotfix 1.**

## 한국어

0.12.1에서 업데이트 준비 중 `The stock font profile is unknown; no write was started`가 표시되는 보고에 대응하는 앱 패치입니다. 구버전의 언어 상태가 A/B 정보를 0으로 반환하면 기존 앱은 바로 중단했습니다. 이제 이 경우에만 기존 읽기 전용 NOR 명령으로 실제 순정 자원을 확인합니다. FAT 사본·파일 할당·읽기 응답과 자원의 반복 읽기를 확인한 뒤 프로필을 선택합니다. 차종·연식·휴대폰 언어로 추측하지 않습니다.

1. `NewDoe-0.15.4-hotfix2.apk`를 기존 앱 위에 설치합니다. 앱 데이터와 페어링을 지우지 않습니다.
2. **현재 기기 상태 확인**부터 다시 진행하고, 안내되는 메뉴 언어와 설치 내용을 확인합니다.
3. 본체가 아직 0.12.1이면 정상 업데이트를 완료합니다. 이미 본체가 0.15.4이면 APK만 갱신하면 됩니다.

구버전 본체에서 새 앱의 주행 연동이 시작되지 않는 것은 현재 펌웨어 일치 검사에 따른 동작입니다. 이 검사를 해제하지 않았으며, 업데이트 준비 경로를 보완했습니다. 알려진 A/B 값이 있는 기기와 0.14 이후의 새 폰트 상태 경로는 그대로 사용합니다. 새 폰트 상태에서 프로필이 충돌하면 구형 경로로 우회하지 않습니다.

실제 자원 손상, 불명확한 프로필, 진행 중인 NOR 백업, 오래된 응답이나 통신 실패는 쓰기 전에 중단합니다. 이 식별 과정은 NOR에 쓰지 않으며 전체 백업 모드·초기화·재부팅에 들어가지 않습니다. Gate 검증, 설정 보존과 기존 복구 절차도 유지합니다.

## English

This app patch addresses the preparation failure `The stock font profile is unknown; no write was started` reported when updating from 0.12.1. Previously, a zero A/B identity from the legacy language-status cache ended preparation immediately. In that case only, the app now inspects the real stock resource through the existing read-only NOR endpoint. It checks FAT copies, file ownership, correlated read replies and repeat reads before selecting a profile. It never guesses from the scooter model, year or phone language.

Install the new APK over the existing app, use **Check current device status**, review the language/install choice, then complete the normal update if the dashboard still runs 0.12.1. A dashboard already running 0.15.4 needs only the new APK. Do not clear app data or pairing.

Riding sync still requires this app's matching firmware. This patch repairs the preparation path rather than sending current riding data to older firmware. Known legacy profiles and the modern font-state path remain unchanged. Modern conflicts never fall back to a legacy identity. Actual damage, uncertain identity, active backups, stale responses and transport failures still stop the operation before writes. Existing Gate checks and settings preservation remain intact.

## Verification scope

The old code reproduces the exact reported error with a zero legacy identity and valid physical A/B resources. The patched code identifies both profiles without any write/reset command, and rejects damaged FAT, cross-links, ambiguous resources, changed content, stale responses and active backups. The released 0.12.1 NOR command implementation was checked against the app reader. Its released Product hash remains update-required for riding sync.

The user's diagnostic ZIP has not been received: the reason that particular device reports no cached profile remains unconfirmed. An absent supplemental language pack alone does not prove that condition. Tests are software-based; no new physical phone/vehicle validation is claimed.

All firmware, Gate, Bootstrap, language variants and NOR resources are byte-identical to 0.15.4. Hotfix 1's notification drawing fix is retained. The previously accepted legacy stock-return power-interruption limitation remains unchanged.

Use the APK for normal installation; ZIPs are matching expert-use artifacts. Updated Korean/English illustrated manuals, licenses, replaceable-library objects and exact SHA-256 hashes are included.
