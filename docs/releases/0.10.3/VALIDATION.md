# Companion 0.10.3 validation

- 102 selected Android tests passed, zero failures/errors/skips. Includes the actual public archive download via the production Java downloader, pinned import, UI explicit consent / Later / backgrounding / stale-answer rejection, persistent setup access, no false radio-timeout warning during an internet download, and guarded Bootstrap help.
- Actual ARM language installer integration passed for healthy publication, explicit FONT repair boundaries, and moving-device rejection. No physical hardware was accessed.
- Existing firmware installer, boot handoff, retained workflow and actual 0.9.73 / 0.10.1 / 0.10.2 bundle imports passed. Gate migration policy and all firmware images are unchanged.
- A responsive asynchronous-storage fixture transferred two complete pages with all command serial/offset checks and zero fixed poll sleeps. The former fixed delay contributed 120 ms for these 12 commands. This does not predict total real-device transfer time; backoff remains for busy storage.
- APK 0.10.3/code106 retains the 0.10.2 signing certificate and exact Product hash. Firmware ZIP 0.10.2, language ZIP 0.10.1 and license notice are byte-identical to the prior release. Product flash free remains Debug1036 / Release1036 bytes.
- All eleven app locales contain the new guidance without missing strings or locale validation errors.
- Real radio timing, actual Xciting vehicle data recovery, battery draw and physical power-cut recovery have not been measured. The accepted legacy stock-return power-cut limitation remains.
