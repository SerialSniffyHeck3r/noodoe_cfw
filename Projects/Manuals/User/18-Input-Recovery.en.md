# 0.9.40 controls, recovery and updates
These controls require the matching0.9.40 Product and Gate.

| Gesture | Action |
|---|---|
| UP + O together for3seconds | Immediate MCU reset, without waiting for Storage, Bluetooth or Graphics |
| DOWN + O | Quick settings |
| Quick settings: short UP/DOWN | Adjust brightness, or leave Super Nite |
| Quick settings: DOWN for0.8seconds | Super Nite |
| Quick settings: UP for0.8seconds | Reinitialize LCD/EVE while preserving the riding session |
| Quick settings: short/long O | Close/main menu |
| Hold O with IGN OFF, turn ON, keep holding2seconds | Enter the Gate recovery menu |

There is no UP+DOWN gesture. Release every key after restarting. A one-shot reset marker prevents the carried O press from opening recovery or restarting repeatedly. Gate still checks real incomplete installations and damage.

Entering Gate does not restore stock automatically. Release the keys and make a fresh selection. Display-only recovery remains in quick settings; its completion means initialization finished, not that physical light was measured.

![Quick settings — EVE software simulation](../images/quick-settings-0.9.40-simulation.png)

## Update after rollback
Once a confirmed CFW is running and no write/commit is active, the previous failure becomes history and another ZIP can be installed. Neither selecting the failed ZIP again nor waiting for notification acknowledgement storage is required. Short-press and release O to dismiss the rollback notice. Repeated status queries do not reopen the same event.

An active write or unknown state remains protected. The app distinguishes snapshot age, progress age and operation ownership; it does not overwrite BUSY with IDLE. Read-only inspection expires after5seconds in the queue or60seconds total. Connection generations fence stale results. A storage stall with a live Bluetooth link is not a reason to keep re-pairing.

## Gate migration
A compatible installed Gate permits the ordinary Product update. Older Gates require the explicit **keep-data stock restore → new Bootstrap → new Gate and Product** route. A routine update never silently writes Gate. Use the APK and ZIP from the same release.

## A device already stuck on old firmware
The APK cannot replace a stalled old firmware input/storage path by itself. Existing state-query and recovery paths must respond to start migration. If they do not, these new gestures are not installed yet; actual power removal or wired recovery may be necessary. Recovery of the currently reported locked physical unit has not been verified.

Validation uses compiled ARM code and EVE software captures. Physical LCD behavior and repeated wireless installations remain separate tests.
