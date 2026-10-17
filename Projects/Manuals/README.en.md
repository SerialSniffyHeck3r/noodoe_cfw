## 0.11.5 Bootstrap display update

**From 0.11.4, use the normal CFW update.** Gate and language resources are unchanged. Older Gates still use keep-data stock return and reinstallation with the matching latest APK and ZIP. The older 0.11.4 Gate migration instructions below apply only when coming from an older Gate.

The initial file checks use a horizontal loading bar without numbers. Menus are centred; mint marks the selected item. Two fixed bars show the overall device stage and current device operation throughout transfer and verification. The phone uses the same device snapshot, with a small polling delay.

On **INSTALL CFW?**, a visible 30-second countdown automatically approves the verified installation. UP/DOWN stops the countdown. **Back → O** cancels; **Install CFW → O** approves immediately. Losing the connection stops automatic approval; a verified reconnect starts a fresh 30 seconds. The existing safe cancellation of an already started write is unchanged.

The Ambient sensor menu and Bootstrap sensor sampling are removed. **Developer’s message** displays the complete original text. DOWN advances and wraps from the last page to the first; O returns to the menu. Riding automatic brightness is unchanged.

Bootstrap runs during installation from stock. Existing 0.11.4 CFW users do not need to return to stock just to see these menus. Use the matching 0.11.5 APK and ZIP.

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
