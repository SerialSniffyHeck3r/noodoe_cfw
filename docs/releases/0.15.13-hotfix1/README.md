# NewDoe 0.15.13-hotfix1

Android build 163. Install over the existing NewDoe app without clearing data.

This is an Android-only stability hotfix. The APK still includes the exact verified 0.15.13 firmware, language resources and Diagnostic. If your headunit already runs 0.15.13, update only the phone app. Older headunits still use the app's normal status/update flow.

## Changes

- Coalesce Bluetooth ACL and companion-presence callbacks through one connection owner. Events from an in-progress connection cannot refill its retry budget or start duplicate foreground services.
- Preserve a restart budget across process recreation. Three unexpected service recreations within ten minutes pause automatic restarts for ten minutes, without scheduling wake alarms. Opening the app permits a fresh attempt.
- Stop location-check timers, media subscriptions and hidden Activity updates during their inactive states; restore them when needed.
- Fence callbacks after service destruction, acknowledge foreground-service starts, and stop cleanly on Android foreground-service timeout without replaying installation commands.
- Handle Bluetooth reader and media-provider runtime failures at the callback boundary. A zero-byte stream read cannot spin indefinitely. Cancelled socket deadlines are removed immediately.
- Clean up every independent resource even if one provider fails; clean up partial runtime construction too.
- Include bounded, payload-free crash call locations and Android process-exit counters in diagnostic exports. Android's original crash handler still terminates normally; the app does not swallow fatal errors.

Google Maps, weather, navigation, O-long footer selection, firmware memory layout and installation safeguards remain as in 0.15.13. Existing settings, pairing and ride records are retained. The original APK signing certificate is preserved.

## Validation limits

Software-tested beta. Android lifecycle simulations include API 28 and 35. The reported Galaxy S26 Ultra / One UI 9 crash has not been reproduced on physical hardware, and its exact original exception remains unknown. Real battery savings, Bluetooth range and welcome-light latency are not measured. Full lint still contains inherited findings; new findings are checked separately.

Gate is unchanged. The previously accepted legacy stock-return transition power-interruption limitation remains. Follow the current recovery manual; no Gate/resident modification is included.

The illustrated manual and license/relink files accompany this release. Normal users install the APK; the firmware ZIPs are for expert import only.
