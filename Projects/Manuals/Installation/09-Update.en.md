# Update an existing CFW

**0.10.5 beta:** [current installation and language-pack sequence](../User/35-Languages.en.md). Confirm normal firmware startup before installing optional language resources.

**Keep permanent power connected:** a power interruption while older CFW records its keep-data stock-return request can stop boot before the new recovery code runs. Automatic recovery is not guaranteed in that legacy interval. Keep permanent power connected throughout stock return/reinstallation and do not operate the starter. IGN OFF is not removal of permanent power. If already stuck, do not assume another reset will recover it; retain diagnostics and ask for help.

**APK / Product 0.10.5 · Manual revision 3 · 2026-10-17**

[Installation index](README.en.md) · [한국어](09-Update.md)

## Ordinary update

1. **Install APK 0.10.5 over the existing app**, retaining app data, logs and backups.
2. Select the correct Noodoe and **stop riding integration**.
3. Select the matching **0.10.5 ZIP** and start an ordinary CFW/Product update. The stock 5.14/5.16 requirement does not apply to the older CFW currently running.
4. Follow transfer, verification, local approval and restart instructions. When Gate and resources already match, the ordinary payload is the 384KiB Product APP.
5. Confirm the actual new `CFW UPDATE` screen in the app. Reconnect to the same device/candidate and wait for at least 5 seconds of healthy execution and permanent confirmation. The current candidate's maximum window is 5 minutes; follow the device-reported deadline for older firmware.

| Connection | Transfer and verification | Approval |
|---|---|---|
| ![Connection](../images/update-1.png) | ![Transfer](../images/update-2.png) | ![Approval](../images/update-3.png) |
| Restart | New screen confirmation | Complete |
| ![Restart](../images/update-4.png) | ![Confirm](../images/update-5.png) | ![Complete](../images/update-6.png) |

Update APK and ZIP as a pair. Gate and Bootstrap are unchanged from 0.10.4 in this ZIP. For an older Gate, use **keep-data stock return → Check current device → Bootstrap from the new ZIP → Restore stored data → Gate+CFW installation → confirmed normal startup**. Later updates with the matching Gate use ordinary Product transfer.

## Resume after interruption

Use the same device and ZIP and follow **Identify device → Inspect last operation → Continue**. A complete staged image may be reused after integrity checks. A lost COMMIT/RESET reply does not justify repeating the request or bypassing unresolved records by changing ZIPs or clearing app data.

If 0.10.5 displays the manual-reset instruction, follow [recovery](10-Recovery.en.md). **The previously installed firmware performs the first restart, so older wording such as `Paused. See phone help.` may still appear during that handoff.** The new wording is not a fix for every underlying installation failure.

## Rollback and retained data

Ordinary updates and failed-candidate rollback retain settings, rider name, photos, trips and maintenance baselines. Explicit fresh start, reset and full removal are separate actions. A normal update does not repeatedly back up the entire NOR or recreate every file.

A candidate that fails execution or confirmation attempts a known-good rollback. If none exists or rollback fails, use Gate recovery. **100% transfer is different from permanent candidate confirmation.** See [troubleshooting](11-Troubleshooting.en.md).

The app now saves and restores settings around CFW updates. See [settings retention](../User/30-Sharing-Settings.en.md). Do not clear app data before completion.

## 0.10.5: language verification and saving

Install both the 0.10.5 APK and ZIP. Matching-Gate 0.10.x devices use normal CFW-to-CFW update; this resource issue does not require stock return, formatting or deleting app data. Confirm the new CFW UPDATE screen and firmware installation first, then accept the language-resource offer. Already confirmed firmware can use Install / resume language resources.

Remain parked with ignition OFF. Transfer 100% is followed by file hashes, translated strings, glyphs, emoji validation, journaled saving, automatic restart and readiness confirmation. The phone and dashboard show these stages separately. Glyph validation uses actual item counts instead of completed transfer bytes.

Turning ignition ON pauses uncommitted work. Turn it OFF again, or wait for five seconds of fresh valid stationary speed, to resume. Missing speed or movement cannot authorize writes. Five minutes without progress or a lost connection safely stops uncommitted work; use the same resource menu to resume. Once metadata commit begins, the bounded transaction finishes or recovers through Gate before releasing the filesystem. Keep the module powered.

Resume compares physical page hashes and reuses only matching candidate bytes. Firmware completion and resource completion remain separate. Wait for automatic restart and multilingual-ready confirmation. An explicitly enabled English only preference remains enabled; turn it off and select Follow app or the desired language, then confirm restart. Matching installed packs are reused.
