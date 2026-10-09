# 0.9.64 · HOME AUTO, manual Bootstrap pairing and background return

> Pairing instructions below describe historical 0.9.64. In 0.9.72 the mandatory wizard is removed; use [current connection recovery](../Installation/12-Connection-Recovery.en.md). HOME behavior is unchanged.

> Historical 0.9.64 behavior; notification admission is superseded by [0.9.65](27-Latest-Messages.en.md).

HOME automatic modes show a newly registered notification for **20 seconds**, then keep playing music visible. Without playing music, date, phone and compass information cycle. Hold O to skip the notification or cycle music/information; automatic priority resumes **20 seconds after that long press**. Notification popup settings do not suppress the HOME notification. Dismissing a popup does not dismiss its HOME content. HOME uses the Material Round bell and the same scrolling text style as HOME music.

While one notification is being rendered/transferred, new notification arrivals are ignored by the companion until both the detail card and the initial HOME scrolling tiles are registered on Noodoe. They remain Android notifications. Reconnecting does not replay old tray entries as new HOME alerts. Ordinary no-music information cycles every ten seconds; the twenty-second intervals apply to new alerts and manual override.

When map display is disabled or no usable map texture covers the current location, the trail uses a moving, world-anchored grid and a red line. Cached road maps and their existing zoom/prefetch behavior remain. The average and peak speed markers are hidden at or below **5 km/h**. IGN OFF makes the speed ring and markers neutral gray; IGN ON restores their normal colors.

For first installation, after Bootstrap appears, **manually disconnect and forget this Noodoe in phone Bluetooth settings**. On the Bootstrap connection screen press **UP**, select this phone, release O, then hold **O for two seconds** to delete its key on Noodoe. Pair again and use the app's verification step before continuing. Do not resend Bootstrap. The app verifies that a new pairing key was durably saved. Bootstrap-to-CFW handoff and ordinary CFW-to-CFW updates do not acquire this reset step. Already accepted installations continue to result confirmation.

Background return adds an Android inexact wake retry and companion-device/connection signals when the selected device has been associated. Manual disconnect, unresolved installation and pairing recovery still stop automatic riding. The wake retry is not an exact one-minute guarantee: Android Doze and manufacturer restrictions can delay it. Closing the app screen differs from Android Force stop, which requires opening the app again. Welcome light behavior on a returning vehicle still requires physical testing.

Use the matching **0.9.64 APK and ZIP**. First-install admission still accepts stock **5.14 or 5.16**, without HW, bootloader-version, model or PCBA whitelists. Vehicle wiring compatibility is not inferred from that admission. Stock recovery uses the bundled 5.16 image. Stage 6/4 manual-reset behavior is not claimed fixed: follow the displayed instruction to hold UP+O together for three seconds, release, then let the app check the result. Gate and Bootstrap binaries are unchanged.
