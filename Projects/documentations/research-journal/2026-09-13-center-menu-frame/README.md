# Central riding-UI frame and small footer alignment adjustment

Implemented from the preceding actual EVE capture (`before-display.png`).

- Move mode text and its SERV/RESV replacement icons right6px, x112 ->118.
- Move the clock separator up3px; retain clock typography/position.
- Add an oppositely facing lower trapezoid: (120,403), (144,384), (336,384), (360,403). Both separators use2px strokes and0x71867C.
- Fix the central content viewport at x72..407, y108..375 (336x268). Its complete rectangle is inside the speed ring and clear of both separators.
- Reparent existing central card/menu/warning/edit labels into this fixed LVGL container while preserving their absolute coordinates. Child clipping uses the existing EVE scissor path; no framebuffer or software mask was added.
- Expose `SpeedHome_GetContentRoot()` and `SpeedHome_GetContentArea()` for future riding UI. The clock, ring, footer and existing development HUD remain owned by the surrounding view. The development HUD's occupied band remains documented.
- Preserve number sizes/positions/right alignment, bottom-aligned km/mi, unpadded values, OIL remaining arc, HOURS/DAYS and all domain/state/storage calculations. `logic-preserved.json` confirms the preceding tested UI/state/calculation source hashes are unchanged.

## Validation

- Geometry/font checks passed, including both separator paths, label collisions and central rectangle containment (`assets.json`).
- Release APP382,752bytes with76,000bytes reserve; Debug APP442,288bytes with16,464bytes reserve. Both builds passed with the existing RWX LOAD-segment warning.
- Release APP SHA-256: `1977ebf6b90e1b0770b6de4e1f56f2614cd64b3d67698b5dc8613f9a450e0ce8`.
- APP installed at0x08010000; full readback and lower64KiB preservation passed, followed by two reboot/tick/RTOS checks (`install/summary.json`).
- Three real EVE captures verify the6px mode/icon shift; numeric/unit/auxiliary pixels remain identical to the preceding capture. Reparented central text is also pixel-identical at the original screen coordinates. Both separator shapes are present. See `capture-validation.json`.
- Full captured frame reports29.8FPS and56.6% CPU at that instant. This is not a worst-case performance claim for future screens.
- Preview cancellation was acknowledged (`active_id=0`); no preview remains active.

The initial diagonal-line assertion expected a solid stroke color at a single subpixel sample. An antialiased channel differed by9 rather than the8 tolerance. The test now checks the stroke's3x3 footprint. `validate_captures.py` rechecked the already acquired CRC/SHA-verified bytes and passed; no additional device command or firmware change was needed. The earlier diagnostic is preserved in `preview.log`.

## Actual EVE captures (explicit preview values)

- `preview/odo-oil75/display.png`: complete screen with both separators.
- `preview/oil-hours/display-bottom.png`: OIL mode shifted6px; hours unchanged.
- `preview/service-days/display-bottom.png`: SERV icon shifted6px; days unchanged.

These are controller captures, not photographs of the physical LCD. Existing limitations of unprovisioned settings storage, absent BT/ALS verification and future menu/service effects remain unchanged.
