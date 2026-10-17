# NewDoe 0.11.0: startup, recovery and parking

NewDoe is the new name of this firmware and Android app. Install the matching APK and ZIP. Android package identity, signing key, saved settings and language content remain compatible. Use the new APK for riding; pre-0.10 riding message formats are no longer accepted. Installer/stock/update compatibility remains.

## First installation of this Gate

This release replaces Gate. **0.9.x and 0.10.x both require one keep-data stock return and reinstallation**, even though ordinary updates between matching 0.10.x Gates previously worked. The existing installer compares the actual Gate image; it never overwrites Gate through a Product-only update.

1. Install NewDoe APK 0.11.0 over the existing app without clearing its data. Select the same module and matching ZIP, and stop riding integration.
2. Follow the existing **return to stock, keep CFW data** route. Check the current device and confirm stock. Stock 5.14/5.16 remain the installation targets; no new model or bootloader whitelist is introduced.
3. Send Bootstrap from the 0.11.0 ZIP and choose **Restore stored data**. Keep permanent power connected throughout. If Bootstrap cannot connect, use the existing connection-help route: unpair in Android settings, reset pairing on Bootstrap as instructed, pair again, then **Check current device → Continue**. No mandatory re-pair completion flag was added.
4. Complete installation, normal startup and the actual **CFW UPDATE** confirmation. Transfer 100% alone is not completion.
5. Healthy existing B language resources are reused. If resources are missing, accept the separate language-resource offer after firmware confirmation. The optional offline archive is NewDoe-LANGUAGE-B-0.11.0.zip; its text/font bytes and hashes are unchanged from 0.10.7.

Future updates with this same Gate can use the normal CFW-to-CFW path. A different Gate still requires migration. The original resident bootloader is not replaced.

**Known legacy limitation:** if permanent power is lost while older CFW writes its stock-return request, boot may stop before the new Gate runs. Automatic recovery is not guaranteed in that old-code interval. Keep permanent power connected, avoid operating the starter during installation, and retain diagnostics if it stops. IGN OFF is not removal of permanent power.

## What Gate shows

An ordinary startup shows NewDoe and its logo for about two seconds, then checks storage, firmware and the boot record before entering Product. A post-update installation restart skips the logo delay. Errors detected during checks enter the error screen. Holding DOWN at cold boot, or for one second during the logo window, requests recovery; the existing O entry remains available.

An error first shows the skull, the existing numeric code formatted as **0x0000**, and **Press Any Key... 30** counting down. A fresh button press or thirty seconds opens the recovery menu. That press only opens the menu. The reason is shown as **[code] [short reason]**; manual entry leaves the code column blank. Details also retains the latest operation error.

Eight rows always stay in place: Start NewDoe, Check again, Reinstall firmware, Restore previous, Return to stock, Details, Help, Restart. Unavailable actions are grey and explain why when selected. Only verified sources enable recovery. Start cannot bypass a failed trial or boot-attempt limit. Reinstall, previous-image restoration, stock restoration and restart require releasing O and holding it again for two seconds; UP/DOWN cancels. Checking never erases files. A pending publication is resumed by Restart, not falsely described as a dead flash chip.

Gate has **no Bluetooth, LVGL, external-font or SDRAM dependency**. Its artwork and help QR are in internal flash. If local recovery sources cannot be verified after bounded retries, the menu says no on-device recovery is available. Help links to this project's GitHub and explains programmer recovery after opening the module, or replacement if repair is not possible. An invalid image alone does not prove physical chip damage. If internal Gate itself cannot execute or the display hardware has failed, it cannot show these instructions.

## Parking and battery use

Existing power-sequence settings and timings are retained. Connection retention keeps Bluetooth available; the deepest parking stage disables Bluetooth, so proximity reconnection and welcome light cannot work there. Set these stages in **Settings → System → Power sequence**. Existing defaults are sixty minutes of screen standby followed by sixty minutes of Bluetooth retention, then deep sleep; saved user values are not reset.

The idle input supervisor now allows 100ms waits while parked, instead of waking every 10ms and defeating the 16ms minimum for STOP. Button interrupts wake it immediately; debounce/held-button processing stays at 10ms. Incomplete RAM-only resource reception expires after sixty seconds without progress. NOR publication is allowed to finish and never cancelled just because IGN changes.

In Gate with IGN OFF, an untouched error screen lasts thirty seconds, then the menu remains for the rest of the sixty-second idle period. Gate then shuts down the panel and uses short RTC STOP intervals to service the watchdog. RTC-only wakes do not turn the display back on or restart Bluetooth. The first button press wakes the screen without selecting anything. A missing/unusable RTC uses bounded timer-driven ordinary sleep instead of a busy loop; this fallback consumes more power than STOP.

Software checks verify state transitions and simulated wake sequences, not battery current. There is **no measured standby-current or one-week battery-life guarantee** for this release. Validate parked current after the configured deep-sleep delay, and check IGN/button wake and retained settings on hardware. A week-later battery voltage alone does not identify which circuit drew the current.

---

# NewDoe 0.11.0: 부팅·복구·주차 절전

이번부터 펌웨어와 앱의 이름은 NewDoe예요. APK와 ZIP을 같은 버전으로 설치하세요. 앱 패키지·서명·설정 저장 형식·언어팩 내용은 유지합니다. 주행 연동은 새 APK를 사용해야 해요. 0.10 이전의 주행 메시지 형식은 제거했지만, 구버전 확인·업데이트·순정 복귀 호환 경로는 유지했어요.

## 이번 Gate를 처음 설치할 때

**Gate가 바뀌므로 0.9.x와 0.10.x 모두 한 번은 데이터 유지 순정 복귀 후 재설치가 필요해요.** 앱은 실제 Gate 이미지를 비교하며, Product 업데이트로 Gate를 덮어쓰지 않아요.

1. 기존 앱을 지우거나 데이터를 초기화하지 말고 NewDoe APK 0.11.0을 덮어 설치하세요. 같은 본체와 0.11.0 ZIP을 선택하고 주행 연동을 중지하세요.
2. 기존의 **CFW 데이터 유지 → 순정 복귀**를 진행한 뒤 **현재 기기 확인**으로 순정 상태를 확인하세요. 순정 5.14/5.16 설치 지원과 기존 모델·부트로더 판정은 유지합니다.
3. 0.11.0 ZIP의 Bootstrap을 보내고 **보관 데이터 복원**을 선택하세요. Bootstrap 연결이 안 되면 기존 연결 도움말대로 휴대전화에서 페어링 해제, Bootstrap의 페어링 초기화, 재페어링을 거쳐 **현재 기기 확인 → 계속하기**로 진행하세요. 재페어링 완료를 강제하는 별도 기록은 만들지 않았어요.
4. 설치와 재시작 후 실제 **CFW UPDATE** 화면을 확인하고 정상 실행 확정까지 기다리세요. 전송 100%만으로 끝난 것은 아니에요.
5. 정상 B 언어팩은 다시 보내지 않고 재사용해요. 자원이 없으면 펌웨어 확정 후 언어팩 설치 제안을 수락하세요. 오프라인 파일은 NewDoe-LANGUAGE-B-0.11.0.zip이며, 문구·폰트 내용과 해시는 0.10.7과 같아요.

이 Gate를 설치한 뒤 같은 Gate를 사용하는 업데이트는 CFW→CFW로 할 수 있어요. 순정 상주 부트로더는 교체하지 않습니다.

**이미 알려진 구버전 전환 제한:** 구버전 CFW가 순정 복귀 요청을 기록하는 도중 상시 전원이 끊기면, 새 Gate가 실행되기 전에 부팅이 막힐 수 있어요. 이 구간의 자동 복구는 보장하지 않습니다. 순정 복귀·재설치 중 상시 전원을 유지하고 스타터를 조작하지 마세요. IGN OFF는 상시 전원 제거와 달라요.

## Gate 화면과 조작

일반 부팅에서는 NewDoe 로고를 약 2초 표시한 뒤 저장소·펌웨어·부팅 기록을 확인하고 Product로 들어가요. 업데이트 직후 설치 재시작에서는 로고 대기를 건너뜁니다. 검사에서 오류가 확인되면 오류 화면으로 넘어가요. 전원 투입 때 DOWN을 누르고 있거나 로고 표시 중 1초 누르면 복구로 들어갑니다. 기존 O 진입도 유지해요.

오류가 나면 **해골 → 기존 오류 번호를 0x0000 형식으로 표시 → Press Any Key... 30 카운트다운**이 먼저 나와요. 새 버튼 입력 또는 30초 경과 후 복구 메뉴가 나옵니다. 그 입력으로 메뉴 항목까지 실행하지는 않아요. 사유는 **[코드] [짧은 설명]**으로 표시하며, 사용자가 직접 들어온 경우 코드는 비워 둡니다. Details에는 가장 최근 작업 오류도 남아요.

메뉴는 Start NewDoe / Check again / Reinstall firmware / Restore previous / Return to stock / Details / Help / Restart의 8줄로 고정돼요. 사용할 수 없는 항목은 회색이며 선택하면 이유를 보여 줍니다. 검증한 이미지가 있을 때만 복구를 허용해요. Start로 실패한 시험 부팅이나 부팅 횟수 제한을 우회할 수 없어요. 재설치·이전 펌웨어·순정 복구·리셋은 O를 뗀 뒤 다시 2초 눌러 확정하며 위/아래로 취소합니다. 재검사는 파일을 지우지 않아요. 저장 작업이 미완료라면 Restart로 재개하도록 안내합니다.

Gate에는 블루투스가 없고 LVGL·외장 폰트·SDRAM도 사용하지 않아요. 로고·해골·GitHub QR은 내부 플래시에 들어갑니다. 반복 확인 후에도 검증된 복구 소스가 없으면 기기 내 복구 불가를 표시하고, Help에서 본체 분해 후 프로그래머 복구 또는 수리가 불가능할 때 교체를 안내해요. 이미지 오류만으로 칩 고장이라고 단정하지 않습니다. 내부 Gate 자체가 실행되지 않거나 디스플레이 하드웨어가 고장 나면 화면 안내도 할 수 없어요.

## 절전과 방전 확인

기존 Power sequence 설정과 시간은 그대로예요. 연결 유지 단계에서는 블루투스를 유지하고 가장 깊은 주차 절전에서는 끕니다. 깊은 절전 중에는 휴대전화 접근 자동 연결·웰컴 라이트가 작동하지 않아요. **설정 → 시스템 → Power sequence**에서 조절하세요. 기존 기본값은 화면 대기 60분, 블루투스 유지 60분, 이후 깊은 절전이며 저장된 사용자 값을 초기화하지 않아요.

주차 중 입력 감독 작업의 대기를 100ms까지 허용해, 10ms마다 깨면서 STOP의 최소 16ms 조건을 막던 문제를 고쳤어요. 버튼 인터럽트는 즉시 깨우며 버튼 디바운스·누름 유지 처리는 10ms를 유지해요. RAM으로만 받던 미완료 자원 전송은 실제 진행 없이 60초가 지나면 정리합니다. NOR 저장 중에는 IGN 변경을 이유로 작업을 버리지 않아요.

IGN OFF인 Gate 오류 화면은 30초 후 메뉴로 넘어가고, 최초 오류 표시부터 총 60초 동안 조작이 없으면 패널을 끄고 RTC STOP으로 대기해요. 워치독을 위한 RTC 깨어남은 화면이나 블루투스를 다시 켜지 않습니다. 첫 버튼 입력은 화면만 깨워요. RTC를 쓸 수 없으면 타이머 기반 일반 Sleep으로 대기하며, 이 경우 STOP보다 전력을 더 사용해요.

소프트웨어 검사로 상태 전이와 가상 깨우기 순서를 확인했습니다. **실기 대기전류나 일주일 배터리 유지 시간은 아직 측정하지 않았어요.** 설정한 깊은 절전 시간이 지난 뒤 전류, IGN/버튼 깨우기, 설정 유지를 실기에서 확인해야 해요. 일주일 뒤 배터리 전압 하나만으로 방전 원인을 확정하지는 않습니다.
