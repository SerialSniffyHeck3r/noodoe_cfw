## 0.11.7 Bootstrap pairing handoff

**Use the matching 0.11.7 APK and ZIP. From 0.11.4–0.11.6, normal CFW update is available.** Gate and language resources are unchanged. Older Gates still use the existing keep-data stock-return/reinstall route.

During stock installation, **keep the existing Android pairing** and choose **Install CFW** in Bootstrap. Bootstrap now loads the saved stock link key before starting Bluetooth and carries it into CFW through the verified settings journal. A normal key handoff skips the reset question. Do not delete the pairing as a routine installation step.

If the key is missing or connection still fails, manual recovery remains available. In the Bootstrap connection-wait screen, press **UP**, choose the phone or all pairings, then release O and hold O for two seconds to confirm. If **Reset Bluetooth?** appears directly, **Yes** clears saved pairings and **No** keeps them; neither is automatically selected. After clearing:

1. Wait for **PAIR YOUR PHONE**.
2. Forget this Noodoe in Android Bluetooth settings and pair it again.
3. Return to **Check current device → Continue from here**. Do not resend Bootstrap or clear app data.

Previously re-paired or deliberately deleted CFW keys are preserved when the stock key has not changed. The stock NOR record is read only. Before final installation, the current key must be durably saved and verified for Product to reuse it. On a first installation without CFW settings storage, a manual re-pair remains session-local until that final save; an earlier power restart may require re-pairing again.

The two synchronized progress bars, full-screen developer message and final **Back / Install CFW** choices with the existing 30-second confirmation remain unchanged. Bootstrap runs only during stock installation: a normal CFW update does not replace an already-running Bootstrap. To test this fix, the stock-to-Bootstrap step must use the new ZIP.

Software tests replayed the stock 5.16 bond routines, key authentication commands, journal interruptions and both handoffs. Physical radio testing and the unavailable stock 5.14 binary are not covered by that evidence. If a phone has already forgotten its key, firmware cannot recreate the phone-side copy: pair once using the fallback above.

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
