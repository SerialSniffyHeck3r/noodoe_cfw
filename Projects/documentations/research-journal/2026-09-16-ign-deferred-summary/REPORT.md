# Corrected IGN OFF sequence

This implementation supersedes the prior immediate20% dimming and the older
ring-scanout-gated session end. The user's final order is authoritative:

1. OFF edge: begin a1000ms cancellable wait. Retain the prior frame, image,
   brightness, text, ring pose and RUN policy. Background hold makes no GPU
   writes and pauses image/opacity/brightness animation clocks. It does not
   block other tasks from observing the ignition input.
2. At the first tick reaching1000ms of uninterrupted OFF: commit SESSION_END
   once and freeze the ride snapshot. Rendering does not delay this decision.
3. Together at confirmation: restore the globally selected wallpaper, fade
   toward20% brightness over240ms, exit the ring over400ms, and raise/fade the
   summary over240ms. Summary's5000ms timer starts at confirmation.
4. ON before confirmation cancels the end and preserves the ride. ON after
   confirmation starts a new ride; old ring notifications cannot close it twice.

No font, layout, photo storage, bootloader, IOC or vendor file changes are needed.
The selected Kuromi wallpaper remains in its already verified WALL0.JPG file.

Validation artifacts:

- `power-ui-test.log`: actual Cortex-M4 power UI/state/session/scene code,
 31 assertions each at O0/Os, including999ms no render,1000ms commit while
 scanout is stalled, simultaneous ring/summary motion, and fast reversals.
- `product-test.log`: actual complete pure UI at O0/Os/Oz, including timer wrap,
 cancellation at100/500/999ms, exactly-once end and ignored legacy callbacks.
- `wallpaper-test.log`: App runtime + actual background renderer at
 O0/Os/Oz × DATA_DEBUG0/1, including zero shade/photo writes during hold,
 frozen blend/brightness clocks and gradual brightness only after release.
- `build-release.log`, `build-debug.log`: actual Cube builds and memory budgets.
- `install/result.json`: exact full APP comparison and lower64KiB preservation.
- `boot/result.json`: read-only runtime diagnostics after exact halted APP match.

Hardware deployment and runtime checks are reported only when those result
files say they passed. Host tests are not a physical IGN transition observation.

## Installed result

{
  "app": {
    "state": "candidate-installed",
    "app_sha256": "23a9b30b99071693f94a1813a724c61f60555e9229b1d262c8aaf37bad4e0913",
    "bootloader_preserved": true,
    "nor_written": false
  },
  "elf_sha256": "44b68bef1cb26a38ee1078f216c585f0ca5efc1b54ad2e4e8af93bf122262900",
  "source_artifact": "candidate.elf",
  "boot": "verified",
  "tests": {
    "product_ui": "PASS",
    "power_ui_host": "PASS",
    "wallpaper": "PASS"
  },
  "sequence": "OFF ->1000ms exact frame/brightness hold ->commit once + background fade + ring exit + summary rise",
  "physical_ign_retest": "not yet observed after this corrected build",
  "builds": [
    "Product Release",
    "Product Debug"
  ],
  "memory_free": {
    "release_flash": 89292,
    "debug_flash": 74172,
    "sram": 35912,
    "ccm": 16320
  },
  "fps": [
    29.8,
    29.9,
    30.7
  ],
  "cpu_percent": [
    57.2,
    57.3,
    57.5
  ]
}
