# NewDoe 0.11.12 validation

- Android: 174 tests across 21 suites; no failures, errors or skips. Matching bundle, update confirmation, settings preservation, pairing/handoff/reconnect and manual resources checked.
- Actual ARM C code with synthetic MMIO/time: power admission 4066 assertions, parking 34, power UI 1183, watchdog 4277, dashboard UART 117, storage scheduler 40000 per O0/Os build. Bluetooth Product 34 scenarios and Bootstrap 6 scenarios pass.
- Storage scheduler used 1000 seeds per build. The earlier 10000-seed run exceeded the emulator instruction budget; it is not claimed as a pass. The final 1000-seed run includes in-progress journal draining and forced-snapshot parking freeze.
- The compact transition-event scratch produces byte-identical 64-byte records to the released logger, including reserved bytes, record order, queue rejection and retries: 1024 assertions in each O0/Os build.
- Debug flash free: 532 bytes; Release: 532 bytes. Minimum remains 512 bytes. SRAM used 153496; CCM used 49216. Compiler policy, reserved heap/stack and existing assets remain unchanged.
- 81 Gate/Bootstrap/Update/linker files are unchanged. Released Gate 0.11.4, Bootstrap 0.11.8 and language 0.11.2 are reused byte-for-byte. Matching APK/ZIP hash contract, existing APK signer and compiled-object relink validation pass.
- No firmware was flashed to a physical Noodoe and no phone/RF or current/endurance test was performed in this task. These are software checks, not measured battery-life results.
- A dark screen or successful GPIO request does not prove external power cutoff. Unsaved RAM changes may be lost after the 30-second snapshot grace; active physical writes remain protected. Old-code stock-return power-loss limitation remains accepted and unfixed.

[Installation and hardware verification notes](INSTALL.md) · [Parking guide](../../../Projects/Manuals/User/36-NewDoe-Recovery.en.md)
