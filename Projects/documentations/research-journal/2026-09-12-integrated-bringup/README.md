# Integrated bring-up and remote display evidence

This directory preserves device observations, failed attempts and independent
readbacks. A successful build is not a device test. The latest **verified
installed** image is identified by the bringup run, not by a changing Release
directory. Later installation records below supersede earlier ones.

Follow-up restricted to BT and ambient, including corrected first UART order,
startup digital trace and four fixed address tests:
[BT/ALS focus results](../2026-09-12-bt-als-focus/README.md).

Earlier device-service work (Ambient/BT, stock comparison, explicit retry and
ID-only diagnostics): [DEVICE_SERVICES_RESULT.md](DEVICE_SERVICES_RESULT.md).
That record distinguishes completed software/API work from the still-unresolved
radio and light-sensor hardware bring-up.

## Display capture verified

Installed run: `../bringup-runs/2026-09-12-143330-998-Release`.
APP SHA256: `54c6643656ce5d48619135d5b1870a88fc6fa9c4975db0153e3da97c97bde8ca`.
APP readback and unchanged lower64KiB both passed. HAL/RTOS progress and graphics
frame progress passed. The prior failed HUD installation remains in
`../bringup-runs/2026-09-12-142050-746-Release`; its top label rectangle exceeded
the local circle guard. Width120 fixes it, and a real-C regression reproduces
the failure when width140 is restored.

- [Actual captured frame1](capture-01/display.png): drain phase and results3/3.
- [Actual captured frame2](capture-02/display.png): fill phase and results3/3.
- Each folder contains original460800-byte RGB565, PNG,113 checked chunk
  records, request/read logs and SHA256 manifest.
- Snapshot durations were24/25ms. DHCSR S_HALT was clear before/after snapshot
  and after export. No CPU halt/reset was sent by the capture tool.
- `capture-validation.json` checks raw hashes, image dimensions and that every
  pixel outside the fixed center(239.5,239.5)/radius239.5 circle is black.
- These are EVE raster captures, not camera photographs of panel/backlight.

## Measured status at this image

`probe-hud-fixed/diagnostics.json` is a live, non-atomic multi-structure read.

| Function | Observation | Limit |
|---|---|---|
| UI |29.8FPS,58.3% CPU in one1s window; zero missed slots|Captured HUD windows show29.8–29.9FPS,~62% CPU|
| NOR |C2201B,128MiB,42MHz SPI, independent transport checks passed|Full A/B backup and writes are separate tests|
| SDRAM |64MiB address geometry, walking/address probes,64KiB patterns and DMA passed|Not an exhaustive64MiB stress test|
| RTC |Calendar reads and time advancement passed|Stored2016 date is not current UTC; no set/alarm test yet|
| UART5 |Initialized1152008N1, no errors|No external dashboard frames connected|
| USB CDC |Initialized, not enumerated|No connected USB host observed|
| Bluetooth |Startup timeout0x302, no controller identity/RX|SPP peers/OTA cannot be claimed working|
| Ambient light |I2C HAL error at expected0x45|No valid sensor identity or lux|
| FS/NVM |No recognized new filesystem, empty settings|No automatic format or provisioning|

The display reports PASS/FAIL/WAIT from these runtime states, with six rows per
page and automatic6s rotation. It does not substitute host parser tests for
real radio or vehicle input. `LCDTest()` remains the sole strong main-task call.

## Work still requiring device validation

Runtime update/control, persistent settings and SWD NOR backup are being
integrated. Startup writes remain locked. No128MiB format, OTA staging/commit,
option-byte write or stock BL change has been performed by these integrated
bring-up attempts. BT controller response and ALS response remain unresolved.
The user is away, so physical USB/ELM/GPS/button tests cannot be supplied now.

## Read clock qualification

`swd-speed-4000` contains two independent whole366944-byte APP reads matching
the exact installed SHA above (2.167/2.201s tool read durations). This qualifies
that read path only. Flash programming stays50/100kHz and mailbox request
writes use their conservative separate clock. SDRAM reads additionally require
CRC against the device-produced immutable buffer; higher clock alone never
certifies data integrity.

## Subsequent integrated service installation

Run `../bringup-runs/2026-09-12-145522-906-Release` installed APP
`89b28979eef992a7d1dea8361755625e0ac41edc56e33f34673682c1981898f6`.
APP383400bytes; verified readback, unchanged lower64KiB and graphics/RTOS
progress passed. The exact ELF and files are in `integrated-final-image`.
This image adds the Control/Settings/RuntimeUpdate/StorageSWD connections.
`probe-control/diagnostics.json` records Control initialized, updater ready in
SDRAM and locked, zero metadata commits/resets, settings unprovisioned with
zero writes. A missing marker remains a normal first-boot state.

[Results page1 actual capture](capture-results-1/display.png) shows LCD PASS,
backlight25%, NOR READ PASS, RTC READ PASS, BT FAIL0302, ALS FAIL0002 and USB
WAIT CABLE, with29.9FPS and57.8% CPU in that displayed window. The host selected
page1 temporarily, captured EVE, then restored automatic page rotation.

The first NOR-backup attempt (`nor-full-backup`) stopped before any SRAM request
write because a host verifier compared PT_LOAD padding00 against canonical
FF. The actual APP read was byte-identical to the installed file. The host now
reuses the canonical image validator; the independent retry and its completion
state are recorded in `nor-full-backup-02/manifest.json`. Do not infer a verified
full backup from the existence of a partial file.

## Stock staging representation discovered in the raw backup

`stock-stage-independent-check.json` compares the complete448KiB reserved APP
stage from raw NOR A with the original internal-flash APP. Adjacent byte-pair
exchange gives an exact match, SHA256
`162faeecc64bf56d828570168785e66d1a7da0152ef912bf83f9a19094d91edf`.
The stock BL's SPI16 DMA path confirms that it consumes this exchanged wire
representation. The raw A/B files must retain physical wire order.

The earlier installed `89b289...` and built `02d940...` adapters passed canonical
APP bytes directly to raw NOR; they must not be used for OTA staging/commit.
No such transaction was performed. RuntimeUpdate now translates only APP
staging reads/programs between canonical and stock physical order, including
odd offsets/lengths. Raw backup, filesystem and NVM semantics stay physical.
Later build/install evidence below identifies the corrected image; model tests
and successful installation do not substitute for an actual OTA experiment.

## Full NOR backup completed

`nor-full-backup-02/manifest.json` is now `state=verified`, `verified=true`.
Two independent physical-wire-order captures A/B each contain134217728bytes;
the final byte comparison found **zero differences**. Both SHA256 values,
also independently rehashed after the tool exited, are
`970af11e42f6c148c59f2ca99dec8fba1f9552a2def1405a43f50d3d1c98156f`.
Every8MiB segment matched the device's immutable SDRAM-buffer CRC, and the
MCU UID, NOR identity and boot metadata matched before/after the full operation.
No format, provisioning, staging program or metadata commit was performed.

`storage-layout-observation.md` was first written from A while B was running;
the completed byte-identical B now independently corroborates those raw bytes.
The new NVM range overlaps the original BPB volume and must never be initialized
as if it were unused space. Any later approved repartition still needs an
explicit format operation and these preserved full backups.

## Final corrected Integrated installation

Run `../bringup-runs/2026-09-12-154706-643-Release` installed the corrected
APP383944bytes, SHA256
`04c09a59ba1c71ee95190118a568fdcf7543d3e17907eebde2c7fe7fc182e049`.
The exact ELF/BIN/manifest are preserved in `stock-codec-image`.
Debug452196bytes and Release383944bytes both built successfully; see
`build-stock-codec-debug.log` and `build-stock-codec-release.log`.

APP readback, original lower64KiB hash, reset/HAL/RTOS progress and graphics
frame progress passed. The install's observed frame rate was29.976FPS.
`probe-stock-codec-final/diagnostics.json` measured29.9FPS and58.9% CPU in its
1second window, with no missed frame slots. Runtime updater is initialized
but locked, with zero commits/resets; formats, file writes, NVM commits and
settings writes remain zero. BT startup0302 and ALS error2 remain unresolved.
No physical ELM/GPS/dashboard/USB input test or actual OTA was performed.

The corrected RuntimeUpdate source/model tests passed45529 assertions each
atO0/Os, including physical pair order, odd DATA fragments, no neighbor
rewrite, stage/page bounds, epoch changes, canonical READ_STAGE and complete
448KiB decode/hash equality. These host results do not certify actual radio,
flash interruption recovery or stock-BL OTA installation.

The final image's [results page1](capture-final-results-1/display.png) was
captured from EVE in20ms, with29.9FPS/61.0% CPU shown. Its raw460800bytes,
113 checked chunk records, generation/CRC and source APP SHA are preserved in
`capture-final-results-1/capture.json`. CPU S_HALT remained clear, and the
temporary page selection was restored before export. Pixel validation found
zero nonblack pixels outside the fixed active circle.

The same image's [page2](capture-final-results-2/display.png) and
[page3](capture-final-results-3/display.png) were also exported. An initial
visual interpretation of missing fixed labels was incorrect:
`static-labels-corrected-comparison.json` proves DEMO SPEED and km/h regions
are byte-identical across the three PNGs. The proportional page digit changes
the centered title's position; aligning its first lit column also gives an
identical unchanged title prefix. `static-labels-before.json` preserves the
initial unnormalized comparison, not evidence of a rendering defect.

Separately, review reproduced a real upstream primitive-cache mismatch:
EVE BEGIN is not saved/restored by SAVE_CONTEXT, while the software cache
restores its prior primitive. The project dispatch now synchronizes it before
each visible task. The corrected actual-C EVE state model excludes BEGIN from
context restoration and passes182 assertions each atO0/Os, including a legacy
sequence that draws rectangle corners as bitmap vertices. This is not claimed
to be a SNAPSHOT2 defect or an explanation of the misread PNGs.

## Primitive synchronization image and remote continuation

Run `../bringup-runs/2026-09-12-160529-453-Release` installed APP383960bytes,
SHA256 `731f3735abf3fff7fa2ab6953d93b6eb1b516fa6f13c5265f82e2b40f22bfbee`.
It includes the primitive synchronization fix and stock staging codec. APP
readback, preserved lower64KiB and running graphics/RTOS checks passed.
Exact ELF/BIN/manifest/map are kept in `primitive-final-image` before further
builds or the bounded stock comparison. The preceding section's image remains
historical evidence, not the current installed image.

`capture-primitive-final-1/display.png` shows29.9FPS/58.6% CPU and the actual
BT0302/ALS0002 failures. Its27ms snapshot and all seven completed captures passed
`capture-validation.json`: raw hashes, active-circle black exterior, and CPU
not halted before/after snapshot/export. This is EVE raster evidence, not a
photograph of the panel/backlight. USB WAIT CABLE in this historical image means
only that enumeration was absent; the next source changes it to WAIT ENUM.

After this image, UART DMA-abort failure handling and RTC alarm error handling
are being revised and tested. Source changes and host assertions must not be
reported as installed until the later installation evidence identifies them.
