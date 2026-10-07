# 0.9.62 validation

The change adds default-ON, persisted dashboard UART alerts. Settings field0x100a
is appended without renumbering earlier settings. Existing files missing the
field use ON. Bootstrap reads the same verified journal; no extra NOR writer.

Software evidence:
- Actual ARM DashService/Protocol:180 assertions each at O0 and Os. Checks normal
  ambient/override behavior, A1 bytes,1000/500ms boundaries,4/8-step duration,
  repeated events, priority, OFF/ON, expiry, absent link and tick wrap.
- Actual settings/UI:80,385 assertions each O0/Os including ON default, bounded
  ON/OFF requests, labels, reset defaults and existing motion/navigation checks.
  An old test fixture expecting index9 to brighten the LCD was corrected to the
  existing descending stock scale (index0 daylight); runtime mapping unchanged.
- Actual Product input/popup paths:10,000 scenarios,240,023 assertions. Accepted
  fuel/maintenance/call/notification events, duplicates, quiet/blocked arrivals,
  toast exclusion, updater slow/error/normal and disabled-setting transitions.
- Bootstrap committed CFG/bond test:27 power-cut/corruption/preference cases,
  roundtrip, unknown-field preservation and fragmented journal. OFF/ON/missing
  fields survive a simulated reboot and bond save; preference reads write no NOR.
- Actual LVGL/EVE:11 complete frames, including local setting ON and OFF. All
  display lists below8192 bytes. The screenshots are software simulations.
- UART BSP interrupt interleavings:88 assertions each O0/Os. Bootstrap UI,
  install lifetime, approval/cancellation and shared gesture regression passed.
- Android: assembleDebug passed; matching APK/Product hash and unchanged signer
  checked. Full unit/Robolectric validation did not complete: an initial run
  exposed an obsolete unchanged-Bootstrap assertion (corrected for the rebuilt
  image) and host disk exhaustion. The isolated rerun was stopped at the user's
  explicit request to minimize testing and publish promptly. The corrected test
  is not claimed as passed. Lint was not rerun. No real device backup was deleted.
- Rebuilt Product Debug/Release and Bootstrap. Debug free1024 bytes; Release
  free33188 bytes; Bootstrap free8828 bytes. Debug minimum1KiB, Release16KiB.
  Compiler flags, heap/stack/CCM reservations and font/icon assets unchanged.

To fit the small Debug build, common default initialization is shared, the
UART vehicle snapshot is published directly under the existing reader lock,
fixed frame header XOR is folded, and redundant install-state queries are
removed. The snapshot getter performs only a bounded RAM copy/freshness math.
No rendering delay, queue-size cut or asset compression was introduced.

Gate, resident stock bootloader, stock recovery, resources, Uninstall and
Diagnostic are unchanged; package comparison proves the unchanged payloads.
Bootstrap and Product are newly built. A normal Product update does not replace
Bootstrap. Old running firmware cannot acquire new blink behavior mid-update.

No physical device, ST-Link, optical brightness, radio, vehicle timing or LCD
FPS was measured. The software UART high/low endpoints follow stock index0/9;
the actual optical response of every connected dashboard remains unverified.
