# NewDoe 0.11.7 beta — stock pairing handoff

## 0.11.7 Bootstrap pairing handoff

**Use the matching 0.11.7 APK and ZIP. From 0.11.4–0.11.6, normal CFW update is available.** Gate and language resources are unchanged. Older Gates still use the existing keep-data stock-return/reinstall route.

During stock installation, **keep the existing Android pairing** and choose **Install CFW** in Bootstrap. Bootstrap now loads the saved stock link key before starting Bluetooth and carries it into CFW through the verified settings journal. A normal key handoff skips the reset question. Do not delete the pairing as a routine installation step.

If the key is missing or connection still fails, manual recovery remains available. In the Bootstrap connection-wait screen, press **UP**, choose the phone or all pairings, then release O and hold O for two seconds to confirm. If **Reset Bluetooth?** appears directly, **Yes** clears saved pairings and **No** keeps them; neither is automatically selected. After clearing:

1. Wait for **PAIR YOUR PHONE**.
2. Forget this Noodoe in Android Bluetooth settings and pair it again.
3. Return to **Check current device → Continue from here**. Do not resend Bootstrap or clear app data.

Previously re-paired or deliberately deleted CFW keys are preserved when the stock key has not changed. The stock NOR record is read only. Before final installation, the current key must be durably saved and verified for Product to reuse it. On a first installation without CFW settings storage, a manual re-pair remains session-local until that final save; an earlier power restart may require re-pairing again.

The two synchronized progress bars, full-screen developer message and final **Back / Install CFW** choices with the existing 30-second confirmation remain unchanged. Bootstrap runs only during stock installation: a normal CFW update does not replace an already-running Bootstrap. To test this fix, the stock-to-Bootstrap step must use the new ZIP.

Software tests replayed the stock 5.16 bond routines, key authentication commands, journal interruptions and both handoffs. Physical radio testing and the unavailable stock 5.14 binary are not covered by that evidence. If a phone has already forgotten its key, firmware cannot recreate the phone-side copy: pair once using the fallback above.

## 한국어

## 0.11.7 Bootstrap 페어링 인계

**0.11.7 APK와 ZIP을 함께 사용하세요. 0.11.4~0.11.6에서는 일반 CFW 업데이트가 가능합니다.** Gate와 언어 자원은 같습니다. 더 오래된 Gate에서는 기존 데이터 유지 순정 복귀·재설치 경로를 사용합니다.

순정에서 설치할 때 **Android의 기존 페어링을 유지한 채** Bootstrap의 **Install CFW**를 선택하세요. Bluetooth를 켜기 전에 순정이 저장한 연결 키를 불러오고, 검증된 설정 저널을 거쳐 CFW에 전달합니다. 키를 이어받으면 초기화 질문 없이 연결을 진행합니다. 정상 설치 중에는 페어링을 미리 지우지 마세요.

키가 없거나 연결이 실패하면 수동 복구를 사용할 수 있습니다. Bootstrap의 연결 대기 화면에서 **UP**을 누르고 해당 휴대폰 또는 전체 페어링을 선택한 뒤, O를 놓았다가 2초간 눌러 확인하세요. **Reset Bluetooth?**가 바로 뜨면 **Yes**는 저장된 페어링 삭제, **No**는 유지이며 자동 선택하지 않습니다. 초기화했다면:

1. **PAIR YOUR PHONE**을 기다립니다.
2. Android Bluetooth 설정에서 이 Noodoe를 등록 해제하고 다시 페어링합니다.
3. 앱의 **현재 기기 확인 → 여기서부터 계속하기**로 돌아갑니다. Bootstrap을 다시 보내거나 앱 데이터를 지우지 마세요.

순정 키가 바뀌지 않았으면 이후 CFW에서 재페어링하거나 직접 삭제한 결과를 유지합니다. 순정 NOR 기록은 읽기만 합니다. 최종 설치 전에 현재 키의 저장과 재읽기 검증을 끝내야 Product로 넘어갑니다. 처음 설치하여 CFW 설정 저장소가 없을 때 수동 재페어링한 키는 최종 저장 전까지 이번 실행에만 남으므로, 그 전에 전원이 재시작하면 다시 페어링해야 할 수 있습니다.

함께 차오르는 진행 막대 두 개, 전체 화면 개발자 메시지, 최종 **Back / Install CFW**와 기존 30초 확인 동작은 유지합니다. Bootstrap은 순정 설치 과정에서 실행됩니다. 일반 CFW 업데이트는 이미 실행 중인 Bootstrap을 교체하지 않으므로, 이번 수정을 시험하려면 순정 → Bootstrap 단계에 새 ZIP을 사용해야 합니다.

순정 5.16의 실제 키 처리 코드, 인증 명령, 저장 중단과 양쪽 인계 경로를 소프트웨어로 검증했습니다. 실기 무선 시험 및 확보하지 못한 순정 5.14 바이너리는 이 검증에 포함되지 않습니다. 휴대폰에서 이미 키를 삭제했다면 기기가 그 사본을 복원할 수는 없으므로 위 수동 복구로 한 번 페어링하세요.

## Downloads and verification

- Install `NewDoe-0.11.7.apk` over the current app, preserving its data, and select `NewDoe-CFW-0.11.7.zip`.
- Existing verified `NewDoe-LANGUAGE-B-0.11.2.zip` resources remain valid. The Gate binary is unchanged from 0.11.4.
- Test stock → Bootstrap using the same phone, with its existing pairing left intact. Then confirm the CFW startup and reconnect without forgetting the device. If already running CFW, a normal update remains supported; returning to stock is only needed to exercise the changed first-install boundary or migrate an older Gate.
- The accepted older-firmware stock-return power-interruption limitation remains: maintain module power throughout that transition. This release does not replace the resident bootloader or claim to fix that old-code interval.
- No physical Bluetooth, LCD or installation qualification was performed. See `verification.json` for software evidence and `SHA256SUMS.txt` for artifact hashes.
- Font licenses are in `LANGUAGE-LICENSES.txt`; the replaceable-library objects and instructions are in the matching relink archive. Source is backed up privately; public branches contain documentation and release artifacts only.
