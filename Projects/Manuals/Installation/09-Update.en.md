## 0.11.6 Bootstrap display and optional re-pairing

**From0.11.4 or0.11.5, use normal CFW update.** Gate and language resources are unchanged. Older Gates still require keep-data stock return and reinstallation using the matching APK/ZIP.

The nine initial file checks use a horizontal loading bar without numbers. During installation, the pale mint upper bar is overall progress; the mint lower bar is the current device operation. The overall bar combines completed stages with the current operation’s fraction, instead of counting that stage as already complete. Percentages are hidden on the Noodoe. The phone uses the same snapshot, subject to polling delay.

Choosing **Install CFW** opens **Reset Bluetooth?**. **Yes is recommended** and clears all saved phone pairings; it is never automatically confirmed. **No** keeps the current pairing. After choosing Yes:

1. Wait for **PAIR YOUR PHONE** on the Noodoe.
2. In Android Bluetooth settings, forget this Noodoe.
3. Pair the same Noodoe again.
4. Return to the app, choose **Check current device → Continue from here**. Do not resend Bootstrap.

The existing connection closes during reset. If CFW configuration storage exists, cleared keys are saved before Bluetooth restarts. On a first installation without that storage, re-pairing is held in the running Bootstrap and the new keys are saved before final installation. Persistence across a power restart before installation is not claimed in that case. Reset/save failures are not reported as success.

The final **INSTALL CFW? / Everything is ready.** screen shows **Back / Install CFW** side by side. Its30-second countdown approves installation through the existing path. UP/DOWN stops the countdown and changes the selection; O executes it. Back cancels; Install CFW approves immediately. Connection loss stops automatic approval.

**Developer’s message** uses the entire circular screen for contiguous text. **DOWN advances one screen, UP goes back one screen**, either end wraps, and O returns to the menu. Original text and font size are preserved; the fixed heading/footer no longer consume reading space.

Bootstrap runs during installation from stock. Existing CFW users need not return to stock merely to see the changed menus. To run this new Bootstrap, use this ZIP during stock installation; normal Product update replaces Product only. Use the matching0.11.6 APK and ZIP.

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
