# GPS full-screen corruption hotfix — 6.9.7

The GPS edge-fade renderer expanded each visible grid line into many independent LVGL/EVE jobs. Each job carries a complete clip/context/primitive sequence. Rotating the world grid therefore changed display-list size substantially. The old producer had no frame-sized geometry bound.

The production grid projection and old/new UI drawing functions were executed as Cortex-M4 code against the unchanged EVE line backend and project scissor wrapper. For 648 combinations of heading, world offset and all six zoom levels, with the full48-point visible trail, the old grid+trail reached **7,732 bytes**. The actual always-visible speed-ring track and round viewport add **516 bytes**: **8,248 bytes**, already over the8,192-byte list before clock, footer, icons, background or arrow. This reproduces an overflowing command producer in software; it is not a recording of the user's vehicle GPU.

There is also an independent coordinate defect: route projection permits±30,000 pixels, but EVE `VERTEX2F` packs signed15-bit coordinates. Pixel scissoring cannot prevent those distant vertices wrapping into different positions. The fix clips whole segments against the GPS window before creating LVGL draw jobs. It does not clamp endpoints independently and invent false edge paths.

## Changes

- `Graphics/UI/inc/Gps_GridFade.h`: bounded parametric line clipping using the existing M4F; no large intermediate integer products.
- `Graphics/UI/src/dashboard_pages_view.c`: shared clipping for grid and route; four nested translucent strokes instead of sampled edge segments. Source-over alpha preserves centre opacity120 and the page-transition fade. World anchoring, rotation, interpolation, zoom and all48 trail points remain.
- No EVE driver/vendor, DMA, swapping, GPS ingestion, Bluetooth, storage, Gate, resources or Android changes. No framebuffer or persistent GPU allocation added.

## Validation

- Actual ARM O0/Os/Oz:648 heading/offset/zoom cases each; maximum grid+trail **4,212 bytes**, versus7,732 before. Grid maximum53 jobs. Every opacity0..255, page translation, far crossings, offscreen parallel lines and degenerate points tested. No submitted coordinate exceeded the EVE range.
- The same ring+mask brings this subset to4,728 bytes. This is a subset command count, **not a claimed maximum for every complete UI frame**. Existing common chrome and other overlays still consume display-list space.
- Driving policy/GPS interpolation and page-model regressions; unchanged EVE state/clip/primitive regression.
- Release:327,672 bytes used, **65,544 bytes free**. Debug:359,464 used, **33,752 free**. SRAM free53,272/54,192; CCM16,320. Original budgets, RTOS heap, stacks and queues preserved.
- Production Android package importer accepts the new bundle. Bootstrap, Gate, stock, resources, uninstall and diagnostic payloads remain byte-identical to6.9.6. APK6.9.6 is reused byte-for-byte.

No bench, ST-LINK, vehicle screenshot, real Bluetooth or FPS measurement was performed for this hotfix. Software reproduces the overflow cause; final confirmation of the reported physical symptom requires the updated vehicle firmware.

## Update

Use the existing APK6.9.6 and new6.9.7 GPS hotfix ZIP through the normal CFW update route. The Gate is unchanged; this hotfix does not require returning to stock or reinstalling Bootstrap. Normal update confirmation/reset policy remains unchanged.
