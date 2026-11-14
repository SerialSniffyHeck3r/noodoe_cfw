# NewDoe 0.15.13 beta

Android build 162 and matching device firmware. Install the APK over the existing app; do not clear data.

## Changes

- Multilingual Google Maps next-turn distances, road names and destination summaries. Fixes Taiwanese meter distances dropping guidance and missing road text. MAP/full TBT show destination distance, remaining duration and arrival time when supplied by the notification.
- CLDR-based unit/AM-PM handling, local decimal digits and expanded notification fields. Missing/ambiguous data stays unavailable instead of being estimated.
- Phone retries stop after three connection failures until a Bluetooth/user event. Removes timed vehicle-absent reconnect alarms and reduces parked polling.
- Classic Bluetooth vehicle paging at 3 seconds in screen standby / 5 seconds in Bluetooth standby, and corresponding connected Sniff requests. No BLE. Full OFF stops detection. Sniff support is negotiated with the phone, not assumed.
- Five-minute proximity welcome-light test under Device → Device connection · app theme, translated into all eleven app languages.
- Reuses map/geometry computations without removing features, visuals, heap or stack reservations.
- Hold O in analog/digital clock or full TBT to cycle the bottom ODO / TRIP 1 / TRIP 2 / oil / belt / service distance display. Fuel RESERVE still has priority. ETA strips and other pages' long-O controls are unchanged.

## Installation

Use the APK's included packages and normal device-status/update flow. Existing settings, trips, media, pairing and NOR layout are retained. The APK and firmware must be updated as a pair. The standalone ZIP is for expert import; normal users should let the app identify the device and language. Never install the font-preparation image as a normal final firmware.

The illustrated Korean/English manual ZIP contains offline HTML and images. Open its index.html. Font, weather, navigation, Unicode CLDR and Babel notices are in FONT-LICENSES.txt and the app. Relink objects are provided for the applicable library licenses; private application source is not part of the public release.

## Validation limits

Software-tested beta. Notification replay tests cover eleven supported app languages and additional scripts, plus 17,338 generated CLDR unit examples from 1,066 locales. Eight inherently ambiguous narrow-unit examples intentionally remain unknown. These are parser tests, not captures from every country's live Google Maps app. Real radio range, phone background restrictions, Sniff acceptance, welcome-light response and battery savings have not been measured on hardware.

Gate is unchanged. The previously accepted legacy stock-return interval can still be interrupted unsafely by power loss; this release does not repair that old installed-code interval or claim hardware qualification. Follow the current recovery manual and preserve app data.

