# 0.9.57 validation

This release combines the0.9.56 display candidate, the later display simplification and the map/media latency follow-up. All new measurements below are software tests. No device, vehicle, SPP radio or physical LCD was accessed.

## Package and source identity

- Companion0.9.57, Android versionCode87, minimum API23.
- Product SHA-256: `ac95375a955308eab32c94b504edb765e84765824eaa242873a6f7c253f04ec6`.
- APK signing certificate matches the public0.9.55 APK. The exact Product SHA in the app matches the installation ZIP.
- The actual app importer accepts both the current and previous ZIP. Gate bytes and Bootstrap, stock, resources, uninstall and diagnostic images are byte-identical to0.9.55.
- Application source and detailed source/test inventories belong only in the private source repository. Public Git history contains documentation; APK, firmware ZIP and compiled relink objects are release assets. Keys, device dumps, caches and build directories are excluded from source commits.

## Validation

- 54 Android tests pass with no skips, failures or errors. This includes resource ordering, real Korean map compilation/transmission, in-flight slot identity, zoom cache reuse, unchanged-song artwork return, notification delivery and update/importer checks.
- Android lint:0 errors,0 fatal findings,135 warnings.
- ARM background/stream tests: six DATA_DEBUG/optimization combinations pass. Under a model of5ms owner visits and35ms composition slots, restoring a full retained album texture after map borrowing completes in1,015ms before versus400ms with interframe work. Fade time and real SPI execution are excluded.
- A saved0.9.56 ARM harness reproduces one unnecessary complete raster and230,400B upload after an offscreen neighbor arrives. The current renderer performs neither when the selected geometry is identical.
- 28 complete NVM5 packets preserve feature/point data;28 malformed packets preserve the preceding valid tile.100 hidden-map owner calls upload zero bytes. A zoom restarts the unpublished back raster while retaining the displayed front.
- Fixed-clock and advancing-clock raster runs produce identical pixels (SHA-256 `395f27ebcd9022767ab607fa40af2f5345ae10d8583aa2a71fe213cbf134b535`). Cooperative rows/time limits remain.
- 1,203 compiled ARM/LVGL/EVE frames: peak4,984/8,192 command bytes, zero displayed-texture/shade overwrites and18 pixel-equivalence checks.
- Warning rendering:6 scenes,18 predecessor-state injections and45 production-clock frames pass. This does not resolve the reported physical ignition behavior without hardware testing.

## Resource bounds

| Build | Flash free | SRAM free | CCM free |
|---|---:|---:|---:|
| Product Debug |2,072B |49,384B |16,320B |
| Product Release |34,044B |48,424B |16,320B |

Debug2KiB and Release4KiB hard minima pass. Compiler policy, heap/stack, queues, device RAM reservations, fonts and icons are unchanged. The phone-only prepared-packet cache is bounded to32 packets, at most1.5MiB of payload.

The28 Korean map samples range from784 to46,000B per tile (mean20,580B); the wire maximum remains49,152B. Only local vector tiles are transferred, not the whole-country map file. Successful UART probing does not measure end-to-end SPP throughput.

The replaceable-library kit passes D8, zipalign and APK-signature verification with a disposable validation key. It contains the matching APK, compiled JAR objects, instructions and license notices; no application source or signing keys are included.

Install the matching APK and ZIP together. The initial upgrade still runs the previous firmware's updater. Actual installation success, RF speed, sustained LCD30fps and cold/ignition behavior remain unmeasured.
