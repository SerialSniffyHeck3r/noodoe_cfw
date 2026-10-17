0.11.1: the logo and skull are now 240px. Welcome! becomes Device Startup when checking begins; the skull screen has no title. Gate text is laid out inside the 480px round screen.

# NewDoe 0.11.1: startup, recovery and parking

NewDoe is the new name of this firmware and Android app. Install the matching APK and ZIP. Android package identity, signing key, saved settings and language content remain compatible. Use the new APK for riding; pre-0.10 riding message formats are no longer accepted. Installer/stock/update compatibility remains.

## First installation of this Gate

This release replaces Gate. **0.9.x and 0.10.x both require one keep-data stock return and reinstallation**, even though ordinary updates between matching 0.10.x Gates previously worked. The existing installer compares the actual Gate image; it never overwrites Gate through a Product-only update.

1. Install NewDoe APK 0.11.1 over the existing app without clearing its data. Select the same module and matching ZIP, and stop riding integration.
2. Follow the existing **return to stock, keep CFW data** route. Check the current device and confirm stock. Stock 5.14/5.16 remain the installation targets; no new model or bootloader whitelist is introduced.
3. Send Bootstrap from the 0.11.1 ZIP and choose **Restore stored data**. Keep permanent power connected throughout. If Bootstrap cannot connect, use the existing connection-help route: unpair in Android settings, reset pairing on Bootstrap as instructed, pair again, then **Check current device → Continue**. No mandatory re-pair completion flag was added.
4. Complete installation, normal startup and the actual **CFW UPDATE** confirmation. Transfer 100% alone is not completion.
5. Healthy existing B language resources are reused. If resources are missing, accept the separate language-resource offer after firmware confirmation. The optional offline archive is NewDoe-LANGUAGE-B-0.11.1.zip; its text/font bytes and hashes are unchanged from 0.10.7.

Future updates with this same Gate can use the normal CFW-to-CFW path. A different Gate still requires migration. The original resident bootloader is not replaced.

**Known legacy limitation:** if permanent power is lost while older CFW writes its stock-return request, boot may stop before the new Gate runs. Automatic recovery is not guaranteed in that old-code interval. Keep permanent power connected, avoid operating the starter during installation, and retain diagnostics if it stops. IGN OFF is not removal of permanent power.

## What Gate shows

An ordinary startup shows NewDoe and its logo for about two seconds, then checks storage, firmware and the boot record before entering Product. A post-update installation restart skips the logo delay. Errors detected during checks enter the error screen. Holding DOWN at cold boot, or for one second during the logo window, requests recovery; the existing O entry remains available.

An error first shows the skull, the existing numeric code formatted as **0x0000**, and **Press Any Key... 30** counting down. A fresh button press or thirty seconds opens the recovery menu. That press only opens the menu. The reason is shown as **[code] [short reason]**; manual entry leaves the code column blank. Details also retains the latest operation error.

Eight rows always stay in place: Start NewDoe, Check again, Reinstall firmware, Restore previous, Return to stock, Details, Help, Restart. Unavailable actions are grey and explain why when selected. Only verified sources enable recovery. Start cannot bypass a failed trial or boot-attempt limit. Reinstall, previous-image restoration, stock restoration and restart require releasing O and holding it again for two seconds; UP/DOWN cancels. Checking never erases files. A pending publication is resumed by Restart.

Gate has **no Bluetooth, LVGL, external-font or SDRAM dependency**. Its artwork and help QR are in internal flash. If local recovery sources cannot be verified after bounded retries, the menu says no on-device recovery is available. Help links to this project's GitHub and explains programmer recovery after opening the module, or replacement if repair is not possible. If internal Gate itself cannot execute or the display hardware has failed, it cannot show these instructions.

## Parking and battery use

Existing power-sequence settings and timings are retained. Connection retention keeps Bluetooth available; the deepest parking stage disables Bluetooth, so proximity reconnection and welcome light cannot work there. Set these stages in **Settings → System → Power sequence**. Existing defaults are sixty minutes of screen standby followed by sixty minutes of Bluetooth retention, then deep sleep; saved user values are not reset.

The idle input supervisor now allows 100ms waits while parked, instead of waking every 10ms and defeating the 16ms minimum for STOP. Button interrupts wake it immediately; debounce/held-button processing stays at 10ms. Incomplete RAM-only resource reception expires after sixty seconds without progress. NOR publication is allowed to finish and never cancelled just because IGN changes.

In Gate with IGN OFF, an untouched error screen lasts thirty seconds, then the menu remains for the rest of the sixty-second idle period. Gate then shuts down the panel and uses short RTC STOP intervals to service the watchdog. RTC-only wakes do not turn the display back on or restart Bluetooth. The first button press wakes the screen without selecting anything. A missing/unusable RTC uses bounded timer-driven ordinary sleep instead of a busy loop; this fallback consumes more power than STOP.

Software checks verify state transitions and simulated wake sequences, not battery current. There is **no measured standby-current or one-week battery-life guarantee** for this release. Validate parked current after the configured deep-sleep delay, and check IGN/button wake and retained settings on hardware. A week-later battery voltage alone does not identify which circuit drew the current.
