# Retain riding mode; raise Ride Summary — 2026-09-18

## Changes
- `App_Logic/UI/src/ui_power.c` now selects Home only on first boot. A retained
  IGN return keeps the prior category, local item and footer through all three
  OFF states. Ring sweep and Welcome/fresh-frame gates remain independent.
- Remote-control arming and held actions still clear at an IGN epoch; full
  Settings still returns to its saved riding context. No navigation NVM added.
- `Graphics/UI/src/power_view.c` shifts the entire Summary group up24px.
  Title bottom161; Dist/Time/OIL bottom222/278/334. X alignment, fixed widths,
  font sizes, no-leading-zero formatting and the24px entrance motion remain.
  Shared clip expands upward to121 and retains lower edge384. Welcome stays.

## Validation
- Product-model ARM O0/Os/Oz:111,522 state checks +5,697 speed checks each pass.
  New coverage:8 categories ×3 local selections ×3 OFF stages, duplicate ON,
  unchanged footer, expected sweep, one ride end and no remote rearming.
- Power UI ARM O0/Os:1,151 each pass, using a non-home Music/item1 fixture to
  exercise actual coordinator resume while retaining backlight/scanout gates.
- Release CubeIDE build passed. APP373,012B; free85,740B. Debug also passed; APP388,444B, free70,308B.
- Core/IOC/vendor files untouched. Main remains the existing LCDTest facade.

## Installation status
Not installed yet. The running image remains the previous off-stages APP
`c4a69c5376a9c58eca8b399d1a4de4ab64b4e2ad96f2cd09a5cf1593f41a0b60`.
ST-LINK reports3.30–3.32V but cannot read MCU core ID, including after the user
reconnected USB and a normal-mode connection attempt. No new flash writes
have been issued. The safe APP-only installer is ready; it verifies donor,
previous APP and unchanged stock BL before programming/readback.

See build logs, model results, layout.json and connection logs in this folder.
No new hardware screenshot or physical IGN verification is claimed.
