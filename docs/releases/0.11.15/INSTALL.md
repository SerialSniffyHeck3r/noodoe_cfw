# NewDoe 0.11.15 — installation and manual update

**2026-10-19: beginner manuals and app quick help rewritten. Firmware behavior is unchanged.**

- [한국어 — 처음부터 따라하기](../../../Projects/Manuals/Guide.md)
- [English — complete step-by-step guide](../../../Projects/Manuals/Guide.en.md)
- [기존 CFW 업데이트](../../../Projects/Manuals/Guide.md#update) / [Update existing CFW](../../../Projects/Manuals/Guide.en.md#update)
- [언어팩과 언어 변경](../../../Projects/Manuals/Guide.md#language) / [Language resources](../../../Projects/Manuals/Guide.en.md#language)
- [폰 없이 복구·순정 복귀](../../../Projects/Manuals/Guide.md#recovery) / [Recovery without a phone](../../../Projects/Manuals/Guide.en.md#recovery)

## Which files?

| File | Purpose |
|---|---|
| `NewDoe-0.11.15-manual-r2.apk` | App with revised native help, versionCode 127. Install over the existing app; keep app data. |
| `NewDoe-CFW-0.11.15.zip` | Device firmware. Unchanged from the original 0.11.15 release. |
| `NewDoe-LANGUAGE-B-0.11.2.zip` | Optional B resources. Unchanged; healthy installed resources are reused. |

Already running device firmware 0.11.15? The manual-r2 APK does not require a device update or language retransmission. The original versionCode 126 APK still works with that firmware; its offline help is older. Both apps open the current online manual.

For a first installation: APK → select CFW ZIP → stock check → Bootstrap transfer → wait for Bootstrap → Check current status → device Install CFW → optional Bluetooth reset (default No) → prepare/install → confirm the actual CFW UPDATE screen → optional language resources. Read the full guide before starting.

With a matching Gate (normally 0.11.4 onward), use normal CFW update. Older Gates require keep-data stock return and reinstall; let the app verify the actual Gate. No Gate OTA is added. The legacy old-code stock-return power-interruption limitation remains. Keep permanent power connected throughout installation/recovery.

100% is transfer progress, not the final result. Language resources continue through verification/saving, restart, reconnection and ready confirmation. IGN changes do not cancel that job; keep the vehicle parked and the module powered.

Camera-triggered disconnection and physical parked current/endurance remain unverified. This documentation revision does not claim a firmware fix or new hardware qualification. [Original firmware validation](VALIDATION.md) and [manual revision checks](MANUAL-REVISION.md) describe separate scopes.
