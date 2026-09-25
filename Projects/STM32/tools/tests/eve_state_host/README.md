# EVE state command-stream tests

This fixture compiles the project's unchanged upstream `lv_eve.c`, `lv_draw_eve_fill.c`, and `lv_area.c`,
production `graphics_geometry.c` and `graphics_eve_viewport.c`, with the actual
LVGL headers, viewport constants, and `lv_conf.h`, for Cortex-M4. It executes the resulting
freestanding C in local Unicorn, with a 20-second / 5-million-instruction bound.
It does not build the Cube project or access the physical board.

The runner extracts `EveDispatch` and the first-frame `bitmap_baseline_pending`
block verbatim from `Graphics/Port/src/graphics_eve_port.c`. It fails if their
signatures or structural markers disappear. `EVE_cmd_dl_burst` is replaced by a
bounded command recorder. A small state model interprets clip/bitmap commands and
the viewport context registers across SAVE/RESTORE; it does not emulate EVE rasterization or claim
anything about screen appearance. BEGIN/END are deliberately outside saved context:
RESTORE cannot restore the selected primitive. This corrects the earlier fixture's
incorrect inclusion of BEGIN among context registers.

Cases deliberately reproduce the upstream failure before checking the adapter:

1. Changing only a clip's x1 leaves the SIZE word stale.
2. The production clip wrapper emits both XY and SIZE; the dispatch synchronizes
   primitive selection with the cached RECTS setter and an explicit BEGIN(RECTS).
3. SAVE/clip/RESTORE leaves the vendor C clip cache stale; the clip wrapper ignores it.
4. An idle dispatch emits no commands and preserves the upstream result.
5. Resetting only the modeled GPU leaves a warm bitmap C cache stale; the
   lifecycle seed restores source, size, layout, handle and extended bits.
6. A previous bitmap with nonzero high bits also recovers after reset.
7. The one-shot seed does not repeat on subsequent frames.
8. The upstream zero primitive setter is ignored; only a changed valid primitive
   emits BEGIN. Zero is therefore unsuitable for invalidating the cache.
9. Entirely outside draw extents, and visible extents with a disjoint clip, finish
   without calling the renderer or emitting upload/draw commands.
10. `_real_area` extending into the circle is rendered even when nominal `area`
    is outside; clipping those real extents back outside causes culling. Single
    pixel tasks at x=0/479 on y=239/240, and their transposed top/bottom positions,
    reach the renderer. The diagonal boundary(69,70) remains outside while(70,70)
    dispatches; all four raster corners remain culled.
11. The actual viewport source emits exactly 30 words / 120 bytes, clears stencil before its
    circle, disables color writes while drawing the mask, uses center(239.5,239.5)
    and radius240 (`VERTEX2F(3832,3832)` / `POINT_SIZE(3840)`), keeps both mask and
    outside fill under the 480x480 scissor, draws black only for NOTEQUAL outside fragments, and
    restores all modeled context state despite dirty preceding settings.
12. Raw viewport commands preserve the upstream clip/color cache and maintain the
    same bounded command cost on a subsequent invocation. Primitive selection is
    handled separately at the next dispatch boundary.
13. A legacy SAVE / BEGIN(BITMAPS) / RESTORE sequence leaves actual primitive
    selection at BITMAPS while the vendor C cache says RECTS. The unchanged
    upstream fill function then emits its rectangle corners as BITMAP vertices.
    The fixture explicitly detects this incorrect legacy behavior.
14. The production dispatch repairs that state before calling the same actual
    fill function. BITMAPS, POINTS, and an edge primitive followed by raw END are
    tested as predecessors. The synchronization emits no vertices and adds one
    BEGIN word when the C cache already says RECTS; all fill corners then use RECTS.
15. The actual `graphics_eve_clip.c` wrapper emits inclusive XY/SIZE for every
    vertical edge position from144 through172, including after SAVE/RESTORE.
    No clip expands into an adjacent page or half of the moving/stopped bar.

Run using the installed Python and ARM GNU toolchain:

```powershell
& '<local-user>/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' -X utf8 tools/tests/eve_state_host/run_eve_state.py
```

`output/results.json` records source/config/extracted-block/ELF hashes, assertion
counts, return status, and both `-O0` and production vendor `-Os` results.
The current run passes **279 assertions in the fixed 15-case sequence for each optimization**.
The generated `.inc` files are test artifacts and are not firmware source files.

An initial visual interpretation of repeated snapshots suggested missing labels.
The root task's pixel comparison subsequently refuted that interpretation:
DEMO SPEED and km/h were identical across all three captures, and the centered
page title moved with the width of its changing digit. No snapshot clipping or
SNAPSHOT2 bitmap corruption was established. These tests independently establish
the primitive-cache defect and its repair. [FT81x Programmer Guide](https://brtchip.com/wp-content/uploads/Support/Documentation/Programming_Guides/ICs/EVE/FT81X_Series_Programmer_Guide.pdf)
sections4.1,4.5 and4.30 distinguish saved drawing context from primitive actions;
bitmap source/layout/size belong to persistent per-handle state. Actual capture
comparison remains separate from this command-stream regression.

The dispatch wrapper relies on the project's single-task LVGL ownership and the
upstream task-search function being a read-only search. Supported rendering calls
use the link-wrapped clip operation; changing either assumption requires another
adapter review. The test uses a mock upstream dispatch to reach the actual scissor
setters, rather than pretending to test the full scheduler.
