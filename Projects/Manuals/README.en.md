[0.11.10: phone dashboard and three-page app](User/37-Phone-Dashboard.en.md)

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

# FuckNudo manuals

[0.10.1 languages and installation transition —0.10.1 beta](User/35-Languages.en.md)

**APK / Product0.10.1 beta · 2026-10-16**

1. [Usage guide](User/README.en.md): buttons, pairing, media, calls, maps, settings and power
2. [Installation, updates and recovery](Installation/README.en.md): distinct stock-first-install and CFW-update procedures
3. [Troubleshooting and diagnostics](Installation/11-Troubleshooting.en.md): Paused, unknown results, pairing and recovery
4. [Menu, GPS, battery and fuel-warning indicators](User/23-Controls-Recovery.en.md)
5. [NOR backup and file browser](User/13-NOR-Backup.en.md)

[0.10.1 APK](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/latest/download/NoodoeCompanion-0.10.1.apk) · [matching ZIP](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/latest/download/NoodoeInstaller-CFW-0.10.1.zip) · [release notes](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/latest)

Use APK and ZIP from the same release. Ordinary updates retain settings, photos, trips and service baselines. First installation, an explicit fresh start and full removal are different operations. The app includes an offline quick guide; its Full manual button opens the appropriate Korean or English index.

Versioned historical notes and older screenshots describe their original versions. The current usage and installation guides take precedence; software-rendered pictures are labelled as simulations.

## Compatibility before installation

Read the [compatibility requirements and vehicle evidence](Installation/07-Compatibility.en.md) first. Stock 5.14 or 5.16 is admitted without HW, bootloader-version, model or PCBA whitelists. Wiring and vehicle data on every other model are not verified. The Korean and English installation/recovery guides now cover the same steps and conditions. This release includes a new Product and matching APK/ZIP; Bootstrap and Gate are unchanged.

[0.9.62 · Dashboard blink alerts](User/24-Dashboard-Alerts.en.md)

[0.9.63 · Indicators and reconnection](User/25-Connection-Indicators.en.md)

[0.9.64 · HOME AUTO · manual pairing · background return](User/26-Home-Auto.en.md)

[0.9.65 · Latest messages · location/music replies](User/27-Latest-Messages.en.md)

[0.9.66 · Balanced speed ring colours](User/28-Speed-Ring.en.md)

[0.9.67 · Actual stock mode](Installation/29-Stock-Mode.en.md)

[0.9.68 · Sharing, settings and smooth ring](User/30-Sharing-Settings.en.md)

[0.9.69 · Settings catalog update fix](User/31-Settings-Catalog.en.md)

[0.9.70 · Ignition-off photo without shading](User/32-Off-Photo.en.md)

[0.9.71 · Summary contrast, retained settings and oil arc](User/33-Ride-Preservation.en.md)
