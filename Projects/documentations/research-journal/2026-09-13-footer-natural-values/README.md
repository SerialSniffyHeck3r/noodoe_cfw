# Natural numeric values and bottom-aligned distance units

User scope: retain fixed field widths, numeric positions, sizes and alignment; align the bottom of km/mi with the number and remove unnecessary leading zeroes from values.

## Implementation

- Unit field alone moves from baseline 424 to 437. Its x=322, width=40 and Lato20 font remain unchanged. Visible unit ink ends at y=436, matching the D-DIN36 numeric ink envelope.
- Main number remains x=194, width=116, baseline=436, right aligned. Auxiliary number remains x=210, width=100, baseline=464, right aligned. Both retain the right edge x=309.
- `Ui_Number.h` shortens display strings in place: 036475 -> 36475; 0123.4 -> 123.4; 0075.0 -> 75.0; 00365 -> 365. The three existing numeric formatters use it, including editable values and the speed model's ODO string.
- Zero remains 0 or 0.0 as appropriate. Fractional precision, unknown/overflow markers, numeric capacity and conversions remain unchanged. Clock HH:MM retains its two-digit fields.
- OIL arc, ignition-hour accumulation, calendar/remaining-life logic, storage and state-machine behavior are retained. Source changes are recorded in `changes.diff`; pre-edit copies are under `before/`.

## Verification

- Actual Cortex-M4 C tests passed at O0 and Os: 110,584 state/presentation assertions plus 416 speed assertions each. Coverage includes zero, sub-unit values, digit-count transitions, metric/mile formatting, HOURS/DAYS, editable values, overflow and source-value preservation (`ui-results.json`).
- Fixed geometry, font vocabulary, km/mi bounds, circle containment and non-overlap checks passed (`assets.json`).
- Release APP 382,528 bytes, reserve 76,224 bytes; Debug APP 442,064 bytes, reserve 16,688 bytes. Both builds passed with the existing RWX LOAD-segment warning.
- Release installed at 0x08010000, full APP readback matched, lower64KiB stock BL/config preserved, and two reboot/tick/RTOS checks passed (`install/summary.json`).
- APP SHA-256: `8eb4abf5072422e67601eed7d511d338119518b9b93611e2cbbb6fffb45e7dfd`.
- Real EVE captures passed for OIL HOURS, SERV DAYS and ODO with a75% OIL arc (`preview-final/results.json`). Numeric and km ink end at y=436 in all three captures; mi uses the same checked layout/font metrics. Full ODO capture shows29.9FPS and55.4% CPU at that instant. Preview values were canceled and the normal selection restored afterward.
- Initial capture assertion used only bright pixels and excluded the dim antialiased bottom row of D-DIN. The assertion was corrected to include the actual edge pixels; no firmware change was needed. The earlier diagnostic is preserved under `preview/`.

## Actual EVE captures (explicit test data)

- `preview-final/odo-oil75/display.png`
- `preview-final/oil-hours/display-bottom.png`
- `preview-final/service-days/display-bottom.png`

These are display-controller captures, not photographs of the LCD. Actual ignition-ON accumulation continued with no reported observation gap; NOR settings remain unprovisioned and RAM-only (`persistent=0`). No formatting, provisioning, RTC setting or stored usage migration was performed by this display change.
