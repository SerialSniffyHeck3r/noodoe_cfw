# Stock return and recovery · 0.9.60

[Installation index](README.en.md) · [한국어 illustrated guide](10-Recovery.md)

| Choice | Result |
|---|---|
| Return to stock, keep CFW data | Gate restores the stock APP; CFW settings/photos/data files remain for a later compatible reinstall |
| Full CFW removal | A dedicated local uninstall image verifies ownership, removes CFW content/allocations and restores stock |
| Emergency Gate stock return | Keeps CFW data; entering the menu does not itself authorize restoration |

Stock files, factory identity and stock photos are preserved. A stock return does not restore every NOR byte to factory condition. Full removal does not delete files solely because their names look familiar. The dedicated uninstall image has no Bluetooth; a missing phone progress report is not evidence that it stopped.

## Enter Gate without the phone

Turn IGN OFF, hold O, turn IGN ON while holding O, and keep O held for2seconds on a current compatible Product/Gate. Release keys when the recovery screen appears. Older versions can have different timing. In RECOVERY MODE, choose Back to stock and, when requested, release O then hold it freshly for2seconds to approve. Gate verifies the recovery image, stages it in NOR, verifies the staged bytes, and starts the stock writer.

Gate has no Bluetooth. Use its on-device buttons; Bootstrap and Product are the Bluetooth-enabled roles. UP and DOWN cannot be pressed together on the vehicle rocker. IGN OFF does not disconnect permanent module power, and battery disconnection may require bodywork removal.

## Restart versus display recovery

- UP+O for3seconds requests an immediate MCU restart in current Product. Release both afterward.
- O+DOWN opens quick settings; UP for0.8seconds there reinitializes only LCD/EVE and preserves the running session.

These are different actions. For an installation with an unknown write/commit outcome, first preserve its screen/code and query the last operation. Do not use force reset as a universal substitute for a confirmed update result.

After returning to stock, use the app's stock-return confirmation so it reads actual stock identity and reconciles its journal. Re-pairing can be necessary if the CFW and stock Bluetooth keys differ. A reset of app connection state alone does not prove stock restoration.

On reinstall, choose retained data or fresh start explicitly. Diagnostic is a separate image for investigation, not a riding firmware or a workaround that approves a failed candidate. [Troubleshooting](11-Troubleshooting.en.md).
