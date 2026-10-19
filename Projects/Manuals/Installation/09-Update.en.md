## 0.11.14: language startup and installation status

**Use NewDoe-0.11.14.apk with NewDoe-CFW-0.11.14.zip.** Versions 0.11.4 or newer with the same Gate use the existing CFW update route. Gate and B language resources 0.11.2 are unchanged. A healthy installed pack does not need retransmission. The live ODO/UART fixes and trip/maintenance record preservation remain.

After choosing a language and accepting the existing restart prompt, startup prepares the saved language's text and fonts before opening HOME. SYSTEM STARTUP appears during preparation. No phone button or long ignition cycle is required. Vehicle communication, trip accounting and boot confirmation continue during preparation. Missing/corrupt resources or failed RAM allocation fall back to English. English-only mode and language preferences remain saved.

If Bluetooth disconnects while checking language readiness after confirmed firmware installation, the app reconnects and verifies the same device, Product and confirmed boot before reading the saved pack and menu readiness again. It does not replay firmware or resource writes. Completed byte transfer can still be followed by index/font checks and menu preparation. Wait for the final readiness result. If the result remains unconfirmed, reconnect and select **Install / resume language resources**. Firmware reinstallation or stock recovery is not required.

The new Bootstrap keeps the installation screen while inspecting reused files. The overall bar accounts for each file's transfer, verification, NOR write and readback once; the lower bar shows the current operation. Repeated reads do not rewind overall progress. Manual Check current status after stock transfer, the default-No Bluetooth reset question, final approval and write-safety conditions remain unchanged. Do not return to stock just to update the Bootstrap display.

The phone now parses a valid reply already received before declaring a timeout, and riding response waits allow 10 seconds instead of 3. Actual socket closure still triggers the existing reconnect path immediately. Camera-triggered disconnection could not be reproduced with the vehicle because it is at a service centre; this release does not claim a verified hardware fix for that symptom.

Software validation only. Keep the module powered. The previously accepted old-code stock-return power-interruption limitation remains unchanged.

## 0.11.13: Bootstrap message layout and manual connection handoff

Use the matching **NewDoe-0.11.13.apk + NewDoe-CFW-0.11.13.zip**. Product must also be updated to **0.11.13** for the ODO/UART fixes. Devices with the same Gate (0.11.4 onward) use the existing CFW update route. Gate and B language resources are unchanged. The Bootstrap display fix applies to new stock installations with this ZIP; do not return to stock just to change that temporary screen.

From 0.11.13, ODO storage, correction and replacement with an older value are removed. ODO displays the current valid dashboard UART reading. Missing data is shown as unknown until reception resumes. The key-OFF display may retain this running session in RAM only. TRIP A/B, today/session trips, oil, belt, service records and settings remain preserved. The ODO check menu is removed; old correction records are neither read nor rewritten.

After the stock transfer has been verified, the app closes the connection and waits. Keep module power connected. Follow the stock screen: IGN OFF, wait for the NewDoe Bootstrap menu, then IGN ON. Tap **Check current status** in the app. On Noodoe choose **Install CFW** and answer **Reset Bluetooth?** (default **No**, preserves pairing). The app waits for this local choice on the same connection, then continues the existing preparation/verification/install flow without a third phone button. If already in the installation screen, it proceeds as soon as local permission and readiness are verified. The existing separate restore-data installation choice remains available; an existing selected restore policy is retained.

Transfer verified is not CFW installed. The final on-device confirmation/countdown and Product boot confirmation remain required. A connection failure or an uncertain commit does not authorize resending Bootstrap or silently starting a new installation. If pairing fails, use the existing optional Bluetooth recovery guide and check the device again.

The developer message keeps all original text and page controls. Its left/right placement now uses the correct EVE text alignment bits. Progress displays fresh acknowledged transfer bytes rather than an old device job; internal work is sampled every 250 ms and the screen refreshes every 200 ms. Repeated progress ticks no longer rebuild unchanged settings controls. These intervals are software scheduling targets, not a physical Bluetooth latency guarantee.

Software verification only; no physical LCD/RF/installation qualification this turn. Keep module power stable. The previously accepted old-code stock-return power-interruption limitation remains.

## 0.11.8 Optional Bluetooth reset — default No

After **Install CFW**, Bootstrap always asks **Reset Bluetooth?**, including when a saved bond is available. **No is selected by default.** The recommendation has been removed. Press O on No to keep the pairing and continue normally. Choose Yes deliberately only to clear the saved phone pairings, then re-pair in Android settings and return to the app. Waiting never chooses Yes or resets Bluetooth.

The stock-key handoff from 0.11.7, secure reconnection, file verification, key-save barrier, final installation confirmation and Bootstrap → CFW handoff are unchanged. The final **INSTALL CFW?** screen retains its existing 30-second confirmation; the Bluetooth reset question has no automatic confirmation.

Use the matching **0.11.8 APK and ZIP**. From 0.11.4–0.11.7, normal CFW update remains available. Gate and language resources are unchanged. The new Bootstrap prompt is used during stock installation with the new ZIP; a normal Product update does not replace an already-running Bootstrap. The existing older-Gate migration and accepted old-code stock-return power-interruption limitation remain.

## 0.11.7 Bootstrap pairing handoff

**Use the matching 0.11.7 APK and ZIP. From 0.11.4–0.11.6, normal CFW update is available.** Gate and language resources are unchanged. Older Gates still use the existing keep-data stock-return/reinstall route.

During stock installation, **keep the existing Android pairing** and choose **Install CFW** in Bootstrap. Bootstrap now loads the saved stock link key before starting Bluetooth and carries it into CFW through the verified settings journal. The optional reset question always appears with No selected by default. Do not delete the pairing as a routine installation step.

If the key is missing or connection still fails, manual recovery remains available. In the Bootstrap connection-wait screen, press **UP**, choose the phone or all pairings, then release O and hold O for two seconds to confirm. If **Reset Bluetooth?** appears directly, **Yes** clears saved pairings and **No** keeps them; neither is automatically selected. After clearing:

1. Wait for **PAIR YOUR PHONE**.
2. Forget this Noodoe in Android Bluetooth settings and pair it again.
3. Return to **Check current device → Continue from here**. Do not resend Bootstrap or clear app data.

Previously re-paired or deliberately deleted CFW keys are preserved when the stock key has not changed. The stock NOR record is read only. Before final installation, the current key must be durably saved and verified for Product to reuse it. On a first installation without CFW settings storage, a manual re-pair remains session-local until that final save; an earlier power restart may require re-pairing again.

The two synchronized progress bars, full-screen developer message and final **Back / Install CFW** choices with the existing 30-second confirmation remain unchanged. Bootstrap runs only during stock installation: a normal CFW update does not replace an already-running Bootstrap. To test this fix, the stock-to-Bootstrap step must use the new ZIP.

Software tests replayed the stock 5.16 bond routines, key authentication commands, journal interruptions and both handoffs. Physical radio testing and the unavailable stock 5.14 binary are not covered by that evidence. If a phone has already forgotten its key, firmware cannot recreate the phone-side copy: pair once using the fallback above.

## 0.11.4 Gate update

This release replaces Gate. **From every earlier release, including 0.11.3, use keep-data stock return and reinstall 0.11.4.** Install the 0.11.4 APK over the existing app, select the matching ZIP and follow its Gate migration guidance. A Product-only update cannot replace Gate.

1. Stop riding integration and choose **Keep CFW data → Return to stock → Check current device**.
2. Install **NewDoe-CFW-0.11.4.zip**, selecting **Restore saved data** in Bootstrap. If connection fails, follow the existing manual pairing help, then **Check current device → Continue from here**.
3. Finish the actual **CFW UPDATE** confirmation and the phone's firmware confirmation after successful startup.
4. Reuse **NewDoe-LANGUAGE-B-0.11.2.zip**. An identical verified pack does not need retransmission. The 0.11.3 language stability fixes remain included.

Small text and the version are now at least as large as the main menu; menus are centered. Dashboard values and status use separate rows. Once the runnable firmware passes verification, **Start NewDoe** works while optional backups are still being checked.

Automatic recovery follows **three consecutive unconfirmed boot failures**. Healthy boot confirmation clears the count. Power loss and BOR exclude the last interrupted handoff without discarding earlier real failures. Invalid firmware/storage can still enter recovery immediately to avoid running corrupted code. Only observed DOWN/O input is described as a button request; other entries show interrupted startup, saved recovery state or the actual error.

**Maintain module power throughout stock return and reinstallation; do not crank the engine.** The previously accepted old-code stock-return power-loss limitation remains. New Gate BOR handling cannot repair the older installer code already running on a device.

Hardware check: verify V0.11.4 at startup, readable centered menus, More/Dashboard, Start becoming available during backup checks, and normal reboot without an unexpected recovery request. Do not deliberately cut installation power. ARM execution and display commands were software-tested; this release has no physical power or LCD qualification.

---

## 0.11.3 language stability update

From 0.11.2, install **the 0.11.3 APK and NewDoe-CFW-0.11.3.zip** through CFW update. Gate is byte-for-byte identical to 0.11.2, so this path does not require stock return. Older Gates retain the existing keep-data stock-return migration requirement.

- Reuse **NewDoe-LANGUAGE-B-0.11.2.zip**. Its older filename is intentional: resource contents are unchanged. A matching verified pack is checked without retransmission.
- An older installed pack gets an explicit upgrade offer. Later preserves the existing pack and language preference.
- Transfer, verification/saving, automatic restart and actual menu activation are separate statuses. A lost final reply triggers fresh device readback, not a firmware reinstallation or blind transfer replay.
- Turn English only off, choose the device language, then accept the restart prompt on the stopped device. Follow app applies the app language at the next boot; a manual choice is not overwritten on reconnection.
- Transient read-only storage errors get two additional attempts. Published text and glyph caches remain usable during a recheck. Persistent errors fall back to English and retain preferences; fonts are not automatically repaired.

Hardware check: choose Korean, accept restart, and verify the Korean settings without tapping the app. Repeat with Japanese, then cycle power and verify the saved choice. With the current pack already installed, Install/resume resources should show the active language without retransmitting. If errors persist, retain diagnostics and use Install/resume resources to recheck. Firmware completion and language resource failure remain separate.

Keep the vehicle stopped and the module powered. Ignition ON/OFF does not cancel resource installation. Confirmed font damage or insufficient storage retains the existing explicit recovery/English-only choices.

---

**0.11.2 Gate migration / Gate 교체:** [Read first / 먼저 읽기](../User/36-NewDoe-Recovery.en.md).

# Update an existing CFW

**0.11.2 beta:** [current installation and language-pack sequence](../User/35-Languages.en.md). Confirm normal firmware startup before installing optional language resources.

**Keep permanent power connected:** a power interruption while older CFW records its keep-data stock-return request can stop boot before the new recovery code runs. Automatic recovery is not guaranteed in that legacy interval. Keep permanent power connected throughout stock return/reinstallation and do not operate the starter. IGN OFF is not removal of permanent power. If already stuck, do not assume another reset will recover it; retain diagnostics and ask for help.

**APK / Product 0.11.2 · Manual revision 5 · 2026-10-17**

[Installation index](README.en.md) · [한국어](09-Update.md)

## Ordinary update

1. **Install APK 0.11.2 over the existing app**, retaining app data, logs and backups.
2. Select the correct Noodoe and **stop riding integration**.
3. Select the matching **0.11.2 ZIP** and start an ordinary CFW/Product update. The stock 5.14/5.16 requirement does not apply to the older CFW currently running.
4. Follow transfer, verification, local approval and restart instructions. When Gate and resources already match, the ordinary payload is the 384KiB Product APP.
5. Confirm the actual new `CFW UPDATE` screen in the app. Reconnect to the same device/candidate and wait for at least 5 seconds of healthy execution and permanent confirmation. The current candidate's maximum window is 5 minutes; follow the device-reported deadline for older firmware.

| Connection | Transfer and verification | Approval |
|---|---|---|
| ![Connection](../images/update-1.png) | ![Transfer](../images/update-2.png) | ![Approval](../images/update-3.png) |
| Restart | New screen confirmation | Complete |
| ![Restart](../images/update-4.png) | ![Confirm](../images/update-5.png) | ![Complete](../images/update-6.png) |

Update APK and ZIP as a pair. This ZIP replaces Gate. 0.9.x, 0.10.x and 0.11.0/0.11.1 all need the one-time keep-data stock-return route. For an older Gate, use **keep-data stock return → Check current device → Bootstrap from the new ZIP → Restore stored data → Gate+CFW installation → confirmed normal startup**. Later updates with the matching Gate use ordinary Product transfer.

## Resume after interruption

Use the same device and ZIP and follow **Identify device → Inspect last operation → Continue**. A complete staged image may be reused after integrity checks. A lost COMMIT/RESET reply does not justify repeating the request or bypassing unresolved records by changing ZIPs or clearing app data.

If 0.11.2 displays the manual-reset instruction, follow [recovery](10-Recovery.en.md). **The previously installed firmware performs the first restart, so older wording such as `Paused. See phone help.` may still appear during that handoff.** The new wording is not a fix for every underlying installation failure.

## Rollback and retained data

Ordinary updates and failed-candidate rollback retain settings, rider name, photos, trips and maintenance baselines. Explicit fresh start, reset and full removal are separate actions. A normal update does not repeatedly back up the entire NOR or recreate every file.

A candidate that fails execution or confirmation attempts a known-good rollback. If none exists or rollback fails, use Gate recovery. **100% transfer is different from permanent candidate confirmation.** See [troubleshooting](11-Troubleshooting.en.md).

The app now saves and restores settings around CFW updates. See [settings retention](../User/30-Sharing-Settings.en.md). Do not clear app data before completion.

## 0.11.2: language installation with ignition ON or OFF

Update both the APK and firmware ZIP to 0.11.2. The new Gate requires the migration described above. The language resources themselves need no formatting or app-data deletion. Confirm CFW UPDATE and firmware completion first, then accept the resource offer or use **Install / resume language resources**.

Ignition ON/OFF is ignored throughout language inspection, optional FONT repair, transfer, verification, saving and automatic restart. Switching ignition or losing vehicle telemetry does not pause, cancel or restart the operation. No five-second ignition admission wait remains. A fresh speed of at least 3km/h still pauses uncommitted work; keep the vehicle parked. The active installation keeps the module awake even with ignition OFF. Keep its permanent power supply connected.

Transfer 100% is followed by real file/index verification, journaled saving, automatic restart and resource-ready confirmation. Wait for that final confirmation. If Bluetooth disconnects or progress times out, use the same resource menu to resume; recorded candidate pages are reused only after physical hash comparison. Committed metadata finishes or recovers through Gate before normal filesystem access resumes.

The new archive is **NewDoe-LANGUAGE-B-0.11.2.zip**, with separate TEXT.PAK and FONTS.PAK entries. The new text adds translated long-parking options. Older healthy packs still work, with English fallback for these new labels; update resources for the new translations. An explicitly enabled **English only** preference is preserved; turn it off, select Follow app or the desired language and accept the restart. Firmware completion and language-resource completion remain separate.


0.11.2 prepares the saved device language after configuration restore and checks actual menu readiness after restart. Resources are distributed as text/common-font parts; identical installed pack content is not resent.
