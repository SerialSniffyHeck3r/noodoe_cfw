# Installation connection recovery — 0.9.34

## 0.9.34: resume the same installation after a dropped connection

**Use the matching 0.9.34 APK and installation ZIP.** A temporary transport loss
triggers bounded reconnects, followed by identity, package and saved-position
checks. Authentication failures do not silently erase keys. Check a committed
installation's result before cancelling or retransmitting it.

### Repeated Couldn't Pair

1. Open **Connection recovery → Reset pairing → Open Bluetooth settings** in the app.
2. Forget only this Noodoe in Android settings. Keep the app and its data.
3. On Noodoe's installation, new-version wait or rollback screen, press **UP**.
   Use UP/DOWN to choose your phone or **ALL**, release O, then hold O for a fresh
   two seconds. ALL removes every phone key stored on Noodoe.
4. In a normally running CFW, **Main menu → Settings → Connections → Re-pair phone**
   also clears the device's stored phone keys.
5. Wait for **Ready to pair**, return to the app, select **Reconnect and check**,
   and approve Android pairing. Reconcile installation state before resending files.

Reset **both ends**. Rolling back executable code does not roll Android and device
keys back together. These steps preserve photos, settings, maintenance records
and installation evidence. Returning from Android settings restores the help dialog.

### The previous firmware has not restarted

The app distinguishes an open Bluetooth link to the previous CFW from the new
candidate booting. Check the result first, then resume only when the app permits
it. Updating the APK cannot repair the old firmware that must perform the restart.
If it remains stuck, use the existing keep-data return to stock, then install via
the new package's Bootstrap. See the installation manual's stock-recovery section.
On supported installer screens, release DOWN and hold it for a fresh three seconds
to request keep-data stock recovery. Reconnecting does not renew the five-minute trial.

### An unresponsive menu — new Product only

**CFW 0.9.34 or later, IGN ON:** release all buttons, hold **UP+DOWN together for
10 seconds**, then release every button. This forces an MCU restart independently
of the menu. Unsaved state may be lost; keys are not erased. It requires the CPU
and I/O task to be running. This is different from ↑+O for three seconds, which
restarts the display only. Installing the APK alone cannot add it to old firmware.

### NOODOE INSTALLER and Bluetooth speed

The existing Gate uses `NOODOE INSTALLER` for startup and recovery checks, including
ordinary reboots. The title alone does not mean files are being installed. This
ordinary CFW update does not replace Gate. Bootstrap stays at standard speed;
only Product attempts high speed. Connection diagnostics record actual UART baud
and persisted-key generation. High-speed failure offers O for standard-speed retry
or DOWN for three seconds for stock recovery. UART baud is not measured RF throughput.

See the [change evidence and validation report](../Validation/update-lifecycle-0.9.34.md)
(Korean). Hardware radio validation remains separate from simulation results.

## Earlier version notes


## 0.9.33: result-check prompt after a confirmed installation

If the device is already installed and confirmed but the app still asks for the result, **run the existing result check once using the new APK.** Verification, resume and connection recovery now clear the pending-update state. Do not erase app data or retransmit the whole package for this prompt. Confirmation still checks the device identity, package and durable boot status; a Bluetooth connection alone is not confirmation.

The user reported successful installation and connection with 0.9.32. Version 0.9.33 leaves that Bluetooth initialization and authentication path unchanged. It also updates frame readback and the fuel-warning veil. See the [build and simulation report](../Validation/render-performance-0.9.33.md) (Korean).


## 0.9.32: explicit rate recovery and authentication handling

**Use the matching 0.9.32 APK and installer ZIP.** An APK cannot fix authentication inside an older running firmware. Connected CFW uses the normal update without replacing Gate. If an old trial boot cannot connect, release DOWN and hold it afresh for three seconds to return to stock with data retained, then install the new package.

If high-speed validation fails, **release O and press it briefly to retry at 921,600**, or **hold DOWN afresh for three seconds to return to stock with data retained**. Bootstrap stays at standard speed. Selecting recovery or reconnecting does not renew Product's original five-minute trial deadline.

Immediate 0x05 after accepting pairing is an authentication failure, handled separately from UART speed. The firmware fixes stale authentication state surviving a radio restart and preserves the first event/status as `Pairing failed 3605`. `3318` indicates local policy refusal, `3605` an SSP authentication failure, and `0605` an authentication-complete failure. Keep the complete code. Physical Galaxy pairing with this build has not yet been verified.

For mismatched keys, use Connection recovery: UP on the device, release O and hold it afresh for two seconds to approve removing the selected phone key; remove only this Noodoe pairing in Android settings and reconnect in the app. Keep app data and installation evidence.


## 0.9.31: pairing accepted on the phone, then rejected

Product now retains the remaining pairing window across high-speed fallback and waits for readiness before advertising connections. It also adopts the OEM post-baud settling interval and stops patch transmission when the host baud change fails.

**Use both the 0.9.31 APK and installer ZIP.** An APK alone cannot patch the old running Product. Use an ordinary update from a connected CFW; no Gate replacement is needed. If stuck in an old trial boot, release DOWN and hold it afresh for three seconds to return to stock with data retained, then install the new package.

For mismatched keys, follow Connection recovery: UP on the device, release O and hold it afresh for two seconds to approve removal of the selected phone key; remove only this Noodoe pairing in Android settings and reconnect. Keep app data and installation evidence. Save the device's `Pairing failed (0xXX)` code if it fails again.


## 0.9.30: Bootstrap error 00050002

`262144 / 262144` is the last file operation's progress, not final installation success. This update fixes transient read-only readiness rejection being latched as a permanent error, and UP on Back / Install being intercepted by pairing recovery.

If an old Bootstrap is already stopped:

1. Release DOWN and hold it afresh for three seconds to return to stock with data retained. Alternatively open Back to stock with O, release O and hold it afresh for two seconds.
2. Confirm the stock return in the app to reconcile the previous operation. Keep app data and installation evidence.
3. Use the 0.9.30 APK and ZIP to install the **new Bootstrap**, selecting retained-data restoration.

An APK update or Continue alone cannot patch an already running old Bootstrap. Product remains byte-identical 0.9.29; a working Product does not need reinstalling. Storage protections and real write errors remain enforced. If rejection repeats, save the code and File / phase or command / state details: this code covers more than one possible refusal.


Use the matching 0.9.30 APK and ZIP; Product remains 0.9.29. Ordinary CFW updates keep settings, photo slots and maintenance records. Existing working PC13-display units do not need a Gate replacement for a Product update.

## Reconnecting during installation

A restart normally drops Bluetooth. The app reconnects automatically after 2, 5 and then 10 seconds, with up to eight attempts in one recovery interval. No extra reconnect button is needed for the ordinary handoff. Reopening the app preserves the installation journal; select the same device and ZIP and check its actual state before continuing. An uncertain COMMIT or RESET is not blindly repeated.

After stock hands control to Bootstrap, the app identifies the running role and continues the wizard. The restore-retained-data/start-fresh choice remains yours. The stock NOODOE INSTALLER screen is still part of installation, not a completed Product boot.

## Two different timers

- **Time left 5:00** is the new Product's whole trial-boot deadline. Reconnecting, restarting Bluetooth or reopening the app does not renew it. Until the device reports its deadline, the app labels its own connection wait separately.
- **Checking CFW (5 sec)** is the healthy-run check after screen confirmation on the phone. Bluetooth connection alone does not confirm the installation.

Follow the app's screen-confirmation prompt after the device actually reaches the new firmware. At 0:00, check the real confirmation or rollback result. Older firmware with a three-minute lease is not displayed as a five-minute lease.

Bootstrap waits five minutes for the initial phone connection. Once work begins, that is not a five-minute limit on the entire installation. A disconnect starts a bounded reconnect wait; expiry pauses at a safe write boundary and keeps the prepared data. Use the device's reconnect action to start another wait.

## Couldn't pair, or repeated connection failures

Open **Connection recovery** in the app. Try **Reconnect and check** first. Transient radio collisions receive bounded retries; rejection, cancellation or authentication failure leads to instructions instead of endless pairing prompts.

To set up pairing again:

1. In Android Bluetooth settings, forget **only this Noodoe**.
2. On the Bootstrap or new-version waiting screen, press **UP**.
3. If several phones are stored, choose the target with UP/DOWN. The screen shows the last six address digits. Release O, then **hold O for two seconds** to confirm.
4. Wait for `Pair again on your phone`, return to the app, reconnect and accept Android's pairing request.

The device removes only the selected phone's key, saves the change and then opens pairing again. Settings, photos, installation data and other phones' keys remain. A storage failure is not reported as successful recovery. With no saved phone, the action restarts pairing without deleting any key. The Product trial deadline keeps running throughout this process.

## Return to stock without a phone

On this version's Bootstrap or trial-boot screen, release DOWN and then **hold it for three seconds**. This requests stock restoration with CFW data retained. Active writes finish at a safe boundary first. The action does not depend on a phone or PH9. The existing IGN OFF + O → IGN ON emergency gesture also remains. Older firmware needs its existing Back to stock route.

## Stuck at 5/8 Restarting

The new screen separates image commitment, saving phone pairing and sending the restart reply. Once the restart reply has been sent, a subsequent Bluetooth disconnect cannot cancel the approved restart. Save and response failures remain identifiable in the journal and retained trace.

**The old running firmware performs the restart before the new image can run.** If an older version is stuck at that boundary, check the operation in the app. If needed, return to stock keeping data, install through the new Bootstrap, and choose retained-data restoration. Repeated RESET or re-upload attempts are not the recovery procedure.

## Bluetooth speed and hardware profiles

Bootstrap uses the 921,600baud path only. Product tries 3,686,400baud and falls back to 921,600baud; a successful fallback can complete installation.

The stock assembly selects EVE GPIO7 for display reset on GPIO straps 0–2 and PC13 on straps 3–15. Those paths now cover initialization, shutdown, wake and Gate rendering, with panel selectors 3/4 and the existing BL/recovery identity checks. PCBA text is not treated as a GPIO strap number. New GPIO7-profile installs need the Gate included in this ZIP. Unknown electrical configurations are not made compatible simply by ignoring their identity.

## Screen examples and test scope

These are **software simulation captures** from the compiled Product ARM/LVGL/EVE command and RAM_G path, not LCD photographs. Physical radio performance, Galaxy S24 Ultra pairing and vehicle power interruption remain separate hardware tests.

![Waiting for a phone](images/install-0.9.29/waiting-500.png)
![Pairing recovery](images/install-0.9.29/pairing-confirm.png)
![Restart response](images/install-0.9.29/restart-response.png)
