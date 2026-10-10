# 0.9.43 incoming calls and automatic brightness

[0.9.73: caller identity and automatic riding reconnect](34-Call-Reconnect.en.md)

[User manual](README.en.md) · [한국어](21-Calls-Backlight.md)

Open **Permissions / setup** in the app and allow **Detect incoming calls** and
**Answer and end calls**. During riding sync, incoming calls appear over the
current page. The ringing notice and controls appear before the caller image.

| Control | Action |
|---|---|
| Short O | Dismiss the popup only; do not reject the call |
| Short DOWN | Answer |
| Hold UP for 0.8 seconds | Reject or end |

The current menu and selection stay in place. Audio stays on the phone/headset.
Android 8 supports detection and answering; use the phone to end when no supported
end-call API is available. Keep companion-device registration enabled on Android
12+ where available. The app's first help topic explains these controls too.

Automatic LCD brightness now increases in bright conditions and decreases in
dark conditions. Manual brightness, SUPER NITE and welcome-light policies remain.

![Incoming call — EVE software simulation](../images/incoming-call-0.9.43-simulation.png)

This is a software preview. Real cellular/headset operation and LCD FPS have not
been measured for this release.
