# 0.9.20: maps, menus and notification popups

> **0.9.22:** Map scale and shading now follow [the latest changes](16-Display-Transitions.en.md). The text below records the earlier release.

[한국어](14-Map-Popup.md) · [User guide](README.en.md) · [Latest downloads](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/latest)

Update the APK and CFW together. Keep the existing Gate: no return to stock is required. Settings, photos and maintenance records stay in place.

## Five menus

Short O cycles **Home → Trip → Music → Phone → Map → Home**. The five icons stay put; the current one grows. The strip hides ten seconds after the last operation. Trip remembers its selection; Phone opens at its summary. Calls and notifications remain inside Phone.

Playing music makes the music icon green. New/unread notification colors now belong to the phone menu icon. Bluetooth keeps its connection color.

## Notification popups and ten replies

Keep using the app allowlist and important-notification filter. Enable automatic display to see the latest eligible new notification over the current page. This replaces automatic page switching without changing your original menu or selection.

- The window lasts ten seconds from arrival. If its image completes after expiry, no belated popup appears.
- UP/DOWN dismiss. Release O briefly to dismiss, or hold O for0.8seconds to choose a reply.
- In reply selection: UP/DOWN select, short O sends, long O cancels. Configure up to ten replies, shown five at a time. Existing replies and signature are retained.
- Selecting a reply pauses expiry and locks its notification ID/revision. Incoming notifications cannot change that target. Deletion, modification or lost reply permission closes selection.
- Display and reply are allowed **below60km/h**. Unknown speed with IGN ON is blocked. Explicit parked use follows its existing policy.
- PH9, calls, warnings, installation, recovery and power screens keep priority. Dismissed key events cannot leak into the page underneath.

These are **EVE software simulations** from real firmware LVGL/EVE commands and Android-generated images, not LCD photographs. The notification image moved8px upward inside the same container.

![Notification over a map](../images/popup-0.9.20-simulation.png)

## Map and background

Import a Mapsforge `.map` region on the phone, enable map display and provide a current position in that region. UP/DOWN on the dashboard map changes scale up to12km. Only newly needed tiles transfer as the position moves.

Major roads are orange; ordinary roads white/gray; water light blue; greenery translucent green. Regional trunks and areas use coarse source geometry, while local roads come from a detailed source level. Entire ordinary-road classes no longer disappear at the old thresholds. Dense scenes simplify and omit geometry within the frame command budget.

Music, notification and map background photographs share25% brightness. Map geometry remains bright, excludes clock/ODO regions, and fades under the common top/bottom gradients. This does not change backlight brightness.

![50m map simulation](../images/map-50m-0.9.20-simulation.png)
![12km map simulation](../images/map-12km-0.9.20-simulation.png)

During STOP wake, clock/ODO text, separators and masking move together. Repeated software tests cover command-list overflow, drawing-state leakage and image-buffer lifetime. Recurrence of the reported physical lower-screen tearing still requires a vehicle test.
