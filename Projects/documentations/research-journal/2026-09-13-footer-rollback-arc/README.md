# Footer geometry rollback, retaining oil arc and maintenance calculations

The final user clarification limits this rollback to interface positions, sizes and alignment. The continuous OIL remaining arc, HOURS/DAYS presentation, ignition-ON accumulation, calendar/remaining-life calculations and asynchronous settings checkpoint logic are retained.

## Display changes

- Restore the main distance number to D-DIN 36 px, right aligned in x=194..309, baseline y=436.
- Restore mode text to x=112, baseline y=424, and unit text to x=322, baseline y=424. Keep `km` / `mi` content.
- Preserve the auxiliary HOURS/DAYS row; its number ends at the same x=309 as the main distance number.
- ODO, ordinary trips and reserve display the continuous OIL remaining arc. OIL/BELT/SERV show the elapsed HOURS/DAYS row instead.
- Preserve SERV-only wrench and RESV gas-pump icons and the reserve distance warning threshold.

Production changes for this rollback are confined to `speed_home_layout.c` and `SpeedHome_Layout.h`. See `layout.diff` and the pre-edit copies under `before/`. `logic-preserved.json` records unchanged UI logic hashes against the preceding tested version.

## Verification

- Debug and Release builds passed. Release APP: 382,512 bytes, 76,240 bytes free. Debug APP: 442,048 bytes, 16,704 bytes free. The existing RWX LOAD-segment linker warning remains.
- APP installed at 0x08010000; complete readback matched. The lower 64 KiB stock bootloader/configuration remained unchanged. Two stock-bootloader reboot checks passed (`install/summary.json`).
- Installed APP SHA-256: `be93d22d940d2332235d797e54739b30bceb536e5acddb286978cb731f01bf6e`.
- Bench UART speed transitions and stale-data handling passed, approximately 29.9–30.6 FPS and 56.4–57.0% reported CPU (`verify-normal/results.json`).
- Geometry asset checks passed (`assets.log`). Real EVE captures for OIL hours, SERV days and ODO with a 75% OIL arc passed (`preview/results.json`). No footer overlap or active-circle leakage was observed. These captures contain explicitly marked preview values, not saved vehicle maintenance data.
- Preview overrides were canceled after capture; normal UI state was restored.

## Captures

- `preview/odo-oil75/display.png`: complete EVE image, restored footer and OIL arc.
- `preview/oil-hours/display-bottom.png`: actual captured footer with HOURS.
- `preview/service-days/display-bottom.png`: actual captured footer with DAYS and SERV wrench.

## Persistence boundary

Actual ignition-ON accumulation advanced from 190,770 to 193,048 ms during the final check, with no reported sampling gaps. The donor's settings storage remains unprovisioned (`persistent=0`, `SETTINGS_NOT_PROVISIONED`), so this hardware run does not demonstrate durable counter storage. No provisioning, formatting, RTC setting or settings-record migration was performed. The retained checkpoint/restore code was covered by the preceding host tests under `../2026-09-13-footer-arc-hours/`; the geometry-only rollback does not change that code.
