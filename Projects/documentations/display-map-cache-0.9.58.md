# 0.9.58 validation

The matching APK/ZIP are built and software-tested. The user explicitly authorized
publication and a1KiB Debug flash reserve. Release retains the root16KiB minimum;
the development target is32KiB. The shared build/package gate enforces these limits.

## Implemented behavior

- Untouched GPU map textures survive page changes. Actual image uploads and GPU
  reset invalidate that residency. Hidden maps neither draw nor rasterize/upload.
- A complete RAM raster restores after GPU eviction only when revision, scale
  and the existing12pixel camera tolerance match. Partial rasters are rejected.
  Restores use at most5760B per visit; cold geometry retains8rows/cooperative2ms.
- Album restoration uses6KiB between composition slots, retaining16KiB at
  composition, the original image quality,240ms fade and actual-swap fences.
- Includes the previously unpublished predictive map loading, spatial raster
  index, adjustable automatic range, original rounded fuel glyph and ordinary
  LVGL warning-background restoration. No additional RAM allocation in the
  reentry change; the earlier spatial index adds2,512,832B of SDRAM.

## Results

- Android57 tests passed, including real current/previous ZIP importer,
  map preparation/prediction and media/resource scheduling. No skipped tests.
  Lint:0 errors/fatal,135 warnings. APK certificate matches0.9.57.
- ARM/LVGL/EVE stress:1,203 frames, maximum4,984/8,192 display-list bytes,
  zero displayed texture/shade overwrites. No reference-image comparison is
  inferred from the legacy stress-report field named pixel_equivalence_frames.
- Unchanged map reentry:60→1 owner visits,480→0 raster rows,230,400→0 upload bytes.
  After artwork eviction:68→48 visits,480→0 calculated rows,230,400 copied bytes.
  After GPU reset:60→40 visits. Complete map pixel hashes match the baseline.
  Camera changes and partial caches were rejected; hidden owner visits upload0B.
- Cold raster: at most8rows/3,840B per visit, or2rows/960B with the injected
  advancing clock. Complete raster hashes match. Four zoom transitions and a
  mid-raster reversal pass with delayed swap fences and no overwrite.
-24 real Korean map packets and24 malformed packets tested.36 warning-dimmed
  and9 expired frames pass. Both phases of the original Google Material Icons
  Round U+E546 footer match the existing glyph bytes.
- Album scheduling model:400→310ms upload, excluding the unchanged240ms fade,
  real SPI execution and scheduling overhead. All six ARM build variants pass.
  SPI transfer/fault/preemption model:40 cases pass. No bus clock or SPI driver
  was changed.
- Debug flash free2,056B; Release34,084B. SRAM free49,312/48,360B; CCM16,320B.
  Authorized1KiB Debug/16KiB Release gate passes. Compiler flags,
  heaps, stacks, fonts, icons and GPU reservations were not reduced.
- Product ZIP bytes equal the Release image padded to the0x60000 partition.
  APK riding contract matches that SHA-256. Gate, Bootstrap, stock, resources,
  uninstall and diagnostic images match0.9.57 byte-for-byte.
- Replaceable Mapsforge object kit reconstructed through D8, zipalign and APK
  signature validation; contains no application source or signing keys.

No bench, vehicle installation, physical RF throughput, LCD FPS or ignition
warning reproduction is claimed. First entry without cached tiles still needs
phone preparation and transmission. Predictive coverage tests from the preceding
candidate assume a warmed cache and2second work delay; they do not guarantee
gap-free operation at180/200km/h on a real radio link.

Product identity (padded partition):
`637b1e1fa89644e800eddab2dc7ad2f56b7fea7d818b830e8c42ceff536f4c97`.
Android versionCode88/versionName0.9.58. Detailed machine-readable results are retained with the matching private source snapshot.
