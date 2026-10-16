# Companion 0.10.3 / CFW 0.10.2 — current language setup

On an existing 0.10.2 device, update the APK only. After firmware screen/boot confirmation the app explicitly asks whether to install languages, then downloads, verifies and transfers them only with consent. Later keeps English available. A healthy installed pack is reused.

Already completed firmware installation? Open **Device language · Install language pack** at the top, stop riding sync, keep/select the matching 0.10.2 firmware ZIP, and choose **Install / resume language resources**. You no longer need to locate a separate language ZIP. Manual import remains for offline use. Afterwards reconnect, disable English only if enabled, select Device language / Follow app, and confirm the stationary restart.

0.10.x keeps normal CFW-to-CFW updates with the matching Gate. 0.9.x requires the existing keep-data stock return and reinstall. Firmware, Gate and Bootstrap bytes are unchanged in this app release.

---

# 0.10.1 · Device language and text rendering

[한국어](35-Languages.md) · [User guide](README.en.md)

**0.10.1 beta · 16 October 2026.** Software validation passes, with the legacy transition limitation below. This is not physical power-loss or RF qualification.

**Keep permanent power connected:** a power interruption while older CFW records its keep-data stock-return request can stop boot before the new recovery code runs. Automatic recovery is not guaranteed in that legacy interval. Keep permanent power connected throughout stock return/reinstallation and do not operate the starter. IGN OFF is not removal of permanent power. If already stuck, do not assume another reset will recover it; retain diagnostics and ask for help.

## Supported devices and languages

On F4 Noodoe with **stock language pack B**, device menus support English, German, Spanish, French, Italian, Greek, Korean, Vietnamese and Japanese. English retains its existing wording, Lato fonts and layout. Japanese uses the new language-pack font.

Android supports those nine languages plus Simplified and Traditional Chinese. With Follow app language selected, either Chinese app language falls back to English on the device. Chinese glyphs are available for music and notifications, but Chinese is not a B system-menu language.

Pack A is outside this release’s multilingual scope. It keeps the existing English UI and does not read B stock-font paths. Vehicle model names do not determine the language pack.

First installation accepts **stock 5.14 or 5.16** without a HW-number, bootloader-version, model-name or PCBA whitelist. Actual device identity, storage checks and stationary conditions still apply. This does not claim physical qualification of every vehicle or hardware revision.

## Installing this version

1. Use the APK and firmware ZIP from the same release, and keep app data.
2. If an existing CFW has an old Gate and the app requests stock recovery, follow **keep-data stock recovery → Check current device to confirm stock → send Bootstrap from the selected 0.10.2 firmware ZIP → Restore stored data → install CFW**. A normal Product upload does not overwrite Gate.
3. Confirm that the new CFW actually runs, and finish the app’s firmware confirmation first. A 100% transfer or the NOODOE INSTALLER screen is not completion.
4. After confirmed firmware startup, answer **Install the language pack?**. Install downloads and verifies the matching resources automatically; Later keeps English available. Manual language-ZIP import is only an offline alternative.

Firmware installation and language readiness are separate steps. After an interrupted language transfer, reconnect to the same device with the same language ZIP and resume resources only. A confirmed firmware installation does not need to be repeated or returned to stock. Later CFW updates using the matching new Gate use the ordinary update path.

## Checking stock fonts

Healthy devices do not show a repair choice. Only a confirmed missing, truncated or structurally invalid stock file needed by this feature prompts:

- **Repair fonts and continue:** restore only the affected files to their original FONT paths from the verified B backup.
- **Continue in English:** leave FONT unchanged and use the existing English UI and phone rendering.

A different hash or an unsupported character alone does not justify repair. Insufficient space or damaged filesystem structure does not authorize deleting other files. Choosing English does not bypass the installer’s essential storage checks.

## Changing language

Choose Follow app language or a language under **Device language** in the app. The device settings offer the same preference. Reconnecting does not replace a manual choice with the app language.

A change to the effective system language prompts for a restart on Noodoe. Confirm while stopped. **Later** saves your choice for the next restart. Restart waits for settings and ride data to be saved; movement, a warning or a power transition cancels an accepted restart while storage is draining.

Each boot prepares one system language. Menus do not switch languages during animation or read font files from external flash on every frame.

## Music and notification text

Under **Music and notification text**, choose either mode. The app and device edit the same saved preference.

- **Render on phone — default:** uses the characters and colour emoji supported by the phone. Preparing and transferring images may add delay.
- **Use device fonts:** uses stock glyphs and the language pack to reduce transferred data. Some characters and sequences are unsupported and appear as □. New monochrome emoji are available in this mode.

This covers the music page, HOME, notification details and notification popups. Calls and quick-reply rendering/sending keep their existing paths. Supported skin-tone, flag, family and other emoji sequences are handled as complete glyphs; an unsupported sequence produces one □.

## Errors and recovery

**English only** uses the existing English UI and phone rendering while retaining the saved language preference. If you enable it manually, repaired resources do not disable it automatically.

Resource errors, insufficient space or failed language-memory allocation also fall back to English. Missing translations use English wording; individual unsupported characters use □. A language-resource failure is not a firmware-installation failure.

If installation requests a manual restart, hold **UP + O together for 3 seconds**, then release them. Reconnect with the same ZIP and inspect the current device and installation result. Stage 6/4 alone proves neither successful installation nor a bricked device.

If Bootstrap cannot reconnect automatically, remove the old pairing in the phone’s Bluetooth settings and Bootstrap’s local pairing menu, pair again, then use **Check current device → Continue from here**. Keep app data and installation records.

## Font licensing

Monochrome emoji come from hash-pinned **Noto Emoji 3.002 Regular 400**. The language distribution and app licence list include its copyright and OFL 1.1 notices. Existing system icons are unchanged.
