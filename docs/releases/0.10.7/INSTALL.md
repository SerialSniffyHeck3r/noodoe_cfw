# CFW + Companion 0.10.7 beta — language activation and split resources

Install **NoodoeCompanion-0.10.7.apk** and **NoodoeInstaller-CFW-0.10.7.zip** together. Keep app data. This release fixes language preparation after saved settings are restored, distinguishes resource storage from actual menu readiness, and reduces internal flash use within the language feature.

## Updating / 업데이트

1. Install APK 0.10.7 over the existing app and select ZIP 0.10.7. Matching-Gate 0.10.x devices use the ordinary CFW-to-CFW update. This fix does not require stock recovery.
2. Confirm actual CFW UPDATE / normal firmware startup before optional language installation.
3. A healthy installed language pack is reused. **You do not need to resend the 11.4MiB pack just for this update.** For a missing or incomplete pack, use Install / resume language resources. The app downloads the matching resource archive; importing it manually is an offline alternative.
4. Leave English only OFF and choose the device language. Existing restart confirmation, save and reset behavior is retained. No unrelated phone button is needed to start preparing the saved language.

APK와 ZIP을 모두 **0.10.7**로 업데이트하세요. 같은 Gate의 0.10 계열은 기존 CFW→CFW 경로를 사용해요. 앱 데이터·설정·정상 설치된 언어팩은 유지하세요. 이 수정 때문에 순정으로 돌아가거나 언어팩 전체를 다시 보낼 필요는 없어요.

CFW UPDATE와 정상 실행을 먼저 확인하세요. 언어팩이 없거나 미완료인 경우에만 **언어 자원 설치 / 이어가기**를 사용하세요. 영어 전용을 끄고 원하는 언어를 선택하면 기존 재시작 안내를 통해 적용해요. 앱의 다른 버튼을 누를 필요가 없어요.

## What changed

- Saved device language is captured after configuration restore, including delayed restore and dark/parked startup. Restart prompts remain available while a language is loading or the UI has fallen back to English.
- After pack publication and automatic restart, the app checks real menu preparation. Checking storage, loading text, loading glyphs, ready, explicit English-only and fallback are distinct states. Status requests carry the current Bluetooth connection epoch.
- Language choices show native names: English, Deutsch, Español, Français, Italiano, Ελληνικά, 한국어, Tiếng Việt, 日本語.
- Stock-font trip units no longer align using invisible bottom rows. Advance widths and visible glyph pixels are preserved.
- Redundant language FONT scanning and metadata are shared. Both Product builds leave **1,124 bytes** free; the 1,024-byte hard minimum, compiler policies, RAM reservations and existing assets are unchanged.

저장 설정 복원이 늦어질 때 영어로 남는 경로와, 준비 중이면 재시작 안내가 사라지는 조건을 수정했어요. 앱은 팩의 저장 완료와 메뉴의 준비 완료를 구분해 보여 줘요. 언어 이름은 각 언어의 표기로 표시하고, 순정 폰트를 쓰는 트립 단위의 세로 위치도 조정했어요. 언어 기능 내 중복 코드·메타데이터를 줄여 이번 수정 직후 빌드보다 824바이트를 절약했어요.

## Resource layout

**NOODOE-LANGUAGE-B-0.10.7.zip** contains TEXT.PAK (112,400 bytes, all nine menu languages) and FONTS.PAK (11,852,992 bytes, common glyphs/emoji/indexes). No language or glyph coverage was removed.

The two parts form one verified, atomically published **LANGUAGE.PAK** on NOR. Keeping the existing on-device contract preserves Gate recovery and compatibility. The combined pack SHA-256 is unchanged: 590c268eb3da187a7fb0477f16e4872421cd28957f7a46fdb262a89937789390.

For a changed pack, unchanged pages at matching offsets can be copied from the old verified chain into the new allocation; only differing pages need payload transfer. The old live file is preserved until verification and publication. This is not a promise that every future text change transfers text alone: changed section lengths may shift later pages.

문구는 **9개 언어 모두** 포함하고 공통 폰트도 그대로 유지해요. 배포 파일은 분리했지만 기기에서는 기존 Gate가 복구할 수 있는 LANGUAGE.PAK 하나로 확정해요. 글꼴이나 메뉴 문구를 STM32 내부 플래시에 넣지 않아요. 새 팩을 기록할 때는 검증된 새 영역에 기록한 뒤 교체하므로 다른 파일을 지우지 않아요.

## Installation conditions and limits

IGN ON/OFF does not pause language inspection, transfer, verification or saving. Keep permanent module power connected. Fresh positive vehicle speed retains the existing stationary restriction. Pack transfer still ends with validation, journaled publication, automatic restart and actual menu-ready checks. Interrupted language installation can be resumed without repeating firmware installation.

Gate, Bootstrap, pairing and the firmware transfer protocol are unchanged. Devices on 0.9.x or an older Gate still follow the documented keep-data stock-return/reinstallation route. **The accepted legacy limitation remains:** a power interruption while older CFW records a stock-return request can prevent boot before new recovery code runs. Automatic recovery is not guaranteed in that interval.

Software tests and rendered firmware frames passed; no physical scooter, Bluetooth timing, battery-current or power-cut qualification was performed. See VALIDATION.md. B supports nine device languages; A remains English-only. English wording, Lato assets and ordinary layouts remain.

시동 ON/OFF와 무관하게 진행하지만 상시 전원은 유지해 주세요. 0.9 계열·구형 Gate는 기존 순정 복귀 후 재설치 경로를 사용해요. 구버전이 순정 복귀 요청을 기록하던 중 전원이 끊기는 구간의 자동 복구는 여전히 보장하지 않아요. 이번 검증은 소프트웨어 기준이며 실차 검증 완료를 의미하지 않아요.
