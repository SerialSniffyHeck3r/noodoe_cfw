# Lower separator aligned to the speed-arc ends

The previous horizontal segment at y384 visibly floated above the speed-arc endpoint centers. Move it down19px to y403, matching the nominal endpoint center y403.34 for a radius231 arc centered at(240,240).

Final points: (92,422), (116,403), (364,403), (388,422). The side arms move outward to clear the existing fixed footer text. Mode, number, unit, clock, central content rectangle, OIL arc and calculation/state code remain unchanged. Production changes are confined to the lower separator array and its explanatory comment in `speed_home_layout.c`.

Validation completed:

- Geometry/font checks passed, including circle containment, actual rounded speed-arc endpoints and footer label collisions (`assets.json`).
- Debug and Release builds passed. Release APP382,752bytes; reserve76,000bytes. Existing RWX linker warning remains.
- Installed APP SHA-256: `8ec80ff1e8e31d37456204ec83951ccc9e3f49a51f22eb3d2d88f2f88e110b15`.
- APP readback, lower64KiB stock BL/config preservation and two reboot/tick/RTOS checks passed (`install/summary.json`).
- Actual EVE capture verifies the new lower path and the unchanged upper separator. Central text, mode/number/unit and OIL arc regions are pixel-identical to the preceding capture (`unchanged-screen-regions.json`).
- Full frame shows29.8FPS and56.3% CPU at capture time. Preview cancellation and real selection preservation passed (`preview/results.json`).

Actual controller capture with explicit test values: `preview/odo-oil75/display.png`. This is not a photograph of the LCD. No changes to time/usage persistence, storage provisioning, RTC settings or peripheral configuration were made by this correction.
