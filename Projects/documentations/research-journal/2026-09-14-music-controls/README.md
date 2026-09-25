# Music controls, global input and adaptive artwork —2026-09-14

Implemented and installed Product Release at0x08010000. Final actual GPU
capture: [final/music/display.png](final/music/display.png). This is EVE raster
output, not a photograph of the physical glass/backlight.

- UP short play/pause, UP long previous, DOWN short next, ENTER long Phone1/2.
  ENTER short still advances the category. Existing physical pin mapping stays.
- Middleware ButtonEvents singly consumes BSP input and fans out to Graphics
  and ProductUI. Application policy remains in App_Logic. No duplicate bridge.
- DATA_DEBUG default remote fixtures keep separate player state for each phone.
  Live-source commands use session-bound NDCP0x10 and the existing serializer.
- Music has32px Lato Bold title,24px regular artist, progress/time, right-edge
  Material Round key/action masks with short dot and long capsule marks.
- Music uses album-driven dimming; current fixture chooses39% center. Top and
  bottom gradients preserve clock/ODO. User brightness API remains independent.

## Verification

- Debug458016bytes,736bytes APP reserve; Release410496bytes,48256bytes reserve.
  Both build successfully. Only pre-existing embedded ELF RWX linker warning.
- Release SHA256:5ffc3718410fe90bb580f744416eaba84d9b18c6b824e24d43d9d500fae28caf.
- APP readback matches; lower64KiB BL/config SHA256 remains
  f38ed839c4fbce74d4d8227fcf13764194f405c4860cda75c628fce66064b574.
- Two reset cycles: HAL/RTOS heartbeat progress; default-task stack remaining
 1740words in these samples. See[install/summary.json](install/summary.json).
- Real GPU Music samples29.8/29.9FPS, CPU69.0/70.4%. After eight-page sweep:
 29.9FPS/66.2%, no graphics/SPI errors, font/icon cache21069bytes, no cache reset.
 These are sampled non-idle estimates, not worst-case scheduling proof.
- Live brightness policy100→40 gives actual Music39→15%; photo upload count
 stays1. Settings restore to100 (effective39), preview canceled, actual Music
 remains selected. Eight pages visited without graphics error.
- ARM tests: Control/NDCP/media61conditions each O0/Os; global Graphics+app input
 77conditions each O0/O2; dashboard data0/1 and wallpaper tests atO0/Os pass.
 Wallpaper runs include145694conditions each, real streaming C, provider
 busy/error, FIFO retirement, memory bounds and dark/white brightness cases.
 Existing fixed trip baseline/typography tests also pass after font generation.

## UART incident and recovery

First capture succeeded with healthy rendering, but verification found speed
 and ODO absent. Prior senderPID40136 had exited after Windows denied replacing
 status.json while it was open. This was a PC diagnostic-file error, not an
 observed firmware receiver fault. First result retained at verification.json.

dashboard_uart.py now preserves the previous complete log and retries a failed
 diagnostic write on the next report instead of shutting down healthy UART.
 Two host tests cover sharing violation and full disk. Genuine serial failures
 remain failures. New hidden senderPID44568 keeps COM11/1152008N1,1→200km/h at
 100ms; logs/STOP sentinel are in[ uart-ramp ](uart-ramp). Final SWD sees valid
 speed and ODO36475. No target RAM speed substitution was used.

## Scope limits

Two-phone player behavior and command/session semantics passed ARM software
 tests. Bluetooth radio/media-service end-to-end remains unverified on this
 damaged donor; a companion must implement the new wire contract. Physical
 UP remains the previously diagnosed held/broken input; pin mapping was not
 changed to disguise it.32×32 placeholder art is GPU-scaled, not high-resolution
 phone album transport. No BL/options/NOR provisioning or permanent photo
 import was added. GUI was not used.

Public control contract:
 [MEDIA_CONTROL.md](../../STM32CubeProjects/FuckNudo_Noodoe_CFW_Project/App_Logic/Control/MEDIA_CONTROL.md).
Final runtime evidence:[final/verification.json](final/verification.json).
