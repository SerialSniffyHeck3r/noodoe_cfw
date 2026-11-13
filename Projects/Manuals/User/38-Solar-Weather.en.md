# Instrument arc color and twelve-hour forecast

Use the matching 0.15.12 app and firmware.

## Arc color theme

Choose **Settings → Display → Arc color theme** on the instrument, or change the same device setting in the app.

- **Default** preserves the existing color. Updating does not enable the automatic theme.
- **Sunrise / sunset** gradually changes only the hue of the unfilled speed arc using the selected reference city, date and local time. Its perceptual lightness and chroma stay matched to the original arc. The filled speed arc, ticks and lower distance arc retain their existing colors.

Set the sunrise/sunset reference city, date/time and UTC offset first. Continuous GPS is unnecessary. If the clock/reference or solar events are unavailable, the default color is shown. Ignition OFF retains the original gray.

<img src="../images/guide-0.15.6/theme-default.png" width="240" alt="Default color"><img src="../images/guide-0.15.6/theme-sunset.png" width="240" alt="Sunset hue example">

The HOME information strip now uses the same 32px numeric face for sunrise/sunset and destination distance as battery and notification counts. The smaller map footer retains its own layout.

<img src="../images/guide-0.15.6/04-solar-font.png" width="240" alt="Sunrise and sunset numbers"><img src="../images/guide-0.15.6/06-destination-font.png" width="240" alt="Destination distance numbers">

## Next twelve hours

1. Use UP/DOWN in HOME to select **Weather**.
2. **Hold O** to open the next-twelve-hours view.
3. Four pages show +1–3, +4–6, +7–9 and +10–12 hours. No footer caption or page counter is displayed. Each larger cell shows source forecast time, icon, temperature and precipitation amount (mm/h). Temperature follows the device unit setting.
4. Use **UP or DOWN** to switch the four pages. The same buttons do not leave Weather while details are open.
5. **Press O** to return to the overview; holding O also closes details. From the overview, UP/DOWN selects another HOME view. Reopening details starts on page 1.

<img src="../images/guide-0.15.12/weather-hours-1.png" width="280" alt="Page 1: next 1 to 3 hours"><img src="../images/guide-0.15.12/weather-hours-2.png" width="280" alt="Page 2: next 4 to 6 hours">

The overview and its two bars still cover **now through +12 hours**. Opening details does not trigger another API request. With weather enabled, a verified NewDoe connection triggers one query as soon as a recent location and API key are available, then sends the result to instrument RAM. Later automatic queries require the instrument Weather view and one hour or 30 km since the last query. Without a connection, automatic weather queries stop; explicit manual refresh remains available. Missing location waits rather than querying an invented position.

**Basic API 2.5** supplies three-hour forecasts. The detail view retains the actual source forecast timestamps, so neighboring cells may repeat a time and value. It does not manufacture hourly precision. One Call hourly data shows individual hourly forecasts. Missing values appear as `--`.

If an older app cannot supply details, the instrument asks you to open Weather in the phone app. Update the app and firmware together.

Images are software renders of the Product LVGL/EVE code, not authenticated live weather or vehicle validation.

The detail page has no high/low temperatures. Blue numbers show precipitation in mm/h. Temperature sits below each icon with its degree sign at the upper right.

A loading icon appears during refresh. The completed forecast replaces the central mm/h caption below the timeline with its age in minutes or hours. The 0 and +12 endpoints and the hourly-detail mm/h units remain. Changed data is delivered to RAM even on other pages; after the initial connection query, periodic requests follow the weather-page one-hour/30 km policy. Disconnected operation permits explicit manual refresh, but no automatic requests. Both bars blend colors between forecast times.

## 0.15.12 — MAP transitions, music loading and information priority

Use app build 161 with the matching included firmware. Install over the existing app without clearing its data.

- Leaving MAP invalidates its display and device map cache, then waits for the display swap before entering the next view. Map tiles and native route coordinates are sent only while MAP is open. TBT guidance continues elsewhere.
- In Music, TBT is followed by artwork and music text. In MAP, TBT and map transfers have priority. Accepted transfers and control/GPS messages retain their ownership and processing guarantees.
- Track handoffs show loading. Returning to the same track reuses cached artwork/text. If the playback session disappears, a short handoff grace period precedes Media Off.
- One-line AUTO prioritizes **notification → call → music → ETA**. With none active, ordinary information rotates every ten seconds. Long UP/DOWN temporarily browses; active priority information returns after twenty seconds.
- Two-line HOME keeps music while playing. Otherwise the lower row shows the selected TRIP and the upper row uses one-line AUTO priority. Manual selections and the navigation-only ETA rotation in MAP/full TBT are retained.
- TRIP km/mi now align with the bottom of the distance digits.
- Hold O in Weather for three hourly columns per screen. UP/DOWN cycles **+1–3 / +4–6 / +7–9 / +10–12 hours**; O returns. This uses the already received forecast without an extra API query. Basic API data retains its actual three-hour source times.

Bluetooth reconnection and proximity detection changes are excluded from this release. The repeated connection attempts reported away from the vehicle are not claimed fixed. Gate, NOR file layout, saved user data and installation/recovery paths are unchanged.

![Forecast hours 10–12](../images/guide-0.15.12/weather-hours-4.png)

Instrument images are software renders of the actual ARM/LVGL/EVE code. Physical LCD/radio/power validation remains separate.

