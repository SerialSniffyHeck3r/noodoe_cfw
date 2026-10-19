# NewDoe user and installation guide

**Current baseline: app and device firmware 0.11.15 / B language resources 0.11.2 · Revised 19 October 2026**
[한국어](Guide.md) · [Downloads](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/latest)

This guide assumes no electronics knowledge. Normal installation uses an Android phone and the vehicle buttons; it does not require dismantling, soldering or an ST-Link. **[Phone]** means the Android app or phone settings. **[Noodoe]** means the round vehicle display and its buttons. Stock, installation and recovery screens remain in English.

## Contents

1. [Compatibility](#compatibility)
2. [Buttons and terminology](#basics)
3. [Files and preparation](#prepare)
4. [First installation from stock, step by step](#first-install)
5. [Updating an existing CFW](#update)
6. [Language installation and selection](#language)
7. [Connection problems](#connection)
8. [What to do at each error or waiting screen](#troubleshooting)
9. [Recovery without a phone / returning to stock](#recovery)
10. [First use and permissions](#phone)
11. [Device controls and settings](#controls)
12. [ODO, trips and maintenance](#vehicle)
13. [Music, calls, notifications and replies](#media)
14. [Maps and automatic scale](#map)
15. [Phone dashboard, customization and preview](#dashboard)
16. [Ignition off, parking and battery](#parking)
17. [Reporting a problem and keeping backups](#report)

<a id="compatibility"></a>
## 1. Compatibility

| Check | Supported scope |
|---|---|
| Phone | Android 6.0 or later. There is no iPhone app. |
| Device | STM32F4-family KYMCO Noodoe. Start with the app's device check; you do not need to open the module to read a chip label. |
| First installation from stock | Stock firmware **5.14 or 5.16**. Do not bypass rejection of another version. |
| Vehicle model | Installation is not restricted to AK550. There is no model-name, PCBA, HW-number or stock-bootloader-version whitelist. |
| Existing NewDoe or older CFW | Use [CFW update](#update). The stock-version requirement is not a requirement for the CFW version number. |
| Multilingual device menus | Stock language pack **B** only. Pack A keeps English menus in this implementation. The app checks actual resources; do not infer the pack from the country or model. |

**Being admitted by the installer is different from being tested on every vehicle.** Wiring and cluster-data differences can affect speed, ODO and button switching. Experience with an AK550 does not qualify every F4 revision. Record a failed check and ask for help instead of substituting another device's identity.

If you start from stock 5.14, **returning to stock uses the bundled 5.16 application**. The installer does not capture and later restore your entire original 5.14 application.

<a id="basics"></a>
## 2. Buttons and terminology

| Label | Meaning |
|---|---|
| UP / DOWN | The vehicle's upper/lower button |
| O / OK | The centre confirmation button, not the number zero |
| Short press | Press and release |
| Long press | About 0.8 seconds in ordinary screens. Installation/recovery instructions specifying 2 or 3 seconds take precedence. |
| IGN ON | Turn the vehicle key/power switch ON. **This does not mean start the engine.** |
| IGN OFF | Turn that switch OFF. Noodoe may still have its separate permanent supply. |
| Stock | KYMCO's original firmware |
| Bootstrap | The temporary installation tool used when installing from stock |
| CFW / Product | The NewDoe device firmware used during normal operation |
| Gate / Recovery | Startup checks and recovery menus. **Gate has no Bluetooth.** |
| APK | The file installed on the Android phone |
| CFW ZIP | The firmware installation package sent by the app to Noodoe |
| Language ZIP | Translations, fonts and monochrome emoji; a different file from the CFW ZIP |

If your vehicle has a cluster/Noodoe selector, select **Noodoe**. Release held buttons when a new screen appears, then press again. No procedure requires holding UP and DOWN together.

<a id="prepare"></a>
## 3. Files and preparation

### Download the right files

Open the [official Latest release](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/latest) on your phone. The baseline files for this guide are:

| File | Purpose |
|---|---|
| `NewDoe-0.11.15-manual-r2.apk` | Phone app. The manual-revision APK also works with device firmware 0.11.15. |
| `NewDoe-CFW-0.11.15.zip` | First installation or updating the device to 0.11.15 |
| `NewDoe-LANGUAGE-B-0.11.2.zip` | Optional B language resources, when absent or outdated. **The older 0.11.2 filename is correct.** |

The app also offers a language-resource download. Keep the language ZIP if you prefer manual import. License, verification and relink files are not installation packages to select in the wizard. **Do not extract either ZIP or select a BIN inside it.** Installing an APK alone does not update the vehicle firmware.

### Before starting

1. Finish riding and park safely. Do not move the vehicle during installation.
2. Make sure the vehicle battery and phone battery are adequately charged and the supply is stable. If the battery is deeply discharged or unreliable, have it charged and checked first. Do not operate the starter, disconnect the battery or unplug Noodoe during installation.
3. Enable phone Bluetooth and keep the phone near the vehicle. Keep NewDoe in the foreground during installation. Avoid switching to the camera or another Bluetooth app. Camera-triggered disconnection has not yet been confirmed fixed on the vehicle.
4. If the app is already installed, install the new APK **over the existing app**. Do not uninstall it or clear its data. Its installation records and backups are needed for continuation.
5. If Android asks whether this source may install apps, verify that you downloaded the official APK and allow that installation. Phone manufacturers use different menu names. Open NewDoe afterward.
6. In **[Phone] Permissions / integration setup**, grant the Bluetooth/Nearby devices permissions requested for connection. Music, calls and location permissions are explained [below](#phone).
7. Disconnect another app that is using Noodoe. You do not need to delete a working Bluetooth pairing in advance.

For a first pairing, **[Phone] Pair new device / Pair Noodoe** opens Android Bluetooth settings. Put the stock device into pairing mode using its original vehicle instructions, then select your Noodoe. Check the address suffix rather than selecting a nearby vehicle with a similar name. An already registered device does not need to be paired again.

**There is no guaranteed fixed installation time.** Bluetooth and storage performance vary. Slow progress is different from an explicit error.

<a id="first-install"></a>
## 4. First installation from stock

The sequence is **prepare the phone → identify stock → transfer Bootstrap → check its device menu → prepare/install CFW → confirm the new CFW screen → optionally install language resources**.

### 4-1. Select your device and package

1. **[Noodoe]** Turn IGN ON to wake the display. Do not start the engine.
2. **[Phone]** Select your Noodoe in the device list. If it is missing, pair through Android, then use **Refresh device list**.
3. Press **Check current status / Check current device**. Confirm that the app identifies stock firmware. Checking the device is not installing firmware.
4. When asked to choose an installation ZIP, select **`NewDoe-CFW-0.11.15.zip`**, not the language ZIP. In the file picker, open your download location and tap the ZIP itself.
5. Start the stock installation step. The app checks the device and package, then sends Bootstrap. If compatibility checks fail, stop here and use [troubleshooting](#troubleshooting).

### 4-2. Wait for Bootstrap to actually start

1. **[Phone]** Wait until Bootstrap transfer and verification have finished. Do not turn the key off early while sending.
2. **[Noodoe]** When instructed by the stock display and app, turn IGN OFF. **Do not disconnect the battery or module wiring.**
3. A stock **NOODOE INSTALLER** screen can mean that writing is still in progress. Wait; do not repeatedly press buttons or call installation complete.
4. After the **NewDoe Bootstrap menu actually appears**, turn IGN ON.
5. **[Phone] Press Check current status.** Do not just wait for an automatic reconnect at this handoff. Do not send Bootstrap again.

### 4-3. Start installation on the device

1. **[Noodoe]** Use UP/DOWN to select **Install CFW**, then press O. If the app is waiting for this choice, the device button action is required.
2. At **Reset Bluetooth?**, keep the existing pairing by choosing the default **No → O**. This question does not have an automatic-choice countdown.
3. **Yes really deletes stored pairing.** Choose it only when you intend to reset pairing, then follow [both-side re-pairing](#connection). Do not routinely select Yes during a healthy installation.
4. **[Phone]** Once readiness is confirmed, the app continues preparation and installation on the same connection. On the normal path, there is no extra third app button to find. If a troubleshooting wizard is displayed, follow its **Check last operation → Check and continue installation** sequence.
5. If existing CFW data is found, select **Restore saved data / Restore preserved data** to retain settings, trips and photos. **Start fresh** is for intentionally replacing the previous CFW environment. A first installation with no preserved data may not show this question.

### 4-4. While the two bars progress

The app and Bootstrap inspect storage, preserve the original information needed by installation, prepare a stock recovery image, write CFW/resources and verify them. The device shows **an overall bar and a current-operation bar**, without numeric percentages. The current-operation bar can restart for a new operation; the overall bar represents the full job.

This is not a full copy of every byte in your module to the phone. Installation preserves its required identity and storage records and prepares stock recovery firmware on the module. A separate full NOR backup is different.

- Follow the screen's ignition instructions instead of repeatedly toggling the key. IGN OFF alone does not cancel Bootstrap installation.
- Do not force a reset simply because a stage is slow and no error is shown.
- To request cancellation where offered, release O and hold it freshly for 3 seconds. Wait for a safe boundary. Cancellation cannot forcibly undo an installation that has already been committed.

### 4-5. The final INSTALL CFW? question

When preparation is complete, **Everything’s ready / INSTALL CFW?** appears with **Back / Install CFW** side by side. This is a separate final confirmation from choosing Install CFW in the initial menu.

- Install now: select **Install CFW**, then O.
- Automatic installation: with no input, installation is selected after **30 seconds**.
- Go back: UP/DOWN stops the countdown; select **Back**, then O.

The 30-second behaviour **does not apply to Reset Bluetooth?** It does not automatically approve failed connection or verification checks.

### 4-6. Restart and confirm the new screen — required

1. **[Noodoe]** Wait through installation and restart. Bluetooth may disconnect temporarily. Gate may show **UPDATING SYSTEM...** or **SYSTEM STARTUP...**. These are not completion screens.
2. **[Phone]** Keep the installer open. It reconnects to the new CFW and requests a screen confirmation.
3. **[Noodoe] Check that CFW UPDATE is actually visible.** Do not confirm while looking at Bootstrap, NOODOE INSTALLER or a blank screen.
4. **[Phone] Press the button confirming the CFW UPDATE screen.** Dismissing the dialog is not confirmation. If you dismissed it, use the confirmation button on the progress screen.
5. Wait until the app confirms **normal operation and completed installation**. The new firmware has a trial-confirmation deadline of up to **5 minutes**. An older firmware still running has its own deadline. The screen confirmation is followed by device health and connection checks.

If confirmation is not completed, the device can return to its previous CFW or enter recovery when no previous confirmed image exists. This trial period is not for waiting through a large language transfer. **Finish firmware confirmation first.**

### 4-7. Optional language installation and final checks

After CFW is confirmed, the app offers language-resource installation. To use translated menus on a B device, accept and follow [section 6](#language). Choosing **Later** still leaves the firmware installation complete; without the pack, use English.

Check the reported device version and that HOME opens. Vehicle speed zero while stationary can be normal; dashes or “unavailable” are different from a valid zero. If ODO, buttons or connection behave incorrectly, record the issue rather than immediately starting a road test.


### Other Bootstrap screens

On first entry, **Checking install files** uses one horizontal loading bar. The menu then offers **Bluetooth test / Install CFW / Back to stock / Help / Developer's message**. Ambient Sensor has been removed. UP/DOWN selects and O opens an item. The developer message scrolls with UP/DOWN; O returns. It is optional and does not start installation. Once installation starts, the two-bar installation view owns progress; it should not alternate with the initial check screen.

<a id="update"></a>
## 5. Updating an existing CFW

### Choose the route

| Current device | Route to 0.11.15 |
|---|---|
| Normally 0.11.4–0.11.15 with the same Gate as the package | **CFW update / resume**. No stock return required. |
| Older Gate, including 0.9.x, 0.10.x and 0.11.0–0.11.3 | **Return to stock keeping CFW data → verify stock → reinstall from the new ZIP**. |
| Version/Gate uncertain | Let the app inspect the device and compare the ZIP. A version number alone does not override the check. |

Gate is the startup/recovery code. A normal Product update does not replace it. Bootstrap is also not an app permanently updated by ordinary CFW updates: the **ZIP selected for installation from stock** supplies that temporary installation tool.

### Normal update with the same Gate

1. Install the new APK over the old app, keeping app data.
2. Park, then use the top **pause** control or **Stop riding integration / prepare update**.
3. On the **installation/current-status page**, select your device and `NewDoe-CFW-0.11.15.zip`.
4. Choose **CFW update / resume**. Wait for settings preservation, transfer, verification, installation and restart. Do not alternate different ZIPs to try to clear an error.
5. Complete the **actual CFW UPDATE screen confirmation and normal-operation confirmation**, as described in [4-6](#first-install).
6. Respond to the language offer according to the installed pack. **A healthy B 0.11.2 pack does not need retransmission just because the firmware version changed.**

Ordinary updates preserve settings, brightness, photos, TRIP A/B, today/session trips and oil/belt/service records. The removed ODO correction is not reapplied. After a disconnect, keep the same device, ZIP and app data; reconnect and complete result checking and any saved-settings restoration.

### Moving from an older Gate

1. Prepare the new APK/ZIP and select **Keep CFW data** when returning to stock. Do not choose complete deletion.
2. After stock actually appears, use **I returned to stock on the device / Check** or **Check current device** to verify the role.
3. Follow [first installation](#first-install) using that new ZIP.
4. When preserved data is found, choose **Restore preserved data**.
5. Confirm the new CFW before installing any required language resources.

**Old Gate code has an automatic-recovery limitation if permanent power is lost while writing its stock-return request.** That runs before the new Gate exists and cannot be retroactively fixed by an APK. New Gates already have an incomplete-record repair, but this is not a physical guarantee covering every possible interruption. Keep permanent power connected and do not operate the starter throughout either route.

<a id="language"></a>
## 6. Language installation and selection

### Supported languages

B device menus support **English, Deutsch, Español, Français, Italiano, Ελληνικά, 한국어, Tiếng Việt and 日本語**. The Android app additionally supports Simplified and Traditional Chinese, making 11 app languages. With Follow app language and a Chinese app language, the device menu falls back to English. Chinese characters can still be used in external music/notification text.

English retains its original Lato appearance. Other B menus reuse stock fonts; Japanese uses the additional font. All supported menu translations and shared fonts are installed together. Changing language does not require another transfer. Pack A devices retain English menus.

### Install resources or update an old pack

1. **Finish firmware confirmation first.** Accept the post-install language offer, or later open **Device language / language resources → Install / resume language resources**.
2. Follow the app's download option. For manual import, choose `NewDoe-LANGUAGE-B-0.11.2.zip` under **Import language resource ZIP**, not the CFW ZIP.
3. The app checks the actual device and resources. An identical healthy pack can be checked without retransmission. An older pack triggers an update choice; **Later** keeps the current pack and language preference.
4. Only if required stock FONT files have definite usable-structure failures does the app offer **Restore fonts and continue / Continue in English**. Not seeing this question on a healthy device is normal. Restoration replaces the affected files from a verified B backup; English mode does not repair stock FONT files.
5. Wait through **transfer → verification/saving → automatic restart → reconnection → menu language ready**. **The transfer bar reaching 100% is not the final step.**
6. If space or storage-structure checks fail, do not delete arbitrary files. Use the offered English path where available or report the error.

The current archive contains separate text and font parts. The app manages them; you do not send two files in a special manual order. Matching parts of a previous pack can be reused, but final saving and activation must still be confirmed.

**In 0.11.15, IGN ON/OFF does not cancel language transfer, verification or saving.** Keep the vehicle parked, permanent module power connected and the phone link available. Restrictions on actual detected movement remain.

### Example: change the device to Japanese

1. Connect to normal CFW and open **[Phone] View/change current device settings**, or the language setting on **[Noodoe]**.
2. Turn **English only** off if it is enabled.
3. Select **Device language → 日本語** and apply it. This is different from changing only **App language**.
4. **[Noodoe]** Accept the restart prompt while parked. **Later** saves the choice but keeps the current screen language until the next restart.
5. After restarting, wait for the blue **Loading menu text / Preparing menu fonts** screen with stage/item progress. HOME opens when the required text and glyphs are prepared. Pressing an unrelated phone button or leaving IGN off for a long time is not an activation step.
6. Check the device settings menu. Other supported languages use the same procedure. **A manually selected device language is not overwritten on reconnection.**

**Follow app language** applies a supported app language on the next restart. Motion, a warning or a power transition can cancel an accepted restart; park again and follow the screen. Settings and ride data must be saved before reset.

### Does every boot scan the whole pack?

No. **Full integrity verification happens only during resource installation/update.** Later boots read a small header and saved identity, then load **only the selected menu text and required glyphs**. Basic bounds, encoding and read-error checks still protect the data being used.

Language changes and pack updates show the preparation screen. An unchanged next boot uses ordinary startup. A full restart still needs to reload RAM, but does not rehash the entire pack. Waking from STOP reuses retained RAM. The whole pack is not copied into RAM or the display chip.

### Music and notification text modes

| Setting | Behaviour |
|---|---|
| **Render on phone** — default | The phone sends rendered text images. It can use characters and colour emoji supported by the phone, with preparation/transfer latency. |
| **Device fonts** | Small text data is sent and drawn using device glyphs. Less data is transferred; unsupported characters/combinations can appear as □. Includes the additional monochrome emoji. |

These modes apply to music, HOME, notification details and notification popups. Calls and quick replies use their existing paths. **English only** explicitly selects English UI and phone rendering. Repairing resources does not turn off an English-only choice you made yourself. Resource-error fallback also retains the saved language preference.

<a id="connection"></a>
## 7. Connection problems

### Identify the screen first

- **Gate recovery:** no Bluetooth. Use the device buttons.
- **Stock installer actively writing:** this may not be a connection or pairing stage. Check the device display first.
- **Bootstrap menu or connection waiting screen:** the procedure below can apply.
- **Normal CFW HOME:** check permissions, selected device and riding-connection state first.

### Couldn’t pair / cannot connect in Bootstrap

The normal choice is **Reset Bluetooth? → No**. Use the following only when the connection actually fails:

1. **[Phone]** Open **Can’t connect? / Bootstrap connection help**. Do not reset pairing during file writing.
2. Use its **Open Bluetooth settings** action. Selecting that recovery action stops the app's current connection attempt.
3. In Android, forget/unpair **your Noodoe only**. Do not clear NewDoe app data or delete its installation ZIP.
4. **[Noodoe]** From Bootstrap's connection waiting screen, press UP to open pairing management and select that phone. Release O, then **hold O freshly for 2 seconds** to delete the bond. **ALL** deletes every phone; use it only intentionally. If **Reset Bluetooth? → Yes** already cleared the device side, follow that screen instead.
5. At **PAIR YOUR PHONE**, find Noodoe in Android Bluetooth settings, pair again and approve the request.
6. Return to NewDoe, select the same device and press **Check current status**. If Bootstrap is at its main menu, choose **Install CFW** on the device. In the troubleshooting wizard, use **Check last operation → Check and continue installation**.

During first installation, a newly paired key may not survive a reset before final storage has been prepared. You may have to pair again in that case. Do not try to solve this by inventing a pairing-complete state, clearing app data or resending Bootstrap.

### Disconnects in ordinary CFW

Check whether riding integration was manually paused, the selected address is correct and Bluetooth/Nearby devices permission is still allowed. Temporary communication failures use automatic reconnection unless manually stopped. After an Android **Force stop** or permission removal, reopen the app and resume. Camera-triggered disconnects are not confirmed fixed on the vehicle; avoid app switching during installation and preserve diagnostics if reproduced.

<a id="troubleshooting"></a>
## 8. Waiting screens and errors

| What you see | Meaning and next step |
|---|---|
| Transfer 100%, Verifying and saving | Verification/saving is still running. Keep power connected and wait. Do not equate transfer completion with installation completion. |
| NOODOE INSTALLER | A device installation stage, not the CFW UPDATE screen to confirm. |
| INSTALL CFW? | Final local approval: Install CFW/O or the 30-second automatic choice. Back cancels this confirmation. |
| CFW UPDATE | The new CFW screen. Confirm it on the phone, then wait for normal-operation confirmation. |
| Firmware installed / Language resources are incomplete | Firmware completion and language readiness are separate. Reconnect to the same device and use **Install / resume language resources** to inspect actual state. Do not immediately reinstall firmware or return to stock. |
| NDCP Response failed, opcode 0xAA, socket closed / read -1 | The final status reply lost its connection. This alone does not prove resource corruption or failed saving. Check the device and the status after reconnecting. Preserve diagnostics if repeated. |
| Installation success or cancellation not confirmed | The phone does not yet know the result. Start with **Check current device → Check last operation**. Keep the same device, ZIP and app data. |
| Stage 6/8, Code 6 / Phase 4, Paused | The numbers alone prove neither success nor a bricked device. Use the reset procedure below only when the device explicitly requests it. |
| English after selecting another language | Check English only, device versus app language, the restart confirmation and B-resource readiness. Record repeated failures. |
| ODO dashes / no speed | Valid cluster data is unavailable. There is no ODO correction menu. Working GPS does not prove that vehicle UART data is arriving. |

### Only when manual reset is explicitly requested

```text
Please manually reset.
Hold UP + O together
for 3 seconds.
```

1. Photograph or write down the screen, Stage, Code and Phase.
2. Release all buttons, then hold **UP + O together for 3 seconds** and release.
3. With IGN OFF, the display may immediately appear off. Release the buttons and turn IGN ON to start from Gate. That off-looking state alone does not mean firmware installation failed.
4. Check the same device and ZIP again. If the actual CFW UPDATE screen appears, complete confirmation. If recovery appears, use [section 9](#recovery).

**Do not interrupt active writing or verification just because it seems slow.** Clearing app data, formatting NOR or importing another unit's dump are not general troubleshooting steps.

<a id="recovery"></a>
## 9. Recovery without a phone / returning to stock

### Enter the new Gate

Park and use the method appropriate to the current state:

- When a fresh boot shows the NewDoe logo, **hold DOWN for about 1 second**. Holding DOWN as module power starts is also supported.
- From running CFW, the existing entry remains: **IGN OFF → hold O only → keep holding while turning IGN ON → hold for at least 2 seconds after ON → release when recovery appears**.
- If UP+O with IGN OFF left the display off, release the buttons and turn IGN ON to start Gate again.

A simple key cycle is not always a full reboot: the device may wake directly from STOP. If the menu does not appear, release the buttons and inspect the screen. Disconnecting battery wiring is not the normal entry procedure.

Errors first show a **skull, a 0x-format error code and PRESS ANY KEY... 30**. Press a button or wait 30 seconds to reach recovery. Record the **code and short entry reason**. A manual entry can have no error code.

### Use the recovery menu

UP/DOWN moves the selection; O selects. **Grey items are unavailable in the current state.** Operations that change stored data require releasing O and then holding it freshly for 2 seconds at the confirmation screen. Follow the on-screen confirmation instructions.

| Item | Purpose |
|---|---|
| Start NewDoe | Run the verified current CFW. Once enabled, it can be used while backup checking continues. |
| Check again | Recheck storage and available recovery actions. |
| Reinstall firmware | Attempt recovery using a stored installation image. |
| Restore previous | Return to an available previous confirmed image. |
| Return to stock | Restore from a verified stock recovery image. |
| Try recovery anyway | Search additional locations when normal stock recovery is unavailable. It does not ignore failed integrity checks. |
| Details | Inspect errors and state. |
| More | Help / Dashboard / Play game / Back. |
| Restart | Restart the module. |

To return to stock, select **Return to stock**, then follow the **fresh O 2-second** confirmation. No phone connection is required. Keep permanent power connected until stock actually appears. Gate has no Bluetooth; a phone connection failure while using Gate is expected.

If forced recovery also fails, press O on the explanation screen to return to the menu. Restart/Check again reevaluates this attempt's failures but does not magically repair corrupted files. If no valid recovery image can be used, dismantling and programmer repair, professional repair or replacement may be necessary. If you cannot do that work, [ask the project](https://github.com/SerialSniffyHeck3r/noodoe_cfw/issues) with the screen and diagnostics.

### Return to stock through the app

Choose **Stop riding integration → Return to stock → Keep CFW data** when planning to reinstall and keep your environment. **Erase CFW data completely** is a separate choice requiring local approval in the deletion tool.

Once the device actually shows stock, use **I returned to stock on the device / Check** in the app. Select **Restore preserved data** when reinstalling later. Keep-data stock return does not restore every external-storage byte to its factory state.


If **Use recovery scratch** appears, read the extra confirmation: it replaces pending font/language repair data to make recovery space available. It is not a routine installation step. Keep a photo of the reason and approve only if you intend to discard that pending repair; other files are not the target.

<a id="phone"></a>
## 10. First use and permissions

Select your device and use the top **play/connect** button to start riding integration. Its **pause** control stops that connection; it is not the music player's pause control. Saving a ride file on the phone is another separate option.

| Permission/setup | Used for |
|---|---|
| Bluetooth / Nearby devices | Connection, installation and updates |
| Notification access | Selected app notifications and active media information |
| Precise location and requested background location | Maps/GPS and location while the phone screen is off |
| Contacts, recent calls and phone permissions | Names, call lists and supported call controls |
| App notifications | Connection/service notifications |
| Companion-device association and background settings | Background integration and reconnection support |

Denied permissions can limit their related functions. Review **Permissions / integration setup** one item at a time. Call-control support varies with Android/manufacturer. Even with permission, a missing caller number from the phone can produce Unknown caller.

Swiping the app from recents is different from Android **Force stop**. Force-stopped apps may need to be opened manually. Check manual pause and phone battery restrictions before assuming automatic reconnection is broken.

<a id="controls"></a>
## 11. Device controls and settings

In ordinary screens, **short O** cycles HOME → trips → music → phone → map → HOME. Notification, call, warning and installation overlays have their own button hints and take priority.

- **Short O + DOWN:** quick brightness settings.
- In quick settings, **short UP/DOWN:** adjust brightness; **short O:** close; **long O:** MAIN MENU.
- In quick settings, **UP 0.8 seconds:** reinitialize only the display, retaining the running session. This is not a firmware reinstall.
- In quick settings, **DOWN 0.8 seconds:** Super Nite low brightness.
- In full settings, UP/DOWN navigates or adjusts; O selects/applies; Back returns.

With valid vehicle-speed data, full settings open after the stationary requirement is met. If IGN ON is known but speed is missing, full settings are accessible. **Missing speed is not proof that the vehicle is stopped: park before operating it.** Installation and storage operations retain their separate checks.

The five menu icons hide after inactivity only on HOME; other ordinary pages retain them. GPS is grey without a fresh valid fix and gives a **120 ms grey pulse** on reception. The Bluetooth icon reports phone battery: below 20% yellow, below 15% red, below 5% alternating red/grey. Charging overrides low-battery colours: below 75% alternating green/grey, at least 75% steady green.

<a id="vehicle"></a>
## 12. ODO, trips and maintenance

**ODO is the current valid cumulative distance received from the cluster.** Local ODO correction, permanent ODO storage, previous-value substitution and ODO check are removed. Missing data is shown as unavailable. The last ODO shown after key-off exists only in the current run's RAM.

TRIP A/B, TODAY, this ignition session and oil/belt/service counters remain. Review or adjust maintenance distance-since-change and intervals in settings. Match edits/resets to actual maintenance history. These edits do not change the cluster's original ODO.

HOME AUTO prioritizes a new notification, then playing music, then other information. A new notification normally gets 20 seconds; a manual long-press information override also returns after 20 seconds. With no music, other information cycles. Dismissing a popup is separate from AUTO information priority.

The speed ring uses vehicle data, not GPS speed. Session average/peak markers appear above 5 km/h; IGN OFF makes the ring grey. The oil remaining display uses yellow at 30% or below, red at 15% or below and blinking red at 5% or below.

<a id="media"></a>
## 13. Music, calls, notifications and quick replies

Start playback in your music app, then check notification access and the riding connection. On the music page: short UP plays/pauses, short DOWN selects the next track and long UP selects the previous track.

The phone page starts with its summary. UP opens recent calls; DOWN opens the latest notification. Detail screens and call states change button meanings, so follow the right-side hints. Dismissing a call popup is different from actually ending a call. Learn these controls while parked.

Rapid notifications retain the newest content. The new-notification icon pulse lasts about 2 seconds; more arrivals during the pulse do not keep resetting its timer. Updating... can appear while new text is being prepared instead of presenting old text as the latest message.

Quick replies are **1–7 custom text / 8 location + this-session statistics + music / 9 location / 10 music**. Replies 8–10 include the signature. Location includes a map link. When music is not playing, the text reports no media information. Check the recipient and content before confirming a real send.

**Dash blink alerts** temporarily alternates cluster brightness between high and low. Disable it in Settings → Display if unwanted. Call/notification/warning popups use four 0.5-second steps; low fuel uses eight. Toast messages are excluded. This is not blinking the Noodoe LCD backlight.

<a id="map"></a>
## 14. Maps and automatic scale

Import a regional **`.map` file** in the phone's map menu. It is neither firmware nor a language package. A GPS trail/moving grid without road-map data is an expected fallback.

- **Long UP on the map:** AUTO / MANUAL.
- **AUTO:** speed-based scale, with UP/DOWN preference from −2 (closer) to +2 (wider).
- **MANUAL:** UP/DOWN changes scale without the continuous automatic zoom animation.
- **Long O:** north-up / heading-up / compass.

A new area needs preparation and transfer time. Map preparation/transfer stops outside the map view. Check phone location permission/services if no position is available. The current map/location/trail display is not a promise of a complete destination-search and turn-by-turn navigation service.

<a id="dashboard"></a>
## 15. Phone dashboard, customization and preview

The app has **Home / Customization / Installation and status** pages, reached by swiping horizontally. The top play/pause button controls riding integration.

Home separates map/speed/ODO from trip and maintenance information. **Tap the trip display itself** to cycle it. Maintenance uses horizontal gauges: a larger oil display and separate belt/service displays with remaining distance. For stock devices, the trip/maintenance space becomes **received stock-information display**, without inventing CFW maintenance data.

Customization contains photos, notification apps/popups, quick replies, favourite contacts and device settings. Stop riding integration when requested before photo or installation operations. Phone ride-file recording is opt-in; disabling it does not disable device trip calculation.

**UI test mode** works only while disconnected. Enter simulated speed/ODO and CFW/stock modes to preview the phone screen. It does not change actual vehicle data or saved device settings. Turn it off and check the real connection before using live data.

<a id="parking"></a>
## 16. Ignition off, parking and battery

Key-off shows Ride Summary, followed by your configured power sequence. **Ride Summary retains its dark overlay; the following OFF-photo standby screen removes it.** Holding O while turning the key off, then releasing it, provides the up-to-10-minute key-off use path. Button presses do not extend it indefinitely.

In **Settings → System → Power sequence → Long parking power off**, select Off / 2 days (default) / 3 days / 7 days. Existing choices survive updates, so check your actual value. At expiry the device finishes pending storage work and requests shutdown. The next IGN ON starts from Gate. Bluetooth approach reconnection and welcome light are unavailable in that long-parking state.

**A dark display does not mean zero current.** This is not a battery-voltage cutoff protector, and actual standby current or week-long endurance is not guaranteed for every vehicle. If your vehicle has suffered severe discharge, have its battery and standby drain checked first. You do not need to probe circuitry yourself or use an update on a discharged vehicle as an experiment.

<a id="report"></a>
## 17. Reporting a problem and keeping backups

Use **Having trouble? / Recovery and diagnostics → Save/share diagnostics** when a problem repeats. Include:

- Vehicle model/year, phone model and Android version.
- **Both app and device firmware versions**; say if only the app was updated.
- Stock installation or CFW update, previous version and selected ZIP filename.
- Device-screen photograph and Stage/Code/Phase or the 0x error code.
- Last action, IGN state and whether reconnection changed anything.
- For language problems: English only, device language, app language and resource status.

Review diagnostic files before sharing. Do not clear app data and lose the records. Full NOR backup is a separate advanced feature, not the minimal backup made by installation. Keep existing original dumps and app backups on another storage device as well where possible.

[Project help](https://github.com/SerialSniffyHeck3r/noodoe_cfw/issues) · [Downloads](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/latest) · [한국어](Guide.md)

This revision documents current code and distribution behaviour. Software verification is distinguished from physical vehicle, supply and radio tests; unverified fixes are not presented as guarantees. Older version/date-specific research and change-history documents do not override this guide's current installation instructions.
