# Paused installation, stock return and display recovery

**APK / Product 0.9.62 · Manual revision 2 · 2026-10-07**

[Installation index](README.en.md) · [한국어](10-Recovery.md)

## Choose the path for the actual device state

| Current state | Recovery path |
|---|---|
| `INSTALLATION PAUSED` with the manual-reset instruction | UP+O restart below, then inspect the same operation |
| Only the app says success or cancellation is unconfirmed | Inspect the same device/ZIP's last operation first; determine whether the device is still writing |
| CFW is running but LCD output is wrong | Display reinitialization from quick settings |
| CFW boot/update needs recovery and the phone cannot connect | Enter independent Gate with the physical buttons |
| Returning a working CFW to stock | App stock return; keep data is the default |
| Pairing is approved but connection still fails | [Connection recovery](12-Connection-Recovery.en.md) |

## 1. Manual restart from the installation error screen

The 0.9.62 Product condition that previously displayed `Paused. See phone help.` now shows the following. Stage and Code/phase remain visible.

```text
Please manually reset.
Hold UP + O together
for 3 seconds.
```

1. Record the screen, Stage and Code/phase, and save app diagnostics if available.
2. Release every held button, then **hold UP and O (centre) together for 3 seconds**.
3. Release both when the device restarts. If IGN was OFF, turn it ON as requested by the new screen/app.
4. Reconnect to the same device with the same ZIP and inspect the last operation. If the new CFW is actually running, complete screen approval and permanent confirmation. If Gate appears, follow its menu.

This restarts the MCU; it does not declare installation successful. If the error recurs, retain Code/phase and the diagnostic ZIP before choosing another recovery route. Do not force-reset normal transfer, erase or verification merely because it takes time. The first handoff of an update started on older firmware uses that older code and wording.

![Installation error instruction — software rendering, not a device photograph](../images/restart-paused-0.9.61-simulation.png)

Code 5 / phase 302 in the picture is a test example; it does not establish the same cause for actual Code 6 / phase 4 or other errors.

## 2. Enter Gate without the phone

On current compatible Product/Gate, use **IGN OFF → press and hold O → turn IGN ON while holding O → keep holding for 2 seconds after ON**. Release when recovery appears. Do not hold UP or DOWN. Older versions may use different timing.

In `RECOVERY MODE / Ready for the rescue.`, choose with UP/DOWN. Select `Back to stock` and, when requested, **release O and hold it freshly for 2 seconds** to approve. Entering the menu does not itself approve restoration. Gate checks the stock image, stages it in NOR, verifies the written bytes and starts the stock writer.

| Gate menu | Stock-return approval | Help |
|---|---|---|
| ![Menu](../images/gate-menu.png) | ![Approval](../images/gate-confirm.png) | ![Help](../images/gate-help.png) |

**Gate has no Bluetooth.** Use its local menu rather than waiting for phone connection. Button-operated stock recovery keeps CFW data and does not automatically perform full removal.

## 3. Stock return versus full removal

| App choice | Action and retained data |
|---|---|
| Return to stock · keep CFW data | Gate restores the stock APP; CFW settings/photos/data files remain for a later compatible reinstall |
| Fully remove CFW data | A dedicated uninstall image requires local approval, removes verified CFW-owned content/allocations and restores stock |

Stock photos, factory identity and stock files are preserved. Keep-data return does not restore every NOR byte to factory condition. The uninstall image is a local operation without Bluetooth; an unchanged phone progress display does not prove it stopped.

After the actual stock screen appears, choose **Stock restored on device · verify** in the app. A fresh stock response reconciles the journal and confirms return. On reinstall, choose **Keep stored data / Fresh start** deliberately. Re-pairing may be needed if stock and CFW Bluetooth keys differ.

## 4. Reinitialize only LCD/EVE

In normal CFW, use **O+DOWN to open quick settings → release the buttons → hold UP for 0.8 seconds**. This reinitializes the display while retaining the running session. It is different from an MCU restart, stock return or firmware reinstall. Read the result if the display request is rejected.

## Power and recovery limits

IGN OFF is different from disconnecting permanent 12V. Disconnecting the module/battery may require bodywork removal. The vehicle rocker cannot press UP and DOWN simultaneously; no such combination is required. Button recovery is not guaranteed for every CPU, storage or recovery-image failure. Diagnostic is a separate investigation image, not a substitute for normal installation confirmation. See [logs and error interpretation](11-Troubleshooting.en.md).
