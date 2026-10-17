## 0.11.6 Bootstrap display and optional re-pairing

**From0.11.4 or0.11.5, use normal CFW update.** Gate and language resources are unchanged. Older Gates still require keep-data stock return and reinstallation using the matching APK/ZIP.

The nine initial file checks use a horizontal loading bar without numbers. During installation, the pale mint upper bar is overall progress; the mint lower bar is the current device operation. The overall bar combines completed stages with the current operation’s fraction, instead of counting that stage as already complete. Percentages are hidden on the Noodoe. The phone uses the same snapshot, subject to polling delay.

Choosing **Install CFW** opens **Reset Bluetooth?**. **Yes is recommended** and clears all saved phone pairings; it is never automatically confirmed. **No** keeps the current pairing. After choosing Yes:

1. Wait for **PAIR YOUR PHONE** on the Noodoe.
2. In Android Bluetooth settings, forget this Noodoe.
3. Pair the same Noodoe again.
4. Return to the app, choose **Check current device → Continue from here**. Do not resend Bootstrap.

The existing connection closes during reset. If CFW configuration storage exists, cleared keys are saved before Bluetooth restarts. On a first installation without that storage, re-pairing is held in the running Bootstrap and the new keys are saved before final installation. Persistence across a power restart before installation is not claimed in that case. Reset/save failures are not reported as success.

The final **INSTALL CFW? / Everything is ready.** screen shows **Back / Install CFW** side by side. Its30-second countdown approves installation through the existing path. UP/DOWN stops the countdown and changes the selection; O executes it. Back cancels; Install CFW approves immediately. Connection loss stops automatic approval.

**Developer’s message** uses the entire circular screen for contiguous text. **DOWN advances one screen, UP goes back one screen**, either end wraps, and O returns to the menu. Original text and font size are preserved; the fixed heading/footer no longer consume reading space.

Bootstrap runs during installation from stock. Existing CFW users need not return to stock merely to see the changed menus. To run this new Bootstrap, use this ZIP during stock installation; normal Product update replaces Product only. Use the matching0.11.6 APK and ZIP.

# Installation, recovery and compatibility ·0.10.1 beta

[0.10.1 languages and installation transition —0.10.1 beta](../User/35-Languages.en.md)

[Manual index](../README.en.md) · [한국어](README.md)

Use the matching **0.10.1 APK and firmware ZIP**. Follow the language/transition guide first: older Gate devices need keep-data stock return and reinstall once. After confirmed Product startup, install the separate language ZIP.

Admitted family: **stock 5.14 or 5.16**. HW, bootloader version, model and PCBA are not whitelisted. AK550 and bench experience do not certify every vehicle or PCB revision. See the compatibility table for evidence and limits.

1. [Compatible models, hardware and versions](07-Compatibility.en.md)
2. [First installation from stock](08-First-Install.en.md)
3. [Update an existing CFW](09-Update.en.md)
4. [Paused installation, stock return and display recovery](10-Recovery.en.md)
5. [Troubleshooting and diagnostics](11-Troubleshooting.en.md)
6. [Connection and pairing recovery](12-Connection-Recovery.en.md)

The current guides above take precedence over versioned historical notes and older screenshots. [0.9.66 controls](../User/23-Controls-Recovery.en.md) · [Release and matching downloads](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/tag/cfw-v0.10.1-multilingual-beta)

[0.9.62 · Dashboard blink alerts](../User/24-Dashboard-Alerts.en.md)

[0.9.63 · Indicators and reconnection](../User/25-Connection-Indicators.en.md)

[0.9.64 · HOME AUTO · manual pairing · background return](../User/26-Home-Auto.en.md)

[0.9.65 · Latest messages · location/music replies](../User/27-Latest-Messages.en.md)

[0.9.66 · Balanced speed ring colours](../User/28-Speed-Ring.en.md)

[0.9.67 · Actual stock mode](29-Stock-Mode.en.md)

[0.9.68 · Sharing, settings and smooth ring](../User/30-Sharing-Settings.en.md)

[0.9.69 · Settings catalog update fix](../User/31-Settings-Catalog.en.md)

[0.9.70 · Ignition-off photo without shading](../User/32-Off-Photo.en.md)

[0.9.71 · Summary contrast, retained settings and oil arc](../User/33-Ride-Preservation.en.md)
