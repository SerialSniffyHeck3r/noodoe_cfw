# FuckNudo 0.10.2 beta — vehicle UART initialization

This release fixes two confirmed firmware defects found while investigating a
2021 Xciting 400 report: buttons and phone features worked, but the speed ring,
ODO and trips stopped updating. The affected scooter has not yet been tested
with this build; this is not a claim that every Xciting revision is validated.

## Changes

- Own PE3 during vehicle UART initialization, set it HIGH and wait 20 ms before
  UART5 starts, matching the analyzed stock firmware. Lower it after successful
  sleep shutdown. A wake repeats initialization. Active/uncertain TX DMA still
  prevents a pin-state change or unsafe buffer reuse.
- A missing/invalid light calibration table no longer disables vehicle
  communication. Automatic light data remains explicitly unavailable with the
  existing initial index-zero fallback. Valid calibration, overrides and alerts
  retain their behavior. No vehicle packet format or ODO policy was guessed.
- Product and Bootstrap include the fix. Gate, stock recovery image, language
  resources, Bluetooth/pairing logic and existing UI behavior are unchanged.

## Install / update

1. Install **NoodoeCompanion-0.10.2.apk** over the existing app, keeping app data.
2. Select **NoodoeInstaller-CFW-0.10.2.zip** while stopped.
3. From **0.10.1**, use the normal CFW update menu. Its Gate already matches;
   this fix does not require a stock roundtrip or new pairing.
4. From **0.9.x / an older Gate**, the existing one-time transition remains:
   **keep-data stock return → Check current device → Bootstrap from this ZIP →
   Restore stored data → install CFW**. Do not select full erase/uninstall.
   F4 stock 5.14/5.16 is accepted subject to existing identity/storage checks.
   If Bootstrap reconnect fails, manually forget/re-pair, then use
   **Check current device → Continue from here**. Do not clear app data.
5. Confirm Product startup. If the device requests a reset, follow its UP+O
   simultaneous 3-second instruction. Transfer 100% alone is not confirmation.

**Legacy transition limitation:** keep permanent power connected throughout
stock return/reinstallation and do not operate the starter. A power cut while
already-installed old firmware records stock return can prevent the new code
from running; automatic recovery is not guaranteed there. IGN OFF is not
removal of permanent power. This previously accepted beta limitation remains.

The B language archive is deliberately still **NOODOE-LANGUAGE-B-0.10.1.zip**:
its contents and contract are unchanged. An installed healthy pack needs no
retransfer. New users can install/resume it after confirming the new Product.
B retains nine menu languages; Android retains eleven. A retains English.
The [language manual](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/User/35-Languages.en.md)
describes rendering preferences, FONT recovery and English-only fallback.

## If vehicle information still does not update

GPS/button operation does not prove that the separate vehicle UART works.
Changing ODO preference alone cannot explain or repair a stationary speed ring.
When safely stopped, record Settings → Debug: Dashboard UART, Dashboard frames,
UART speed/fuel and Ignition input, and report the model/year/firmware version.
Do not operate menus while riding. A remaining failure needs the affected
unit's received-frame/initialization evidence before changing its protocol.

The ignition-off clock/photo holding behavior is unchanged. A low-brightness
photo alone does not establish whether the backlight is electrically off;
actual battery draw has not been measured by this release's software checks.

## 한국어

순정에 있던 **PE3 HIGH → 20ms 대기 → 차량 UART 초기화**를 CFW에 반영했습니다.
조도 보정표가 잘못됐다는 이유로 차량 통신 전체를 차단하던 처리도 고쳤습니다.
해당 Xciting 실물에서 속도·ODO가 복구됐는지는 후속 확인이 필요합니다.

**0.10.1에서는 일반 CFW 업데이트**로 올리면 됩니다. Gate와 언어팩이 같아
순정 복귀나 언어팩 재전송은 필요 없습니다. **0.9.x/구형 Gate**는 기존처럼
데이터 유지 순정 복귀 후 재설치합니다. 이전 코드에서 순정 복귀 요청을 쓰다
상시 전원이 끊기면 자동 복구를 보장하지 못하는 제한은 그대로입니다.

APK와 펌웨어 ZIP은 0.10.2를 함께 사용하세요. 언어 ZIP은 내용이 같은 0.10.1을
그대로 제공합니다. 연결·지도·기존 화면과 ODO 계산 방식은 바꾸지 않았습니다.
문제가 남으면 정차한 상태에서 설정의 Debug 차량 통신 항목을 기록해 주세요.

## Evidence and licenses

See VALIDATION.md for exact software results and hardware limits, SHA256SUMS.txt
for artifact hashes, and LANGUAGE-LICENSES.txt for the retained font notices.
The relink-objects ZIP contains matching compiled library/application objects
and replacement instructions, without private application source or keys.
