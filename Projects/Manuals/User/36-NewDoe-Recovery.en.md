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

# NewDoe 0.11.4: startup, recovery and parking

Install the matching NewDoe APK and firmware ZIP. App identity, signature, settings and language preferences are retained. This release changes Gate: **existing 0.9.x, 0.10.x and 0.11.0/0.11.1 installations need a keep-data stock return and reinstallation**. A Product-only update cannot replace Gate. Future releases with the same Gate can use ordinary CFW updates.

## Install this release

1. Install APK 0.11.4 over the existing app; keep its data and backups. Select the same module and matching 0.11.4 ZIP, and stop riding integration.
2. Follow **Return to stock, keep CFW data**, then **Check current device**. Stock 5.14/5.16 remain supported; this release adds no model or bootloader whitelist.
3. Send the new ZIP's Bootstrap and choose **Restore stored data**. If it cannot connect, open connection help, forget the pairing in Android settings, reset Bootstrap pairing as instructed, pair again, then **Check current device → Continue**. No new mandatory pairing-completion state is introduced.
4. Finish installation, normal startup and the actual **CFW UPDATE** confirmation. A transfer reaching 100% is not permanent firmware confirmation.
5. Healthy B resources continue to work. The matching **NewDoe-LANGUAGE-B-0.11.2.zip** adds translated long-parking choices in all nine system languages; existing Japanese/CJ/emoji glyph masks are unchanged. Install the resource update after firmware confirmation if you want these new labels translated. With an older healthy pack, only the new labels fall back to English. Preserve your English-only preference as desired.

The original resident bootloader is unchanged. **Keep permanent power connected throughout stock return/reinstallation.** The accepted older-code stock-return power-loss interval remains: if power is lost before the new Gate runs, automatic recovery is not guaranteed. IGN OFF does not disconnect permanent power. Avoid operating the starter during installation.

## Startup and updates

The first splash shows only the centered NewDoe transparent wordmark for about two seconds, with the installed Product's **V0.11.4** below. Gate reads a bounded, checksummed version record in Product flash; missing or invalid metadata leaves the version blank. It does not guess a version from the phone.

Checking uses **SYSTEM STARTUP...**, the wordmark, a phase label, and eight mint dots. The dots represent overall progress across phases, not the current phase alone. The active dot blinks twice per second (250ms on/250ms off); completed dots stay mint. Text and dots have generous separate spacing. A completed operation lights all eight.

An update restart skips the splash delay. During Gate's firmware installation, **UPDATING SYSTEM...** appears above one of two illustrations, chosen once for that boot. Both are 240×168 with transparency and are stored compressed in Gate's internal flash. The phase and eight blinking progress dots remain visible. Phone-to-Product transfer retains its existing UI and protocol; this artwork describes Gate's local installation stage.

Hold DOWN during the cold-boot splash or for one second during its window to enter recovery. Existing O entry remains available. A detected error goes directly to the skull screen.

## Errors and local recovery

The skull screen shows the existing error number as **0x0000**, followed by **PRESS ANY KEY... 30**. A fresh button press or 30 seconds after the screen appears opens recovery. That press never selects a menu action. Recovery shows **[code] [short reason]**; manual entry leaves the code blank.

Eight rows stay in place: **Start NewDoe, Check again, Reinstall firmware, Restore previous, Return to stock, Details, More, Restart**. Unavailable entries are grey. UP/DOWN selects, O opens. Destructive actions and restart require a fresh two-second O hold; UP/DOWN cancels. Start still requires the original firmware/resource/boot-record checks and cannot bypass a failed trial.

If normal stock restoration is unavailable, its row can offer **Try recovery anyway**. This searches alternate FAT chains, contiguous backup data and an already complete staged stock image, then verifies the device identity, stock target and full SHA before writing. It does not invent missing bytes or accept corrupt images. If a pending language/FONT transaction occupies recovery scratch, a separate explicit approval is required to discard that work. Other files are not deliberately erased.

Failed-attempt flags are reset on reboot and Check again. Durable boot/update records remain validated. A transient storage read failure is not reported as proof that no recovery exists. Only an exhausted deep search or a failed forced restoration can lead to the no-recovery notice; O returns to the menu so the user can retry. Help provides the GitHub link and programmer recovery after opening the module, or replacement if repair is not possible.

**More → Help / Dashboard / Play game / Back.** Dashboard shows received speed, odometer and fuel; missing or stale data stays unavailable, not a made-up zero. It only reads the vehicle data and performs the normal link handshake. Pong uses UP/DOWN for the paddle, displays the current score, and O exits. No Bluetooth is included in Gate. Gate also has no LVGL, SDRAM or external-font dependency.

Help also explains the existing hardware restart: **IGN OFF → press UP + O → release → IGN ON**. In IGN OFF this can look like a power-off; switch IGN back on to start through Gate. If Gate or the display cannot execute, these on-screen recovery instructions cannot appear.

## Long parking and battery use

**Settings → System → Power sequence → Long parking power off:** Off, After 2 days (default), After 3 days, After 7 days. The Android device-settings dialog provides the same choices. Continuous IGN-OFF time is retained across software resets; genuine cold power-up or IGN ON resets it. Invalid retained timer data restarts the count instead of powering off early.

At the limit, Product waits for settings/ride checkpoints, storage drain and all peripheral power acknowledgements, checks IGN again, and issues the power-off request. Active updates and language publication are not interrupted. If the hardware remains powered, Product stays dark with Bluetooth off in deep wait. The next IGN ON resets into Gate. Bluetooth proximity reconnection and welcome light are unavailable after this shutdown; earlier connection-retention behaviour is preserved.

Existing power-sequence timings still apply before the long-parking limit. Gate with IGN OFF also blanks its panel after inactivity and uses RTC STOP wakes to service the watchdog. A button wakes the screen without choosing an action. If RTC sleep is unavailable, bounded ordinary sleep is used.

Software tests cover state transitions, simulated interruptions and display commands. They do **not** measure actual parked current, radio behaviour or one-week battery life. Measure standby current after the configured sleep/shutdown stages and check IGN/button wake on the real module. Product Debug and Release retain at least **512 bytes** of internal flash; heap, stack, existing fonts/icons and compiler policy are unchanged.
