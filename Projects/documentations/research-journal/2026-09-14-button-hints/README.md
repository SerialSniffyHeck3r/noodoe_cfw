# Shared button hints and wallpaper exclusion — 2026-09-14

The installed Product Release extracts the Music button hints into the shared
`Graphics/UI/Button_Hints.h` renderer. Pages supply three bindings and an opaque
scope; `Middlewares/Noodoe/Input/ButtonFeedback` observes the existing global
ButtonEvents stream. App action handling remains separate.

- Physical key: blue while held, red at the existing 2001 ms long threshold.
- Release: key returns white; only the selected action icon and its dot/capsule
  remain blue/red for 1000 ms. A play/pause pulse retains the released action's
  symbol even when the current command changes. Duplicate events, boot-held
  buttons, context cancellation and timestamp wrap are handled.
- Dot 5 px, capsule 14×5 px, functional icon 25 px. Physical keys remain 24 px.
- Photo and shade sampling exclude the actual clock/ODO separator caps through
  an EVE stencil. The existing black clear is visible there. Foreground,
  separators and speed ring coordinates stay unchanged.

## Validation

- `build-debug-verified.log`: Debug PASS, APP 458304 bytes, reserve 448 bytes.
- `build-release.log`: Release PASS, APP 411552 bytes, reserve 47200 bytes.
  Both builds have zero errors and the existing single RWX linker warning.
- Release APP SHA-256:
  `1c870f6b16f4c4f2214b5dc2ce676137bfd7f3c66002def5e4dea45d4e4e2b8f`.
- `install/summary.json`: APP readback and two reset cycles PASS. Lower 64 KiB
  BL/config remains `f38ed839c4fbce74d4d8227fcf13764194f405c4860cda75c628fce66064b574`.
- `test-input.log`: actual ARM event/model code at O0/O2, 98 assertions each.
  Covers press/long/release, exact pulse expiry, duplicate suppression,
  boot-held exclusion, disabled/context changes and timestamp wrap.
- `test-wallpaper.log`: actual ARM wallpaper code at O0/Os, DATA_DEBUG 0/1,
  145694 assertions per configuration; clip command checks use the same
  separator coordinates as the foreground. These are code tests, not a GPU
  simulation.
- `blue/result.json`: eight page previews render without graphics/SPI errors;
  29.9 FPS window, CPU 70.8%. Actual snapshot shows blue physical UP symbol.
- `release/result.json`: actual snapshot shows red hold capsule/phone action
  with the physical ENTER symbol white; 30.5 FPS window, CPU 75.1% after capture.
  These single-window readings are approximately 30 FPS, not a long benchmark.
- Both captures contain zero non-black pixels in the sampled empty clock/ODO
  cap areas. Full images were also visually inspected for layout/clipping.
- `cleanup/result.json`: read-only audit checks presentation injection removed,
  preview cancelled, and COM11's 1→200 km/h UART producer still running.

## Captures and test limits

`blue/capture/display.png` and `release/capture/display.png` are actual EVE
RGB565 snapshots, not a photograph of the glass/backlight. Feedback presentation
state was temporarily injected through SWD and restored in `finally`; no
physical GPIO event or media command was injected. Event classification and
1000 ms expiry are covered separately by ARM tests. This does not establish
actual Bluetooth playback or repair the known boot-held UP input.

The generic API and page binding example are in
`../../STM32CubeProjects/FuckNudo_Noodoe_CFW_Project/Graphics/UI/BUTTON_HINTS.md`.
No IOC, Cube-generated Core, vendor source, option bytes or NOR data was
changed for this UI update. Heap/stack limits were retained. An unused pair of
page labels was removed to recover 136 bytes of static SRAM, and the build
profile restores the new source registrations and scoped Debug optimization.
