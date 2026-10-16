# Companion 0.10.3 beta — language setup and optional Bootstrap connection help

**Already on CFW 0.10.2? Install only NoodoeCompanion-0.10.3.apk over the existing app. Keep app data. Do not reinstall firmware or return to stock for this fix.**

The bundled firmware is intentionally still **NoodoeInstaller-CFW-0.10.2.zip**, byte-identical to the previous release. Product, Bootstrap, Gate and recovery images have not changed. The language archive remains NOODOE-LANGUAGE-B-0.10.1.zip.

## Firmware routes — unchanged

- 0.10.x with the matching Gate: normal CFW-to-CFW update. Exact Gate, identity and storage checks remain; a version label never bypasses them.
- 0.9.x / older Gate to 0.10.x: keep-data stock return, Check current device, Bootstrap, Restore stored data, install and confirm CFW. Do not choose full erase.
- Stock F4 5.14/5.16: existing first-install workflow.

## Language installation

After the CFW UPDATE screen confirmation and durable Product confirmation, the app checks the device language resources. If a matching pack is missing, it explicitly asks **Install the language pack?** with **Install / Later**. Choosing Install downloads the matching archive, verifies its pinned size/hash and contents, then runs the existing FONT inspection and resource transfer. FONT repair still requires its separate explicit choice. No automatic pairing reset or firmware write is added.

Already on 0.10.2 and missing languages: open **Device language · Install language pack** near the top of the app, stop riding sync, keep/select the 0.10.2 firmware ZIP, and choose **Install / resume language resources**. Confirm the language offer. You do not need to find a separate ZIP yourself. Manual language-ZIP import remains available in Installation for offline use. A healthy matching pack on the device is reused even if the phone has no language archive.

The language setup panel stays available when ordinary tabs are hidden and its result is not replaced by automatic riding connection messages. After resource installation, reconnect for riding, disable English only if previously enabled, choose Device language / Follow app, and accept the stationary device restart. Manual language and English-only choices are not overwritten. B supports nine device languages; A remains English.

Download/transfer interruption does not invalidate confirmed firmware. Use the same device and firmware ZIP to resume language setup. Actual device hashes and written pages, not a phone completion flag, determine reuse.

## Transfer speed

LANGUAGE.PAK is 11,965,392 bytes (about 11.41 MiB); the downloadable archive is 9,318,242 bytes and includes verified FONT backups. The uploader previously slept an extra 10 ms whenever a queued command required a status query. It now queries immediately once, then backs off 2 / 5 / 10 ms when storage has not advanced. One request remains outstanding at a time. Page hashes, per-page verification, full-file hash, publication and restart checks remain unchanged.

Software tests confirm the fixed per-command sleep is gone for responsive storage. This is not a measured real-device installation time or a guarantee of a particular speedup. Flash erase/program time and Bluetooth round trips still limit throughput; chunk pipelining would require a separately validated device protocol change.

## Optional Bootstrap connection help

Tap **Cannot connect to Bootstrap?** only when you need help. Opening the guide does not change a connection or pairing. From its Bluetooth settings action, pause the failed connection attempt; forget this Noodoe on the phone; on Bootstrap use **UP → your phone → release O → hold O for 2 seconds** to remove that device-side pairing; pair again on the phone; then **Check current device → Continue from here**. Keep app data and the ZIP. Do not resend Bootstrap. ALL deletes every phone and is not the default recommendation.

Connection actions in this guide are disabled while firmware transfer/writing is active. Existing successful automatic connections continue unchanged. Bootstrap code and its pairing algorithm are unchanged.

## Recovery limit

The previously accepted old-firmware stock-return power-interruption limitation remains. Keep permanent power connected throughout that transition and reinstallation; do not operate the starter. IGN OFF is not removal of permanent power. No physical vehicle, radio or power-cut qualification was performed for this app update.

## 한국어

**본체가 이미 0.10.2이면 APK만 0.10.3으로 올리세요. 펌웨어 재설치나 순정 복귀는 필요 없습니다.** 설치 ZIP은 내용이 같아 0.10.2를 그대로 제공합니다.

- 0.10 계열은 같은 Gate에서 CFW→CFW 업데이트, 0.9→0.10은 데이터 유지 순정 복귀 후 재설치를 유지합니다.
- CFW UPDATE 화면 확인과 정상 실행 확정이 끝난 뒤 **언어팩을 설치할까요? → 설치 / 나중에**를 묻습니다. 설치를 선택하면 앱이 맞는 파일을 내려받아 검증하고 전송합니다. 폰트 복구가 필요한 경우에는 별도로 동의를 받습니다.
- 현재 0.10.2에서 언어팩만 설치하려면 앱 상단 **기기 언어 · 언어팩 설치 → 주행 연동 중지 → 언어 자원 설치 / 이어가기**를 누르세요. 펌웨어 ZIP은 0.10.2를 선택한 상태로 두세요. 언어 ZIP을 직접 찾을 필요는 없습니다.
- 설치 후 주행 연결을 다시 시작하고, 기기 설정에서 영어 전용이 켜져 있으면 끈 뒤 원하는 언어 또는 앱 언어 따르기를 선택하고 본체 재시작을 확인하세요. 이미 정상 설치된 팩은 다시 보내지 않습니다.
- 언어 전송의 매 명령 뒤 고정 10ms 대기를 응답 상태에 따라 줄였습니다. 검증·이어가기·완료 표식은 그대로입니다. 실기 설치 시간이 몇 분 줄었는지는 아직 측정하지 않았습니다.
- **Bootstrap에 접속이 안 되나요?**를 눌렀을 때만 양쪽 페어링 삭제·재연결 방법을 안내합니다. 안내를 여는 것만으로 초기화하지 않고 Bootstrap 코드도 바꾸지 않았습니다.

구형 CFW의 순정 복귀 도중 상시 전원이 끊기는 기존 제한은 그대로입니다. 전환·재설치 중 전원을 유지하고 시동 모터를 돌리지 마세요.
