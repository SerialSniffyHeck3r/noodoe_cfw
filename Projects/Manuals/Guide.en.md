<a id="top"></a>

# NewDoe illustrated user guide

**App 0.15.4 / device firmware 0.15.4 · 10 November 2026**  
[한국어](Guide.md) · [Downloads](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/latest)

## 0.15.4 Hotfix 2 — legacy stock-font identification

If an older Product such as 0.12.1 reports “The stock font profile is unknown; no write was started”, install Hotfix 2 APK (build 146) over the existing app, then start with **Check current device status** again. When the legacy Product cannot supply its cached A/B identity, the app reads the actual stock resource using the existing read-only NOR command. It does not modify stock files or FAT, or require a language-pack installation first. Damaged resources, missing replies and active NOR backups still stop the operation.

Riding sync requires the firmware included in this app. If the dashboard still runs 0.12.1, complete the normal update offered after inspection. If it already runs 0.15.4, only the APK needs updating. Do not clear app data, re-pair or return to stock as the first response to this error. Existing Gate compatibility checks remain; follow the separate guidance if an actual Gate mismatch is detected.

## 0.15.4 Hotfix 1 — notification processing patch (build 145)

This app patch addresses the reported **CONTENT / 0x74 / IllegalArgumentException** after updating from 0.15.3 to 0.15.4 while the dashboard appears normal. An unreadable notification icon falls back to a default icon without terminating notification delivery or the riding connection. Other local failures now include a code location without private notification text.

**If your dashboard already boots 0.15.4 normally, install the Hotfix 1 APK over the existing app and reconnect.** App-data deletion, re-pairing, stock recovery and another firmware installation are unnecessary for this patch. Android build 145 contains the same firmware bytes and installer ZIPs as 0.15.4. If the dashboard still runs 0.15.3, use the normal update flow in the patched app to install 0.15.4. Unconfirmed installation results are not silently marked successful.

## 0.15.4 — Clock contrast and initial setup improvements

Update the app and dashboard firmware together. Choose **Use the package included in the app**, then review the detected device and installation language. Gate and existing NOR file layouts are unchanged; healthy devices use the normal Product update. Existing users are not reset or forced through setup again.

### After a fresh installation

1. When initial setup is scheduled, the dashboard opens it directly, without the ordinary ignition arc animation. At **WELCOME! / NEWDOE** and the blinking **Press Any Key To Continue** message, press UP, DOWN or O. The next screen says **Please stop your vehicle / Press O to continue**. Stop, then press O to begin. This screen does not advance automatically.
2. Follow **language → sunrise/sunset region → actual fuel bars → fuel recognition ON/OFF → units → timezone → speed-arc maximum**. UP/DOWN changes the value and O saves it. Select English or the language installed with your Product.
3. Enter the zero-to-five bars on the original gauge. With ignition ON, NewDoe compares stable UART samples. Matching bars recommend ON; a mismatch or unsupported value recommends OFF. You may change the recommendation with UP/DOWN; only O saves it. Wait or choose **Skip** if samples are absent or unsettled. Missing data alone does not establish incompatibility.
4. The dashboard restarts after durable completion. A failed save does not advance; review the message and retry. While connected, the phone automatically explains the current dashboard step and controls. This guide cannot be dismissed during setup; reconnecting restores the current step.
5. To repeat later, choose **Settings → System → Initial setup (restart)**. Unrelated preferences are preserved. **Reset system settings** is a separate confirmed action. Trips, maintenance records, photos and pairing are retained.

| Welcome | Fuel check |
|---|---|
| <img src="images/guide-0.15.4/setup-first-frame.png" alt="WELCOME, centered NEWDOE wordmark and button prompt" width="280"> | <img src="images/guide-0.15.2/setup-2.png" alt="Enter the original gauge fuel bars without setup position arcs" width="280"> |

The initial setup wizard has no movement lock or automatic timeout. Its first screen and current step remain present and its buttons remain usable while moving. Follow the instruction to stop before continuing. Ordinary settings retain their existing movement lock and timeout.

The settings category arc and light mode have been removed. Ordinary settings retain their side row indicator; setup has no position arcs.

### Using weather

Weather is OFF by default. In the phone app's **Weather** screen, save your own **API key enabled for OpenWeather One Call**, then enable weather. Provider registration, terms and charges may apply; NewDoe does not supply a shared key. Other features remain usable without a weather key.

#### Register OpenWeather and show the first forecast

1. [Create an OpenWeather account](https://home.openweathermap.org/users/sign_up), verify your email and find your personal key under [API keys](https://home.openweathermap.org/api_keys). A new key can take time to activate. [Official account/key help](https://openweathermap.org/faq)
2. Check **One Call API access**. NewDoe uses the current, hourly and daily data from the `One Call 3.0` endpoint. Having a key for an ordinary free account does not by itself confirm One Call access. Review the provider’s subscription, payment requirements and daily call limit before accepting. At publication the official page lists 1,000 free calls/day for One Call; excess charges and account terms remain the provider’s responsibility. [Current official One Call documentation](https://openweathermap.org/api/one-call-3)
3. Open **Weather** in NewDoe, paste your key into **OpenWeather API key**, then choose **Save key**. The key is masked and the input clears after saving. Keep it out of Discord messages and screenshots. NewDoe does not supply a shared key.
4. Enable **Weather**. Allow location, enable the phone’s location service and connect to the internet. Weather uses the phone’s current GPS location, independently of the city selected for sunrise/sunset in initial setup.
5. Connect the phone to the dashboard. On the dashboard, use **HOME → UP/DOWN → Weather**. Merely opening Weather on the phone does not fetch. The phone’s dashboard-weather query button does not bypass the query policy.
6. When the phone reports **Forecast ready**, check its update time and the dashboard’s current/high/low temperatures and two bars. Temperature follows the dashboard’s unit setting; only a degree mark is shown. The clock and ODO retain their usual positions outside the weather view.

#### Refresh timing and errors

After the first attempt, fetching requires the dashboard Weather page to be visible and either **one hour since the last attempt or 30km from its location**. The 30km threshold is displacement, not accumulated odometer distance. Other pages do not fetch; returning to Weather does not reset the limit. Failed requests also count, so correcting a key may not cause an immediate retry.

| Phone status | What to check |
|---|---|
| Key required / key storage error | Check and save the complete key. Do not share it. |
| Key or One Call access rejected | Check activation, spelling and subscription in the provider account. |
| Waiting for location | Enable location and permissions; wait for a fresh fix. |
| Rate limit | Check provider usage and limits. Repeated button presses do not bypass it. |
| Network / invalid data | Check connectivity. The next attempt still follows page/time/distance conditions. |
| Stale forecast / no dashboard data | Check the update time and wait on the dashboard Weather page for the next allowed request. |

Forecasts remain in phone/dashboard RAM. A reboot or app-process restart requires fresh data; forecasts are not repeatedly written to dashboard NOR. Turning Weather off stops requests. Weather is optional; other features work without an account.

On the connected dashboard, use UP/DOWN in HOME to select **Weather**. The smiling sun or Meteocons condition icon sits beside the large current temperature. Daily high uses vivid red; low uses blue. Current temperature has three fixed-width slots without leading zeros.

The two chevron bars run from the **current clock hour through +12 hours**, left to right.

- **Upper — condition:** clear is yellow, partly cloudy pale yellow, cloudy muted white. Drizzle/rain/heavy rain progress through deeper blue tones. Fog has dark stripes on a light background; showers, thunderstorms and snow use blue. Probability never overrides this bar.
- **Lower — precipitation amount(mm/h):** white means no precipitation; larger rain/snow amounts become darker blue. This is not probability(%). Colors blend between hours; an unavailable amount is dark gray.
- Weather hides the ordinary HOME information strip. After the top menu hides, a localized Weather title appears when no TBT is present. The clock, speed arc and ODO remain.
- Missing or expired data is not shown as current; unavailable temperatures show `--`.

| Clear and two forecast bars | Subzero evening example |
|---|---|
| <img src="images/guide-0.15.4/01-sun-morning.png" alt="Smiling sun, fixed-width temperature, condition and precipitation bars" width="280"> | <img src="images/guide-0.15.4/04-negative-evening.png" alt="Minus3 degrees in fixed-width slots" width="280"> |

Queries require the dashboard Weather page to be actually visible, an ignition-ON connection, valid location and a key. After the first eligible attempt, another is permitted after **one hour OR30km from the last query location**. Other pages do not fetch; leaving the page cancels pending work. Reopening after a failure does not bypass the interval. Forecasts live only in phone/dashboard RAM. The manually selected sunrise/sunset city is independent of the weather location.

### Fuel measurement error

A sustained measurement error replaces GPS with a steady **red crossed-fuel icon**. The blinking Fuel Measurement Error text and full-screen error animation are removed. A valid fuel frame or fuel recognition OFF restores GPS. Existing low/critical-fuel warnings retain their behavior.

<img src="images/guide-0.15.2/fuel-error.png" alt="Red crossed-fuel measurement indicator in the GPS position" width="280">

Weather artwork uses [Meteocons Fill](https://github.com/basmilius/meteocons) under MIT. The sun is separate generated artwork selected for the project. Attribution and license text are available in the phone Weather screen.

Weather artwork retained in earlier guide images derives from [Twemoji](https://github.com/jdecked/twemoji), [CC BY4.0](https://creativecommons.org/licenses/by/4.0/), with resized/palette-converted graphics and modified rain variants.

These are ARM/LVGL/EVE software renders with synthetic weather, not live forecasts or physical LCD photographs. Authenticated OpenWeather requests, physical LCD/radio and power consumption remain unverified. The existing stock-return power-interruption limitation remains.

## 0.15.1 — Fuel recognition and paired updates

The app includes matching A/B and language firmware/resources. An older device receives an update notice after connection. Choose **Use packages included in this app** in the notice, then review the detected device, language and operation before updating. **Check current device status** remains separately available. Riding sync starts after the firmware included in this app is confirmed. Installation, stock return and reset are never automatic. An interrupted installation retains its original device/file records; a newer APK never silently replaces its files.

<!-- FUEL_PAIR_0151_BEGIN -->
| Update notice for older firmware | Fuel recognition setting |
|---|---|
| <img src="images/guide-0.15.1/firmware-update-0151.png" alt="Matching firmware update notice" width="280"> | <img src="images/guide-0.15.1/fuel-recognition-0151.png" alt="Fuel recognition on/off and manual reset help" width="280"> |

Emulated 0.15.1 app screens, shown in Korean. An existing fuel-alert OFF preference becomes fuel recognition OFF after updating.
<!-- FUEL_PAIR_0151_END -->

You need an Android phone and your vehicle's buttons. Normal installation does not require opening Noodoe or connecting a programmer. **[PHONE] means the Android app; [NOODOE] means the round vehicle display.** Distances and progress values in the screen examples will differ on your device.

![First installation overview](images/guide-0.13.0/install-flow-en.svg)

**Jump to what you need.**

| What do you want to do? | Start here |
|---|---|
| Install for the first time | [Prepare](#prepare) → [Follow the screens](#first-install) |
| Update an existing installation | [Updating NewDoe](#update) |
| Change the menu language | [Language resources and settings](#language) |
| Fix a connection or interrupted installation | [Connection help](#connection) · [Screen-by-screen help](#troubleshooting) |
| Return to stock without a phone | [Device recovery menu](#recovery) |
| Use NewDoe | [Buttons](#controls) · [Trips and maintenance](#vehicle) · [Media](#media) · [Maps](#map) |
| Set up the app or parking | [App pages](#dashboard) · [Parking](#parking) · [Get help](#report) |

<a id="compatibility"></a>
## 1. Can I install it?

| Item | Supported |
|---|---|
| Phone | Android 6.0 or later. There is no iPhone app. |
| Noodoe unit | F4-family KYMCO Noodoe. Other vehicle models besides the AK550 can use the app's device check. |
| First installation from stock | Stock firmware **5.14 or 5.16** |
| Already running CFW | Follow [Updating NewDoe](#update). |
| Device menu languages | Nine choices on B units; English, Japanese and Simplified/Traditional Chinese choices on A units |

You do not need to open the unit to identify it. Tap **Check current device** in the app. If it reports an unsupported device, save the message and [ask for help](#report). Returning to stock installs **stock 5.16**.

<a id="basics"></a>
## 2. Buttons and the ignition key

![UP O DOWN buttons](images/guide-0.11.15/buttons-en.svg)

- **O** is the centre confirmation button, not the number zero.
- A **short press** means press and release. A **hold** means keep it pressed; follow the displayed duration when a screen says two or three seconds.
- **IGN ON** means turn the vehicle key/power switch on. **It does not mean start the engine.**
- **IGN OFF** means turn the key/power switch off. **Do not disconnect the battery or Noodoe wiring.**
- If your vehicle has a switch that assigns the buttons to Noodoe or the normal dashboard, select **Noodoe**.

**Bootstrap** is the temporary installer used for first installation. **CFW** is the NewDoe firmware you normally use. **Recovery/Gate** is the device's startup and recovery menu.

<a id="prepare"></a>
## 3. Get ready

### Download just the APK

Get **NewDoe-0.15.0.apk** from [Downloads](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/latest). Language-specific firmware and required fonts are included. Normal installation does not need a separate ZIP download or file selection.

**App and device firmware are 0.15.0.** Choose a system language in the app; it prepares matching firmware and resources. Installing the APK alone does not change the device firmware.

### Park before you begin

1. Park safely and stop the engine. Make sure the vehicle and phone batteries have enough charge.
2. Open the new APK and install it. If NewDoe is already installed, **update the existing app**. Do not uninstall it or clear its data.
3. If Android asks to allow installation from this source, check that you downloaded the APK yourself and allow it. Open NewDoe afterward.
4. Enable Bluetooth and keep the phone close to the vehicle. Leave NewDoe in the foreground during installation.
5. Disconnect any other Noodoe app that is using the device.

**Keep the battery and Noodoe wiring connected during installation and stock recovery.** Do not operate the starter. Charge or service a heavily discharged vehicle before updating it.

### Confirm the five safety screens

When you press an install button, read each picture and tap **Got it**. **Cancel** stops before installation starts. The same installation can continue through Bootstrap without repeating these confirmations. A different device, file or new installation requires them again.

1. Do not disconnect the battery or Noodoe wiring.
2. With the key OFF, do not press **UP + O** together.
3. Do not press the starter button.
4. Check that the battery voltage is **at least 11.5V**. This is your confirmation; the app does not measure it.
5. Keep the key OFF when the step allows it. **The first transfer from stock needs the key ON.** Then follow the screen instructions.

<img src="images/guide-0.15.4/en-safety-0.png" alt="Battery safety confirmation in the app" width="330">
<img src="images/guide-0.15.4/en-safety-4.png" alt="Key OFF recommendation with the first-transfer exception" width="330">

All five signs stay under the installation status while it runs. They are reminders, not extra buttons.

### Choose guidance when installing from stock

On first launch, answer whether you have used NewDoe before and read the button guide if needed. Open **Device/status → Check current status**. Once the actual device is identified as stock, choose **Install** or **Install with step-by-step guidance**. Use the guide if you are unfamiliar with installation.

The optional guide starts only from stock firmware. Existing CFW uses the normal update screen. After a transfer or verification starts, wait for that operation to finish. Reopening a guide does not start installation.

<img src="images/guide-0.13.0/en-stock.png" alt="Stock installation after device inspection" width="330">

<a id="first-install"></a>
## 4. First installation from stock

### ① Select your Noodoe on the phone

<img src="images/guide-0.11.15/en-device.png" alt="Device page with pairing and permissions" width="330">

1. **[NOODOE]** Turn the key ON without starting the engine.
2. **[PHONE]** Open the **Device** page. Swipe sideways or use its bottom button.
3. For a first connection, tap **Pair Noodoe** to open Android Bluetooth settings. Put stock Noodoe into pairing mode and select your unit. Do not pair again if it is already registered.
4. Return to NewDoe and select your device. Refresh the device list if it is missing.
5. Open **Permissions & integration** and allow Bluetooth/Nearby devices access. Music, calling and location permissions can be set up [after installation](#phone).
6. Tap **Check current device**. Confirm that the app identifies **stock Noodoe**.

### ② Send the installer using included files

1. **[PHONE]** After checking the device, review the app, included firmware and prepared firmware versions. The app prepares the files automatically.
2. Choose **Install**, or **Install with step-by-step guidance** for help.
3. Read and acknowledge each safety screen. Cancel sends no installation command.
4. Wait for the first transfer and check to finish. Do not turn the key OFF early during the stock transfer.

### ③ Wait for Bootstrap, then tap Check current status

<img src="images/guide-0.13.0/en-bootstrap-wait.png" alt="Bootstrap transferred: Check current status button" width="360">

1. **[PHONE]** When this message appears, follow the stock display's instruction to turn the **key OFF**. Leave the battery connected.
2. **[NOODOE]** Wait until the **NewDoe Bootstrap menu** below appears. The intermediate **NOODOE INSTALLER** screen is not that menu.
3. When Bootstrap is visible, turn the **key ON**.
4. **[PHONE]** Tap **Check current status**. This step needs your tap. Do not resend Bootstrap.

| A brief file check comes first | Then the Bootstrap menu appears |
|---|---|
| <img src="images/guide-0.11.16/bootstrap-check.png" alt="Bootstrap file check with a horizontal bar" width="300"> | <img src="images/guide-0.11.16/bootstrap-menu.png" alt="Bootstrap menu with Install CFW selected" width="300"> |

The first three seconds show only the NEWDOE wordmark. Checks continue in the background. If still running afterwards, the current check and a horizontal bar appear around the wordmark; otherwise the menu opens.

<img src="images/guide-0.11.16/bootstrap-logo.png" alt="NEWDOE" width="300">

### ④ Choose Install CFW on Noodoe

1. **[NOODOE]** Use UP/DOWN to select **Install CFW**, then press **O**.
2. At **Reset Bluetooth?**, keep the default **No** and press **O** if your connection is working.
3. The phone continues the installation. You do not need to find another installation-start button on the phone.

<img src="images/guide-0.11.16/bootstrap-reset-no.png" alt="Optional Bluetooth reset with No selected" width="360">

**Yes erases saved pairings.** Use it only when you need to reset a failed connection. For “Couldn't pair,” follow [the pairing recovery steps](#connection). This question has no automatic countdown.

If the app finds data from a previous CFW installation, choose **Restore retained data** to keep your settings, trips and photos. Choose **Start fresh** only if you want a new setup. You may not see this choice on a first installation.

### ⑤ Wait while the two bars fill

<img src="images/guide-0.11.16/bootstrap-transfer.png" alt="Overall and current-operation progress bars" width="360">

- **Upper, lighter bar:** overall installation progress.
- **Lower, darker bar:** the current operation. It can start again when the next operation begins.
- The text identifies the file or operation. Your numbers may differ from the example.

Keep power and the phone connection available. Do not force a restart just because progress is slow. To cancel, follow the displayed **hold O for three seconds** instruction, then wait for the stopped message.

### ⑥ Everything is ready: install

<img src="images/guide-0.11.16/bootstrap-confirm.png" alt="Final INSTALL CFW confirmation with Back and Install CFW" width="360">

The files are ready.

- Install now: select **Install CFW → O**.
- Leave it alone: installation begins **after 30 seconds**.
- Go back: press UP/DOWN to stop the countdown, then choose **Back → O**.

Noodoe restarts during installation. Leave the app open even if the connection briefly drops or you see a startup screen.

<a id="confirm-update"></a>

### ⑦ Look at CFW UPDATE on Noodoe, then confirm on the phone

| Look for this on Noodoe | Then confirm on the phone |
|---|---|
| <img src="images/guide-0.11.15/cfw-confirm.png" alt="Device CFW UPDATE screen" width="300"> | <img src="images/guide-0.15.4/en-confirm.png" alt="Phone confirmation that CFW UPDATE is visible" width="330"> |

1. **[NOODOE]** Check that it shows **CFW UPDATE** and **Checking the new version** or **Confirm on your phone**.
2. **[PHONE]** Tap the button confirming that **CFW UPDATE is visible**. If Noodoe still shows Bootstrap, NOODOE INSTALLER or a black screen, choose the “not that screen yet” option and wait.
3. Wait for the phone to report **installation complete** and for Noodoe HOME to open.

**Confirm before the phone's countdown expires.** Otherwise the device may return to the previous version or recovery menu. Dismissing the popup does not confirm the installation.

### ⑧ Check the selected language

The app prepares your selected system language and any required fonts. Wait for completion, then check the device menu. If it opens in English, see [language status](#language).

<a id="update"></a>
## 5. Update an existing NewDoe

**Existing users: check the device, choose a system language and update to 0.15.0.** Compatible Gate firmware and healthy existing B fonts are retained.

1. Install `NewDoe-0.15.0.apk` over your existing app. Keep its data and pairing.
2. Stop the riding connection and check the current device on the status page.
3. Choose **Language settings → Install another language**, tap **Update**, and follow the safety screens.
4. Keep power connected and let transfer, verification and restart finish.
5. Confirm the device's **CFW UPDATE** screen in the app. If additional fonts are being prepared, wait until the final installation completes.

### When a 0.14.1 settings error blocks an update

HOME can work while settings restoration remains incomplete. **Check current device** only reads status. It does not retry settings restoration or installation.

1. Install the new APK over the existing app. Keep app data and export diagnostics.
2. Read **Product boot confirmed** separately from **Settings restoration incomplete**. **Continue installation** can retry the same operation. If 0.14.1 keeps rejecting settings, use the following route.
3. Open **Data-preserving reinstallation** in the app. If Bluetooth cannot connect, [open recovery with the device buttons](#recovery) and choose **Back to stock**. Use **Keep CFW data**, never complete removal.
4. Wait for the stock screen, then tap **I have returned to stock · check** in the app. After reading the actual stock state, the app archives the old operation and prepares reinstallation.
5. Select your language and follow [first installation](#first-install). Choose **Restore retained data** in Bootstrap. This route retains settings, TRIP, maintenance, photos and pairing.
6. Check the device and final app result. A missing or damaged required data file is not automatically initialized. Save diagnostics and stop if this is reported.

**Healthy devices that save settings and prepare an update normally do not need a stock return.** Treat data marked unverified as unverified. Cancelling does not undo completed steps.

### If the app requires the newer recovery system

From 0.11.x or another older Gate, follow **Return to stock → Keep CFW data** first. Once the stock screen appears, tap **I have returned to stock · check**, then follow [first installation](#first-install) using the included package. Choose **Restore retained data**. Do not select complete removal.

Settings, trips, maintenance records and photos are retained. **Do not disconnect power during stock return or installation. An interruption in older stock-return code can require external repair.**

<a id="language"></a>
## 6. Choose or change the system language

**Choose the language in the app before installing.** Install English only, or English plus one selected language. Required files are included in the APK.

### First selection or a different language

1. Park the vehicle and tap **Check current device**. The app identifies the actual A/B resources.
2. Choose **Language settings → Install another language**. Only languages for that resource profile are offered.
3. Tap **Install / Update** and acknowledge the safety instructions.
4. Let transfer, verification, restart and **CFW UPDATE** confirmation finish. If additional fonts are being prepared, wait for the app to report completion.
5. Check the device menu. The app shows **installed language** and **display language** separately.

**Changing language normally does not require returning to stock.** The app updates firmware and its matching base resources together. Healthy additional fonts are reused without another transfer.

| Japanese menus on an A unit | Korean menus on a B unit |
|---|---|
| <img src="images/guide-0.14.0/a-japanese.png" alt="Japanese main menu on A resources" width="300"> | <img src="images/guide-0.14.0/b-korean.png" alt="Korean main menu on B resources" width="300"> |

### Switch between English and the installed language

Open **Language settings** in the app to see the app language, installed menu language, actual display language, English selection, text rendering and supplemental font status. To install another language, choose it under **System language** and confirm. After the device check finishes, use **Continue installation / Check language** to update Product and its matching base resources. Changing only the app language does not change the device menu.

<img src="images/guide-0.15.0/language-settings-en.png" alt="App and device language settings with text rendering controls" width="280">

On Noodoe, open **Settings → System → Language**. **Firmware** shows the installed language; **Language** chooses the display language. Select English or the installed language, save and restart while parked. An English-only Product offers only English.

<img src="images/guide-0.15.0/device-language.png" alt="Device language menu with firmware language, display language and text rendering" width="320">

English-only firmware still displays foreign-language music and notifications. It does not require making room for additional fonts just to use English menus.

### Available languages

| Device resources | System language choices |
|---|---|
| A | English · 日本語 · 简体中文 · 繁體中文（台灣）· 繁體中文（香港） |
| B | English · Deutsch · Español · Français · Italiano · Ελληνικά · 한국어 · Tiếng Việt · 日本語 |

**App language** changes the phone UI. Choose and install the device language separately; changing the phone language afterward does not change it. An old “Follow app language” selection is resolved to one installed language during the update.

### When additional fonts need preparation

Healthy existing B packs remain usable as font resources. Stock files and existing photos are not deleted for cleanup. If the font file is missing or too small, the app explains **why a preparation Product is needed and how it returns to the selected final Product** before asking you to continue. Bluetooth, pairing and settings remain available in the preparation Product.

The preparation Product displays **FONT PREPARATION** and does not provide HOME, music or map pages. Stay parked and continue installation in the app. `Font preparation complete` means that the fonts are ready; **the selected final firmware must still be installed before riding screens return.** For connection help, briefly press UP on the idle preparation screen to open the existing pairing help. Pairings are not deleted automatically.

| Waiting for the phone in the preparation Product | Not enough font space |
|---|---|
| <img src="images/guide-0.15.0/preparation-waiting.png" alt="Example FONT PREPARATION connection screen" width="230"> | <img src="images/guide-0.15.0/preparation-no-space.png" alt="Example insufficient font space screen" width="230"> |

While idle with the ignition OFF, the display turns off under the existing power policy. Font installation or final firmware transfer wakes the installation display. Existing photos, trips, maintenance records, settings and pairings remain stored.

| 1. Transfer preparation Product | 2. Inspect storage | 3. Receive fonts |
|---|---|---|
| <img src="images/guide-0.15.0/helper-transfer.png" alt="Preparation Product transfer" width="230"> | <img src="images/guide-0.15.0/storage-check.png" alt="Font storage inspection" width="230"> | <img src="images/guide-0.15.0/font-transfer.png" alt="Font transfer progress" width="230"> |

| 4. Verify fonts | 5. Complete storage | 6. Install the selected final Product |
|---|---|---|
| <img src="images/guide-0.15.0/font-verify.png" alt="Font verification" width="230"> | <img src="images/guide-0.15.0/font-publish.png" alt="Font storage completion" width="230"> | <img src="images/guide-0.15.0/final-transfer.png" alt="Final Product transfer" width="230"> |

**Insufficient CJ space pauses installation until you choose.** Read the required size and the available capacity reported by the device. If exact free capacity is unavailable, it is labelled unknown.

- If the chosen menu works without CJ, choose **Continue without CJ / Cancel**. Unsupported music and notification text then needs phone rendering. Without phone rendering, those characters may not display.
- If mandatory menu fonts are missing, choose **Select another available language / Cancel**. The app does not silently choose English or mark that language installation complete.
- If you cancel while the preparation Product is running, **it remains installed**. Cancelling does not undo finished steps. Keep app data and the operation record to resume or select another language later.
- If you skip CJ, the result and language settings retain **Installation complete — CJ not installed**. Installing it later requires a fresh space check and confirmation.

Optional CJ / required menu font unavailable

<img src="images/guide-0.15.0/cj-en-optional.png" alt="Optional CJ installation choice" width="280"> <img src="images/guide-0.15.0/cj-en-required.png" alt="Required font language choice" width="280">

Lost connections, timeouts and damaged storage are distinct from insufficient space. Read the failed stage and error code and save diagnostics. After an interruption, keep the same app data and use **Check current device → Continue installation**. Checking status alone does not write again. Ordinary startup does not hash or retransmit the entire font file.

| Music and notification text setting | Behavior |
|---|---|
| **Render on phone** | Uses the phone fonts and colour emoji. |
| **Use device fonts** | Uses the device only when it can display the entire field. If any character is unsupported, the whole title or body is rendered on the phone. |

If menus remain English, check **installed language → English only → restart confirmation → font status and fallback reason in the app**.

<a id="expert"></a>
## Expert settings and interrupted installations

For normal use, choose **Check current status → Install / Update**. Manual ZIP installation, diagnostics, recovery and backups remain in **Device/status → Expert settings**. Manual ZIP selection shows a caution screen; installing the selected ZIP shows it again. Selecting a file does not install it. Choose **Use packages included in this app** to return to the default source.

After an app update or lost connection, check the device first. An unfinished installation continues with its original files and records, even if the new app contains a newer package. If an original file is missing or damaged, import that exact original ZIP through Expert settings. Do not substitute another version.

Files and records live in app-private storage. **Uninstalling the app or clearing its data removes them.** Export **Backup and operation records** from Expert settings first. If storage is full, free other unnecessary files and retry. The app does not automatically delete recovery evidence or ongoing packages. An uncertain operation is checked on the device before further work.

<a id="connection"></a>
## 7. When the phone cannot connect

### “Couldn't pair” in Bootstrap

If files are being written, let that finish first. Follow these steps at the Bootstrap menu or connection screen.

1. **[PHONE]** Open **Can't connect? / Bootstrap connection help**.
2. Tap **Open Bluetooth settings**, then **forget only your Noodoe** in Android. Keep the NewDoe app's data.
3. **[NOODOE]** If you already chose **Reset Bluetooth? → Yes**, follow that screen. Otherwise, use **UP** on the connection screen to open the pairing menu and choose the phone. Release O, then **hold O again for two seconds** to delete it. ALL deletes every phone.
4. At **PAIR YOUR PHONE**, search for Noodoe again in Android and pair it.
5. Return to NewDoe, select the same device, and tap **Check current status**.
6. Choose **Install CFW** on Noodoe. If the recovery workflow appears, follow **Check last operation → Check and resume installation**.

### Disconnection during normal use

**0.14.1 saves a rotated Bluetooth bond key for the next connection.** Install the new APK over the existing app, then use **Check current device → Update** to update Noodoe too. Updating only the APK does not install the device fix.

Check that the app's riding connection is not paused and Bluetooth/Nearby devices access is allowed. Allow NewDoe to run in the background in Android's app battery settings. After force-stopping the app, open it again and start the connection. If another app, such as the camera, interrupted it, return to NewDoe and check the connection.

If re-pairing has already restored your connection, use that connection to update Noodoe. If reconnection still fails afterward, export the diagnostic ZIP and note both app and firmware versions before restarting or re-pairing. Keep app data. If a settings save/restore error blocks updating, follow the [0.14.1 reinstallation route above](#update).

**The recovery menu has no Bluetooth connection.** Use [the device buttons](#recovery) when the skull or Recovery menu is showing.

<a id="troubleshooting"></a>
## 8. What should I do on this screen?

| Screen or message | What to do |
|---|---|
| **NOODOE INSTALLER** | Wait for Bootstrap or the next screen. Do not confirm CFW UPDATE yet. |
| **100% / Verifying and saving** | Keep power connected and wait for saving. See [the remaining stages](#language). |
| **CFW UPDATE** | Tap the phone button confirming that this screen is visible. |
| **Firmware installed / Language resources are incomplete** | Firmware is installed. Reconnect to the same device and choose **Continue installation / check language status**. Do not immediately return to stock or reinstall firmware. |
| **opcode 0xAA / socket closed / read -1** | Check whether Noodoe restarted, reconnect, and check resource status again. Save the error and diagnostics if it repeats. |
| **Installation success or cancellation not confirmed** | Keep the same device and ZIP; use **Check current device → Check last operation**. |
| **Paused / Stage 6/8 / Code 6 / Phase 4** | Look for an explicit manual-reset instruction. If shown, follow the steps below. |
| **English after a language change** | Follow [the four language checks](#language). |
| **ODO ------ / no speed** | Check that the ignition is ON. If it persists, send your vehicle model and a screen photo. |

### When the device says “Please manually reset”

1. Photograph the message and code.
2. Release all buttons, then **hold UP + O together for three seconds** and release.
3. With the key OFF, the screen may stay dark. Release the buttons and **turn the key ON**.
4. Check the same device and ZIP again. Confirm CFW UPDATE if shown, or follow the recovery menu below.

Use this reset **when the device asks for it**. Do not interrupt writing just because progress is slow.

<a id="recovery"></a>
## 9. Recover or return to stock without a phone

### Open the recovery menu

- When the device starts from power-off and shows the NewDoe logo, **hold DOWN for about one second**.
- If CFW is running: **key OFF → hold O → keep holding while turning key ON → hold for at least two more seconds → release at recovery**.
- If UP+O with ignition OFF left the screen dark, release the buttons and turn the key ON to start again.

| An error shows this first | Press a button, or wait 30 seconds, for the menu |
|---|---|
| <img src="images/guide-0.11.15/gate-error30.png" alt="Skull, hexadecimal error code and countdown" width="300"> | <img src="images/guide-0.11.15/gate-manual.png" alt="NewDoe recovery menu" width="300"> |

Manual entry may go straight to the menu. Use **UP/DOWN to move and O to select**. Grey items are unavailable right now. The reason and code at the top depend on how you entered recovery.

| Menu item | What it does |
|---|---|
| **Start NewDoe** | Start the current firmware |
| **Check again** | Look for recovery options again |
| **Reinstall firmware** | Reinstall using the stored installation image |
| **Restore previous** | Recover the previous firmware |
| **Return to stock** | Restore stock firmware |
| **Try recovery anyway** | Look for another recovery copy when normal recovery failed |
| **Details** | Show error and status details |
| **More** | Help, dashboard values and a simple game |
| **Restart** | Restart the device |

### Return to stock

1. Select **Return to stock → O**.
2. Read the confirmation, **release O and hold it again for two seconds**.
3. On the new Gate, read the battery, UP + O and starter warnings in order. **Release O, then press it again** for each screen. UP or DOWN goes back. These screens also appear for a manually chosen forced stock recovery.
4. Keep power connected until the stock screen appears. **No phone connection is needed.** Older recovery menus may not show the three new pictures.

<img src="images/guide-0.11.16/gate-safety-0.png" alt="Battery" width="280">
<img src="images/guide-0.11.16/gate-safety-1.png" alt="UP + O" width="280">
<img src="images/guide-0.11.16/gate-safety-2.png" alt="Starter" width="280">


| Example recovery confirmation | Stock recovery in progress |
|---|---|
| <img src="images/guide-0.11.15/gate-confirm.png" alt="Example hold-O recovery confirmation" width="300"> | <img src="images/guide-0.11.15/gate-stock.png" alt="Stock recovery progress" width="300"> |

If recovery fails, use **Check again** and, if available, **Try recovery anyway**. Press **O** to leave an unsuccessful-recovery message. If it keeps failing, send the code and a photo through [Get help](#report). If no recovery copy can be used, servicing or recovery after opening the unit may be needed.

**Use recovery scratch** means giving up an unfinished language/font repair so its workspace can be used for recovery. Approve it only if you want to abandon that unfinished job.

### Return to stock using the phone

Stop the riding connection, then choose **Return to stock → Keep CFW data**. Confirm on the phone after the actual stock screen appears. Choose **Restore retained data** during a later reinstall to keep your environment. Choose **Remove CFW data completely** only when you intend to erase those records.

<a id="phone"></a>
## 10. Phone connection and permissions

Select your Noodoe and use the app's top **play** button to start the riding connection. The top **pause** button pauses the connection; it is separate from your music player's pause button.

| Permission or setting | Used for |
|---|---|
| Bluetooth / Nearby devices | Connection and installation |
| Notification access | Music information and chosen app notifications |
| Precise/background location | GPS and maps, including with the phone screen off |
| Contacts, phone and recent calls | Caller names and calling features |
| App notifications/background operation | Connection status and reconnection |

Open **Permissions & integration** and enable the features you want. Calls that do not provide a number may show **Unknown caller**.

<a id="controls"></a>
## 11. Everyday device controls

| In a normal screen | Action |
|---|---|
| **Short O** | Cycle HOME → trips → music → phone → map |
| **Short O + DOWN** | Open quick brightness settings |
| **UP / DOWN** in quick settings | Adjust brightness |
| **Short / long O** in quick settings | Close / open the main menu |
| **Long UP** in quick settings | Restart the display output |
| **Long DOWN** in quick settings | Super Nite low brightness |

Follow the on-screen hints while a call, notification or menu is open. Park before changing full settings.

The five top icons hide after a while on HOME and stay visible on other normal pages. **Grey GPS** means waiting for a location; fresh positions cause a short grey pulse. **Yellow/red Bluetooth** indicates low phone battery; **green** indicates charging.

<a id="vehicle"></a>
## 12. Odometer, trips and maintenance

From 0.15.1, **oil intervals use odometer distance only**. Key-ON duration is no longer measured or applied to any maintenance item. Existing distances since service are kept; historical hours are never converted into kilometres. If you previously configured only an hour interval, set a distance interval in Maintenance. Belt/service calendar criteria and riding-time/average-speed statistics remain available.

<img src="images/guide-0.11.15/en-trip.png" alt="Trip computer screen" width="360">

**ODO is the current odometer value received from the vehicle's instrument cluster.** Missing data shows `------`. There is no separate Noodoe odometer correction or saved-ODO selection.

Trip A/B, today, this ride, oil, belt and service records are retained separately. In **Maintenance**, check or change the replacement interval and distance since replacement. Reset or adjust these to match actual maintenance. This does not change the vehicle's original odometer.

Oil remaining changes to **yellow at 30% → red at 15% → flashing red at 5%**. This ride's maximum and average speed markers appear once their values exceed 5 km/h.

Use **UP/DOWN** on HOME to cycle **manual one-line → AUTO one-line → AUTO two-line → speed → clock → map**. The final view shows full TBT during guidance and the existing large compass otherwise. **Hold O** to change the lower information item. Manual keeps the chosen item; AUTO resumes its existing notification/music priority after a 20-second manual override. View and information selections stay in RAM while riding and are saved **when the key switches OFF**. An abrupt power loss can lose selections made since the last key-OFF checkpoint.

Choose **Settings → Display → Clock style → Analog / Digital**. Only the selected clock appears in the HOME carousel. Digital hides the small top clock. Photo shading, hand lengths, speed arc, ODO and the lower information position are preserved.

The lower cycle includes **sunrise / sunset**. Choose a nearby city under **Settings → Display → Sunrise / sunset** on Noodoe, or **Device settings → Sunrise / sunset reference city** on the phone. Noodoe calculates approximate times from the date, device UTC offset and selected city, rounded to minutes. Live GPS does not change this reference. Change the city and UTC offset when travelling, including daylight-saving changes. An existing stored location remains available as **Previously saved position**. New installations without a saved position must select a city; missing date/location shows `--:-- / --:--`. Terrain, buildings, elevation and weather are not considered.

| Analogue clock | Digital clock |
|---|---|
| <img src="images/guide-0.15.4/analog.png" alt="Analogue clock with automatic lower information" width="280"> | <img src="images/guide-0.15.4/digital.png" alt="Digital clock with small seconds" width="280"> |

With the key OFF, the last ODO received during this boot stays on the device and phone dashboard. Turning the key ON requires a fresh vehicle reading. A restart clears this temporary ODO; it never restores an old saved reading. Trips and maintenance counters are preserved separately.

In 0.12.1, the last ODO also stays visible in **10-minute use after key OFF**. A brief receive error no longer clears a valid reading immediately. If communication remains lost with the key ON, the value becomes unknown. Trips and maintenance records are kept; the retained ODO never adds trip distance.

**SINCE REFUEL:** with fuel recognition ON, a fuel level at least two bars above the earlier level for three seconds starts a new record. A 2-bar tank refilled to alternating 4/5-bar readings is accepted. Missing or unknown readings wait for valid data. TRIP A/B and maintenance history are kept.

After refuelling, turn the key ON and allow several normal fuel readings to arrive. A rise of at least two bars is recorded once it stays stable; a missing reading does not count as a change. Ride Summary uses this boot’s last received ODO for the oil percentage after key OFF.

Set **Settings → Vehicle → Fuel recognition** to OFF if your dashboard does not provide usable fuel data. This stops fuel warnings, automatic refuel detection and reserve mode. **Hold O on SINCE REFUEL** after refuelling to start the counter manually from zero, including when it shows `--`. Other trips and maintenance records are kept. Turning recognition on establishes a new baseline; it does not infer a refuel from the pre-OFF level.

When reporting a fuel issue, photograph **diagnostic settings → Fuel RAW P2 / P3** and **UART RX first 16 bytes**. These show the values received from the vehicle dashboard. They remain available with fuel recognition turned off.

<a id="media"></a>
## 13. Music, calls and notifications

- **Music:** start playback on your phone and allow notification access. On the music page, short UP plays/pauses, short DOWN skips forward, and long UP goes to the previous track.
- **Volume:** hold O on the music page to open the volume popup. UP raises media volume by one step, DOWN lowers it, and a short O closes the popup. Calls and warnings take priority. The phone must be connected.

![Dedicated music volume popup](images/guide-0.12.1/music-volume.png)


- **Phone page:** UP opens recent calls; DOWN opens the latest notification. Follow the hints in detailed screens. Closing a call popup and ending the actual call are different actions.
- **New notifications:** the top icon flashes for about two seconds. Rapid messages update to the latest content; Updating... may appear while it is prepared.
- **Quick replies:** slots 1–7 are saved text; 8 shares location, this ride and music; 9 shares location; 10 shares music. Slots 8–10 include a signature. Check the recipient and content before sending.
- **Dashboard blink alerts:** warnings and notifications briefly change the instrument cluster brightness. Disable **Settings → Display → Dash blink alerts** if unwanted.

<a id="map"></a>
## 14. Maps and zoom

Import your region's **.map file** in the app's map menu. Enable location permissions and start the riding connection. Without a map file, Noodoe shows a moving grid and track line.

| On the map page | Action |
|---|---|
| **Long UP** | Switch AUTO / MANUAL |
| **UP/DOWN in AUTO** | Adjust preference from −2 closer to +2 wider |
| **UP/DOWN in MANUAL** | Change zoom directly |
| **Long O** | Switch north-up / heading-up / compass |

New areas take some time to prepare. With guidance OFF, the map shows position and travelled track.

No destination setup is needed for ordinary map viewing. Trip counters and ODO remain available.

![GPS](images/guide-0.13.2/map-north.png)

### HOME, map and music during guidance

The app has four bottom tabs: **Device / Home / Custom / Navigate**. This build uses **BRouter only** for offline guidance. The phone calculates routes; Noodoe shows TBT and the route on the map.

Open **Navigate → BRouter routing files → Download**, obtain official `.rd5` files, then use **Import .rd5 files**. Seoul uses `E125_N35.rd5`; obtain neighboring grids as needed. Download the separate Mapsforge `.map` display map through the existing map screen. Long-press the map or choose a favorite/recent destination to start. Address/POI search and Valhalla country-pack installation are not included in this build.

Existing downloaded maps, country-pack files and saved destinations are kept. Starting a saved destination explicitly calculates a new BRouter route. The road-name line displays **BRouter mode**, with roundabout exit numbers retained. Google Maps/OsmAnd external TBT still uses the road name supplied by that app.

MAP and the HOME map show a **purple route with black and white outlines**, remaining destination distance, and the current position above the route. Destination distance also joins the lower information cycle. Stopping guidance removes these items and restores the existing map/track behavior.

Manual/AUTO/dual/map HOME and Music show a compact pill: **arrow on the left, distance on the right**. Speed, clock and full TBT do not show a second mini panel. Full TBT has a large left arrow and right distance/BRouter mode label (road name for external TBT), without a separate background box. Inactive guidance shows no panel. Starting guidance does not automatically change the current page.

**Round native turn distances up to 100 m** in Navigate is OFF by default. It rounds native metric turn distances of at least 100 m upward to the next 100 m. Distances below 100 m, destination remaining distance, mi/ft and Google Maps/OsmAnd external TBT are unchanged. External TBT remains experimental and retains its existing priority. Stale GPS/disconnection shows `--` and dims the last route.

During guidance, menu icons fade after 5 seconds on every riding page, then the mini panel appears. Menu input reverses this transition. Without guidance the existing menu policy stays unchanged. At an estimated 7 seconds or less to the turn, based on the last 5 seconds of vehicle speed, the mini outline pulses at 2 Hz and full TBT text alternates yellow. Missing/stale speed or guidance disables this highlight.

| Mini guidance | Full TBT | Seoul route example |
|---|---|---|
| ![Mini guidance](images/home-navigation/02-manual-mini.png) | ![TBT](images/home-navigation/01-full-tbt.png) | ![Seoul route](images/home-navigation/06-map-route.png) |

The large TBT distance uses fixed, left-aligned `000 m / 00.0 km / 000 km` fields. Leading zeroes are omitted; the digits and unit keep their own fixed positions.

These are PC renders of the actual LVGL/EVE code, not photographs of a vehicle LCD.

The app **Wallpaper** theme uses a transparent window over the Android system wallpaper. Background shade and vignette are adjustable within the minimum contrast bounds for readable text.

### Change trail colors

Open **Settings → Display → Trail color** on Noodoe, or **All device settings → Trail color** in the app. Choose red, orange, yellow, green, cyan, blue, white, or **By speed**. The default remains red. Changes apply without restarting.

By speed remembers the dashboard speed recorded on each leg. Low speeds start blue, then pass through cyan, green, yellow and orange to red. **Settings → Vehicle → Ring maximum** sets the red endpoint. With a 200 km/h maximum, 100 km/h is green, 150 km/h is yellow, and 200 km/h or more is red. Changing units to mph does not change the color for the same physical speed. Legs with no valid speed are gray.

The color setting is saved; the trail remains a record of the current session as before.

![Speed-colored trail](images/guide-0.13.2/trail-speed.png)

![Fixed blue trail](images/guide-0.13.2/trail-fixed-blue.png)

<a id="dashboard"></a>
## 15. The app's Home, Customise and Device pages

<img src="images/guide-0.11.16/en-maintenance.png" alt="App dashboard: speed, ODO, trips and three maintenance rings" width="330">

Swipe sideways or use the bottom buttons for **Device · Home · Customise**.

- **Home:** map, speed, ODO, trips and three maintenance rings showing oil/belt kilometres remaining and service percentage remaining. **Tap the trip readout itself** to cycle information. With stock Noodoe, the lower area shows stock information instead.
- **Customise:** background pictures, notification apps, popups, quick replies and favourite callers.
- **Device:** installation, updates, current status, system language and the manual.

**UI test mode** lets you preview chosen speed, ODO and CFW/stock screens while disconnected. It does not change actual vehicle values. Turn test mode off before returning to a real connection.

**Bluetooth is off:** connection buttons stop and HOME asks you to enable Bluetooth. Open Bluetooth settings to turn it on. Automatic riding connection follows your existing setting; a manually paused connection stays paused. Installation does not start again by itself.

<img src="images/guide-0.11.16/en-bluetooth-off.png" alt="Bluetooth OFF" width="330">

A fresh installation puts a black 480 × 480 picture with the centered NEWDOE wordmark in Background 1. Replace it with any photo to remove the wordmark completely. Keep-data reinstalls and normal updates retain your existing photos; Backgrounds 2 and 3 are unchanged.

<a id="parking"></a>
## 16. Ignition-off and long parking

After key-off, **Ride Summary** appears before your chosen parking screen and sleep sequence. Holding O while turning the key off, then releasing it, allows up to ten minutes of ignition-off use.

Open **Settings → System → Power sequence → Long parking power off** and choose **Off / 2 days / 3 days / 7 days**. Two days is the default; an earlier user selection remains after updates.

After the chosen time shuts the unit down, phone-approach reconnection and welcome lights also stop. Turning the key on starts it from the startup screen again. Check this setting before extended parking. If the battery is already discharged or its voltage has dropped sharply, have the battery and vehicle checked first.

### Choose NewDoe or stock photos

On Noodoe, open **Settings → Display → Photo source**. On the phone, start the riding connection and open **Device settings → Photo source**.

1. Choose **NewDoe** for your three editable CFW slots, or **KYMCO** for photographs already in the stock album.
2. Open **Photo** and select **1, 2 or 3**, then Apply. The number identifies the slot in the selected source. **Key OFF photo** uses the same source.
3. The selection is saved on Noodoe. Both the phone and device settings change the same saved values.

![Select the photo source](images/guide-0.12.2/photo-source.png)
![Select a photo slot](images/guide-0.12.2/photo-slot.png)

NewDoe reads the stock album without replacing, deleting or repairing it. Uploading a new photo always changes a **NewDoe** slot, even when KYMCO is selected. Switch back to NewDoe to use that upload. A fresh install still puts the NEWDOE wordmark in NewDoe slot 1; updates and keep-data reinstalls retain your existing slots.

**Empty** means the selected stock slot has no JPEG. **Unreadable** means the file or folder cannot be read, the JPEG is damaged, or its format/size is unsupported. The slot number is shown in the error. That failed slot selection is not saved; choose another slot or switch back to NewDoe. No stock file is changed. If the photos are still loading, wait briefly and try selecting again.

![A slot-specific photo error](images/guide-0.12.2/photo-error.png)

If first installation stopped with **0x00050005 / CFWPIC.DAT** after photo transfer, use **Check current device** first. The included firmware package 0.12.2 fixes this issue. If it repeats in an older Bootstrap, inspect the result, return to stock, then reinstall with the included files. Do not erase the stock album or clear app data.

<a id="report"></a>
## 17. Still stuck? Send this information

Open **Having trouble? / Recovery and diagnostics → Save and share diagnostics**. Include the following in a [project support request](https://github.com/SerialSniffyHeck3r/noodoe_cfw/issues):

- Vehicle model/year, phone model and Android version.
- App version, device firmware version and the prepared firmware version shown in the app. Include the ZIP filename if you selected one manually.
- A photo of Noodoe's current screen and the exact error/code.
- Your last button press and what happened afterward.
- For languages: device language, English-only setting and resource status.

Do not clear app data before saving diagnostics. Keep another copy of any backups you already have.

[Back to top](#top) · [Downloads](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/latest) · [한국어](Guide.md)


## Offline navigation — BRouter

This build includes BRouter only. The Valhalla native engine, country-pack installer and vehicle-class selector are excluded. Existing downloaded data is kept.

1. Open **BRouter routing files → Download** for the [official routing files](https://brouter.de/brouter/segments4/). Each file covers a 5° × 5° grid. Seoul uses `E125_N35.rd5`; obtain neighboring files for routes crossing grids. Keep original filenames and preferably use data from the same date.
2. Use **Import .rd5 files**. A verified new file replaces the same grid; failure keeps its previous version. Select files again after interruption.
3. Obtain the separate Mapsforge `.map` through the existing map screen. `.rd5` files contain neither the display map nor an address-search database.
4. Long-press the map or select a favorite/recent destination. Connect to Noodoe and receive a GPS location to start guidance.
5. Noodoe shows turn arrows, next-turn and remaining destination distances, and a purple route. The road-name line says **BRouter mode**; roundabout exit numbers remain. The phone recalculates after an off-route event.
6. Resume never starts automatically. Confirm the saved destination and start to calculate a new BRouter route. Existing Google Maps/OsmAnd external TBT priority and road names are preserved.

Guidance requires Android 8.0+. The basic motorcycle profile uses OSM access, one-way and turn restrictions. Country/displacement special rules have not been separately qualified; follow road signs. Voice, cameras and lane guidance are not included. Device Bluetooth, NDCP, Gate and NOR structures are unchanged.

### First BRouter setup — map and routing files are separate

1. With internet access, open **Navigation → BRouter routing files → Download**. Get the needed `.rd5` files from the linked [official BRouter distribution](https://brouter.de/brouter/segments4/). No NewDoe country ZIP or API key is required. Short routes in Seoul use `E125_N35.rd5`; routes crossing tile boundaries also need neighboring files.
2. Keep the original names. Choose **Import .rd5 files**, select the downloaded files, review names/sizes and confirm. Wait for the import/verification result. Leaving files in Downloads alone does not register them: navigation uses the verified app-private copies.
3. Prepare the **display map** separately in the existing map screen using the region’s Mapsforge `.map`. A `.map` draws roads, while `.rd5` calculates routes; neither replaces the other. [Official BRouter offline guide](https://brouter.de/brouter/offline.html)
4. While stopped, open **Navigation → Choose destination on map**, long-press a point near a road, review the location and choose **Start guidance**. Favorites and recent destinations also work. Address/POI search is not included in this build.
5. The phone calculates after obtaining a real GPS fix. For a first short test, choose road-connected points within one `.rd5` tile. Imported data works without internet, but GPS reception and the dashboard Bluetooth link are still needed.
6. **MAP/HOME map** shows the purple route with black/white outlines. **HOME TBT** shows an arrow on the left, distance to the next turn on the right, and **BRouter mode** below. Destination distance is a separate destination-icon indicator and HOME-strip item. Starting guidance does not force a page change.
7. Mini TBT appears on HOME information/map and Music. During guidance, the upper menu hides after five seconds and mini TBT appears. Speed, clock and full TBT views do not receive a duplicate mini display. The phone recalculates after departure from the route; stale location/connection is not presented as fresh guidance.
8. Use **Stop guidance** to end. After app restart, use **Resume guidance**, confirm the saved destination and start again. A destination saved for another vehicle is not silently applied. Stop guidance before importing updated `.rd5` files. Failed imports preserve the prior file; check partial-success results before selecting missing files again.

**Google Maps / OsmAnd external TBT (experimental):** Enable external TBT in Navigation and grant NewDoe **Notification access**. Start actual guidance in the supported app and verify that its navigation notification appears. Simply opening the app or searching a destination is not guidance. Recognized external TBT takes priority over BRouter; internal guidance cannot start while external TBT owns navigation. It can show the source app’s road name, but does not import its complete route line. Parsing depends on app version, language and notification format. If the source app does not offer guidance in a region, NewDoe cannot supply it on its behalf.

| Problem | Action |
|---|---|
| Missing routing file/tile | Import tiles covering the start, destination and intervening route. |
| Map works, routing does not | Check `.map` and `.rd5` separately. |
| Waiting for GPS | Check phone location permissions/service and wait for a real fix. |
| No route | Pick a point closer to a connected road and check tile coverage. |
| External TBT waiting | Check actual Google Maps/OsmAnd guidance notifications and NewDoe notification access. |
| Dashboard waiting/disconnected | Reconnect using the existing connection flow and check phone guidance. Do not automatically reset or re-pair. |

The basic motorcycle profile uses OSM access, one-way and turn restrictions. Country/displacement-specific rules are not comprehensively validated; follow road signs. Voice, speed-camera, lane guidance and Valhalla country packs are not part of this build.


### First-boot presentation, installation buttons and clocks

A pending initial-setup marker opens the setup welcome directly after boot, without the normal key-ON arc sweep. Existing users without that marker keep normal startup. The wizard stays available while moving and its stop instruction waits for O. Ordinary settings retain their movement lock.

Each of the five installation cards enables **Next** after three seconds; none advances automatically. On the CFW UPDATE confirmation, **I can see CFW UPDATE** is immediately available; **Not yet** becomes available after ten seconds and is not approval. Once CFW installation has started, key OFF is permitted; keep the key ON during the first installer transfer from stock. This never means disconnecting the battery/main power, and the existing legacy stock-return interruption limitation remains.

Both analog and digital clocks dim the photo behind them, while hands/digits, speed arc, ODO and the information strip retain their colors. With key OFF, the digital speed field shows `--- km/h` or `--- mph` instead of retaining the last speed. Weather no longer shows time-of-day triangles beside temperature.

| Welcome | O 확인 / Confirm with O | 키 OFF / Key OFF |
| --- | --- | --- |
| <img src="images/guide-0.15.4/setup-first-frame.png" alt="WELCOME NEWDOE" width="240"> | <img src="images/guide-0.15.4/setup-stop-confirm.png" alt="Stop instruction waiting for O" width="240"> | <img src="images/guide-0.15.4/speed-key-off.png" alt="Unknown speed dashes with key OFF" width="240"> |

