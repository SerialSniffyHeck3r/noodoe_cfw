# Rider name — 2026-09-18

Implemented optional name for two-line `Welcome` / `Rider <name>`. Without a
name, the second line is `Rider`. Existing cold/dark-panel wake timing, centered
32px font and positions are retained; names use a measured288px ellipsis copy.
Prior-page resume and Summary24px upward changes remain in this candidate.

App ownership: `App_Logic/Settings/rider_name.c` provides a bounded task-context
RAM setter/getter with strict UTF-8 validation (48bytes). Graphics owns the
rendering copy. Current ASCII-only font renders unsupported scalars as `?`,
without changing stored UTF-8. No language pack or phone SPP command added.

Optional durable names use the existing SettingsService request/StorageTask
journal and verified readback. Missing provisioning is rejected; no automatic
format. Schema3/312bytes retains prior fields and reads v1/v2 with empty name.
Failed save keeps old committed data. Old firmware cannot read the new record;
the prior NVM backup is required for a downgrade after an actual v3 commit.
Full API, task, lifetime and persistence contract: App_Logic/Settings/RIDER_NAME.md.

## Verification
- Actual ARM settings worker/codec tests O0/Os: 778 checks each,
  including copied packet lifetime, busy/invalid input, failed commit, reboot,
  maximum length, UTF-8 boundaries, previous versions and CRC-valid bad records.
- Actual ARM power coordinator O0/Os:1152 checks each, including name delivery
  to Welcome and retained IGN/standby behavior.
- Welcome production formatting/draw code with real font advances O0/Os:
  61 checks each (empty, normal, percent, Korean fallback,1..48 wide names,
  ellipsis, fixed coordinates, unchanged Summary positions). Mock LVGL, no
  claim of physical display verification.
- CubeIDE Product Release and Debug pass. Release APP373868B,
  free84884B; Debug APP389312B,
  free69440B. SRAM free Release/Debug:
  35272/35280B; CCM free16320B.
  FreeRTOS48KiB/LVGL48KiB and existing memory budgets unchanged.
- Regeneration-shaped isolated metadata fixture passes Product/Integrated/
  Graphics source policy and idempotence, with fixture Core/IOC unchanged.
  Actual GUI regeneration was not performed. No Core/IOC/vendor code edits.
- Existing RWX LOAD segment linker warning remains; no new compiler warnings.

## Installation
NOT INSTALLED. No device access or writes this turn while board-side SWD/GND
confirmation is pending after the prior core-ID failures. Current device APP
remains off-stages `c4a69c5376a9c58eca8b399d1a4de4ab64b4e2ad96f2cd09a5cf1593f41a0b60`.
Candidate APP SHA256: `4b1105259a7598e034656adf4a27ff55653e17efbe1d96443b662e2d0555e150`. Safe APP-only installer is prepared with the
previous-image/donor/stock-BL checks and full readback. It has not been run.
No real name was invented or persisted; no NVM migration has occurred on board.
