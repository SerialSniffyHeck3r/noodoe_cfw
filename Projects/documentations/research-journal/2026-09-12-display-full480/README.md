# Full480 circular viewport and border arc — 2026-09-12

The later [UART CFW](../2026-09-12-uart-bringup/README.md) retains this viewport
and perimeter arc and is now installed. The image/readback/capture in this
folder remain evidence for the earlier full480-only change. The newer EVE
capture additionally shows the permanent UART status and parsed vehicle row.

The user requested all480 pixels at the center of the circular display and a
larger perimeter speed arc. The historical JPEG-derived mask had radius239.5;
the test arc independently had radius220 (diameter440). These were separate
limits, so both have been changed.

## Coordinate and drawing contract

- Raster:480×480, valid indices0..479. Active circle: center(239.5,239.5),
  radius240. Central rows239/240 and columns239/240 now include all480 pixels.
  Corners remain excluded. This is the user's revised coordinate contract,
  not a new physical measurement of the panel or JPEG.
- `GraphicsArcSweep_InitViewport(parent)` creates the full480×480, zero-padding
  perimeter arc, radius240, with the existing18px inward stroke, rounded caps,
  270-degree sweep and4s fill/4s drain timing.
- The existing generic `GraphicsArcSweep_Init` and complete-shape/text
  containment checks remain strict. LVGL's integer center(240,240) and the
  half-pixel mask center are reconciled by the final EVE clipping operation.
- The EVE final mask remains30 words/120bytes, with radius command3840 and
  center3832/3832 in1/16px units. No framebuffer or mask bitmap was added.
  External UI content is excluded; the480×480 hardware scan and clear remain.
- Main's strong `StartDefaultTask` still contains only `LCDTest();` in USER
  CODE. Changes are in project-owned Graphics files, outside Cube-generated
  Core and vendor code. No IOC regeneration was needed or performed this turn.

## Builds and source tests

Both actual STM32CubeIDE1.18.1 builds passed with zero errors and the existing
RWX LOAD-segment warning. See `build-release.log` and `build-debug.log`.

| Configuration | APP bytes | Remaining APP bytes |
|---|---:|---:|
| Integrated Release |394276|64476|
| Integrated Debug |458156|596|

The installed candidate is archived in `image/`, including ELF, map, manifest
and BIN. Release SHA256:
`67e08698c5174678844ccedcd616ba6278148e11f9772799f6f366f27bae9305`.
Debug manifest is archived separately. Debug's small remaining capacity is a
build limitation; no extra optimization exception was added for this change.

Actual production-C host-test results are archived in `host-tests/`:

| Suite | Result |
|---|---|
| Geometry/performance |4033 assertions each at O0/Os|
| EVE command/state |223 assertions and14 cases each at O0/Os|
| Arc sweep, Graphics profile |1781 assertions each at O0/Os|
| Arc sweep, Integrated profile |1769 assertions each at O0/Os|

Both historical invalid-label mutations were rejected as expected. The JSON
records source hashes; an independent read-only review matched those hashes
to the files used by this build. Host tests do not emulate actual EVE raster
antialiasing or establish physical panel appearance.

## Hardware verification

APP-only installation passed in
[`../bringup-runs/2026-09-12-202707-574-Release`](../bringup-runs/2026-09-12-202707-574-Release/summary.json).
The previous full512KiB was independently read twice and both files matched
the original stock SHA256
`38207bed741a8fef9e6d2ed24b86c8cb8b376ba5e4c957163f5edd83fbeb8037`.
Only the new APP was programmed at0x08010000. APP readback was exact, lower64KiB
remained byte-identical, and reset passed through the stock bootloader. The
original debug-freeze setting was restored and execution resumed.

Over the measured9435ms graphics interval,283 rendered frames advanced:
**29.9947FPS**, with zero SPI failures, LVGL allocation failures or graphics
errors. `probe-01/diagnostics.json` then independently matched the whole live APP
and observed CPU running before/after: one1001ms window had30frames (displayed
29.9FPS), CPU59.5%, zero missed frame slots. CPU is the existing DWT/task-switch
non-idle estimate, not graphics-only CPU load or a worst-case bound.

`capture-01/display.png` is the complete actual EVE RGB565 raster, exported as
113 CRC-checked chunks/460800bytes. Its source APP matched the manifest; the
CPU stayed running throughout live mailbox access, and snapshot status was0
with zero failures and29ms duration. This is not a photograph of the panel.

`capture-01/raster-analysis.json` records both center rows239/240 reaching
x0 and479, both top-center pixels illuminated, and four black corners.
The displayed arc is visibly full-width. Its bottom90-degree gap is intentional,
so y479 need not contain arc pixels. Labels remain inside the active circle.
The capture displays30.5FPS/CPU60.0% from its own short HUD window; the longer
283-frame interval above is the better target-rate evidence.

The analytic pixel-center circle and EVE's antialiased raster edge are not
identical. There are647 nonblack pixel centers just outside mathematical radius
240, all within radius240.9285; no nonblack pixel center is beyond241. This is
the measured subpixel edge fringe, not evidence of an additional content area.
Do not claim every pixel center outside the analytic circle is exactly black.

After export, `probe-after-capture/diagnostics.json` again matched the exact APP,
observed the CPU running, and recorded30frames/1002ms (29.9 displayedFPS),
CPU60.6%, zero missed slots. No firmware or peripheral settings were altered
after the validated installation.

Capture raw SHA256:
`d509c8b399ade0cda28e06d9207293540eefe272856fb0532eba2aa59b221d7e`.
PNG SHA256:
`ff09548e8a985e78e3b3ed48e989cbd205b6a22a28fd5dea58df70f2ef0e4411`.

BT and ambient-sensor recovery are outside this graphics change and remain
unproven; their historical failures are not fixed. Current installed firmware
is this full480 CFW APP with the preserved original lower64KiB.
