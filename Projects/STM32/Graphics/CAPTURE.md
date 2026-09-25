# Actual EVE screenshot capture

`BSP_Display_CaptureRequest()` schedules a request; `EveRenderReady()` calls `BSP_Display_CaptureProcess()` only after the graphics owner's command burst and swap complete. No SPI call comes from SWD or another task. Idle handling only reads mailbox words. Capture uses FT81x `CMD_SNAPSHOT2`, RGB565, x/y=0, width/height=480, destination `0x00080000`. It snapshots EVE's current raster into460800bytes of RAM_G. The command briefly suspends EVE display output. The resulting bitmap is not a photograph of panel glass or backlight.

The immutable LVGL allocator hardcodes1MiB. Project linker flag `--wrap=lv_draw_eve_ramg_get_addr` routes both cross-translation-unit callers (glyph/image upload) through `graphics_eve_ramg_guard.c`. The wrapper limits new allocations to the lower512KiB, preserves hash-table cache hits, and lets the original allocator manage its table unchanged. Snapshot occupies `0x80000..0xF07FF`; upper remainder stays unavailable to assets too. `Graphics_GetAssetBytesFree()` reports the512KiB asset budget. Capture is available in both build profiles. EVE reinitialization invalidates old snapshots.

SWD mailbox `g_bsp_capture` is4176bytes: twenty little-endian u32 header fields followed by4096data bytes. Header order is `magic, version, request_seq, command, offset, length, expected_generation, response_seq, status, generation, width, height, format, total_bytes, payload_length, payload_crc32, duration_ms, eve_frames, requests, failures`. Magic=`0x43415031`, version1. One requester writes command/offset/length/expected_generation, then publishes a new nonzero request_seq last. Firmware completes payload/header/CRC, then publishes response_seq last. Command1 captures; command2 reads a bounded chunk from the current matching generation; command3 releases. Nonzero status is a `BSP_Display_Status` error. Chunk CRC is standard zlib-compatible CRC32.

`tools/capture_display.py --manifest <current ELF manifest> --output <new folder> --frequency-khz 400 --execute` checks the running APP SHA against the symbol manifest before writing only mailbox request words. HOTPLUG reads and mailbox writes keep the CPU running; no halt/reset/watchdog-freeze is sent. The root task serializes capture against OTA/internal-flash activity. The immutable-until-next-request response and final sequence store make live chunk reads safe; every sequence, generation, offset, length and CRC must match. Capture-local50/100/400/950kHz support does not change the central flash tool's write-clock restrictions. Output contains raw `.rgb565`, RGB8 PNG, transfer logs and SHA256 manifest. Decoding does not resize/crop/mask/reconstruct the scene. Screenshots can show test results and rendering faults; they cannot prove external button actuation, real vehicle input or phone interoperability.

`tools/tests/display_capture_host/run.py` executes actual capture C plus unchanged LVGL allocator with real GNU wrapping under ARM Unicorn at O0/Os. Tests cover snapshot command encoding,512KiB allocation bound and cache hits, byte transport/CRC, stale generations, chunk bounds, release and bounded timeout. Hardware rendering and physical display output still require root's actual capture.

Source: [Bridgetek FT81x Programmer Guide v1.2, §5.64](https://brtchip.com/wp-content/uploads/Support/Documentation/Programming_Guides/ICs/EVE/FT81X_Series_Programmer_Guide.pdf).

## Selecting the diagnostic results page

The Integrated profile overlays eighteen runtime observations in three pages
on the animated arc. Pages rotate every6seconds. FPS and CPU remain visible;
PASS, FAIL and WAIT reflect the named operation, not completion of an entire
bring-up. For example, NOR READ PASS does not certify erase/program, and RAM
BASIC PASS does not claim an exhaustive64MiB memory test.

`GraphicsBringupHUD_SelectPage(0)` selects automatic rotation; values1..3 pin
one page. The public API only publishes a request; the graphics task owns all
LVGL updates. The host capture tool accepts `--page 1`, `--page 2` or
`--page 3`. It verifies the running APP and the page symbol before requesting
the page, then restores the previous selection immediately after the snapshot,
before the slower pixel export. An aborted snapshot also attempts restoration
while the target remains runnable. Do not run capture and NOR backup or flash
programming concurrently against the same ST-LINK.

Verified device images and raw capture manifests are indexed in
[`analysis/2026-09-12-integrated-bringup`](../../../analysis/2026-09-12-integrated-bringup/README.md).
