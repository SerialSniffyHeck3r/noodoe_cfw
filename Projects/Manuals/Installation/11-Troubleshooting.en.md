# Installation and everyday troubleshooting · 0.9.60

[0.9.61 manual reset and ODO controls](../User/23-Controls-Recovery.en.md)

[Installation index](README.en.md) · [한국어](11-Troubleshooting.md)

Record the APK and installed CFW versions, actual device screen, stage, code/phase, last file/sector/offset, ignition state and buttons used. Export the app diagnostic ZIP before clearing anything. Total stage progress, bytes sent, device verification and boot confirmation describe different work.

| Symptom | Meaning and next action |
|---|---|
| Compatibility stopped before transfer | Use matching current APK/ZIP and check F4/HW0, stock5.16, BL0.10–0.19. Model and PCBA are not restricted. This check has not sent firmware |
| The current operation cannot continue / Installation success or cancellation not confirmed | The last change has no confirmed outcome. Select the same device/ZIP and query that operation before retrying |
| Stage6/8, Code6/Phase4, Paused or See Phone Help | The code alone proves neither a brick nor success. Preserve it, use the app status/recovery flow and export diagnostics. Actual persistence/reset preconditions may still be failing |
|100% transfer or NOODOE INSTALLER | Transfer completion and the stock writer are not candidate-boot completion. Wait for the actual CFW screen before approving it |
| Bootstrap cannot connect | Confirm the role/address and approve Android pairing. If Bootstrap is already running, connect/identify it instead of retransmitting from stock |
| Gate has no Bluetooth | Expected. Follow its local button menu |
| Checking the new version / CFW UPDATE | Confirm the actual candidate screen in the app, reconnect and complete at least5seconds of health checks within the device-reported deadline (current maximum5minutes) |
| Stock screen but app waits for CFW | Use the app stock-return confirmation to read real stock identity and reconcile the journal |
|00050006 or a file-validation error | Preserve file number, phase and offset. This is not a blanket instruction to format NOR; photo container and allocation checks have distinct causes |
| Update was not confirmed | Boot confirmation failed. Check rollback/Gate state; it is different from an interrupted file transfer |
| Buttons do not operate normal pages | Check the dashboard/Noodoe selector and blue dashboard badge, release held keys and consider stationary/permission restrictions |
| GPS waiting or no map | Check precise location permission, service status, fresh fixes, imported region and map enablement on both sides. Cold or missing tiles still need loading |
| Album art appears late | Check notification access, the active player MediaSession and phone background restrictions. Current-track artwork is retained; new/missing/evicted content needs preparation or upload |
| Storage error while UI still works | Read Settings/Ride records status and last successful save. Request Retry failed saves once and inspect the result; a live display is not proof of a durable write |

## The update handoff fixed in0.9.59

The user reported a successful stock-to-CFW installation without the previous6/4 symptom and working fuel-warning dimming. The fix separates local approval from a remote RESET acknowledgment and avoids dirtying unchanged Bluetooth keys. An upgrade starting on0.9.58 or earlier still executes old code for its first handoff. Genuine key/NOR write failures are not bypassed. Available diagnostic logs did not prove that every reported Code6/Phase4 had the same cause.

## Keep evidence while recovering

The app's connection-state reset clears sockets and temporary selections; it does not send an automatic device ABORT/RESET or declare an unknown operation cancelled. Logs, journals, backups and downloaded ZIPs remain. Do not clear app data, format NOR, swap packages or overwrite device identity to bypass uncertainty.

IGN OFF is not permanent-power removal. Current Product's UP+O3seconds restarts the MCU; quick settings (O+DOWN), then UP0.8seconds only recovers the display. Use [Gate/stock recovery](10-Recovery.en.md) when indicated by the device state. Do not repeatedly send destructive requests after a lost response.

Share the diagnostic ZIP with the issue details. Normal event logs omit message bodies, track titles, precise coordinates and pairing secrets, but device identifiers and recovery evidence still deserve care. Internal originals remain in the app. Unknown radio/device failure cannot be diagnosed solely from a progress percentage.
