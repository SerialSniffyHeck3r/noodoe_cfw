# Bootstrap / original-APP recovery

## Bootstrap welcome (2026-09-22)

The first usable display frame shows `FuckNudo Bootstrap` and `Welcome` on
two centred lines. This 1.5-second presentation overlay does not block NOR,
radio or RAM initialisation, background checks, watchdog work or protocol
processing. A short button press dismisses it without selecting a hidden menu
item. Errors, recovery and an active installation take priority. Returning to
the menu after cancellation does not replay the greeting.

Bootstrap's normal and early-recovery display titles use the same branding.
Wire identity, package matching and the independent Gate/Product payloads are
unchanged. Android 6.4 can use the refreshed ZIP; an already running Bootstrap
must still match the ZIP it was installed from.

This is an **APP at 0x08010000**, not a replacement bootloader. The currently
approved target has resident metadata version0.14 and the immutable32KiB
boot-code SHA pinned in `Recovery_Core.c`. An existing phone log reports0.15
on another device; that device is outside this build's verified compatibility.

The standalone build uses `tools/bootstrap_build.py`, the installed CubeIDE
compiler, normal generated HAL/RTOS/clock sources and `Linker/Noodoe_APP.ld`.
It does not switch the Eclipse project away from Product. Main's default task
still calls only `LCDTest()`; this profile supplies an independent installer
implementation instead of Product UI.

Bootstrap contains the approved V5.16 APP as raw DEFLATE, with a CPU-only32KiB
CCM dictionary. Recovery validates the decompressed full448KiB SHA before
writing any staging bytes. The later Product build uses the independent
`CFWREC.DAT` source. Both use the same bounded recovery core and stock resident
installer. No fault/timeout silently installs stock.

## Ownership

- BootstrapTask exclusively owns storage, provisioning and updater processing.
- BTstack retains its existing separate radio task. The installer accepts one
  primary-phone session, requires an encrypted link and explicit local intent.
- NDCP is stop-and-wait: one outstanding request. Pipelining is unsupported.
- Replies stay owned until the transport queue drains. Reset acknowledgement
  is recorded after that drain, not after enqueue.
- Storage inspection/provision and OTA cannot concurrently own their buffers
  or NOR. Read-only SWD runs on the same owner and blocks physical writes.
- The REC-only SWD installation mailbox is published after the startup audit
  finishes, so audit reads cannot overwrite an in-flight host upload.

## Physical operation

The motorcycle keeps permanent12V connected; IGN is not a cold-reset signal.
Bootstrap and Product layout2 use the shared GateGesture policy: observe IGN OFF
for200ms, hold ENTER for at least500ms (the UI asks for1 second), then IGN ON
while holding ENTER for another2 seconds. Release cancels; authorization expires
at30 seconds. An already-held ENTER at reset enters WAIT, never automatic restore.
Runtime drains physical work, refuses new writes, then resets to minimal recovery.
A committed/ambiguous update cannot be discarded as an ordinary cancellation.

`BSP_BootSafetyStart` starts IWDG before ordinary normalization. After C/CCM
initialization, `BSP_ApplicationEarly` routes watchdog/fault/explicit intent or
held ENTER to the HSI-only rescue before main, HSE/PLL, RTOS, BT or SDRAM.
The independent rescue uses Gate board polling and embedded DEFLATE stock data.
It does not start Bluetooth. Watchdog reset waits for a NEW local gesture.

Bootstrap restore confirmation (2026-09-21 user correction): on Back to stock,
release O, then hold O continuously for2seconds. No IGN cycle is required.
The same confirmation applies in Bootstrap's early rescue screen. A held key
on entry cannot authorize a restore; a fresh release/press is required. Partial
holds cancel immediately. Runtime confirmation drains the existing storage
owner before resetting into the same verified embedded-image restore path.
Independent RecoveryGate's emergency IGN gesture is unchanged. Bootstrap also
retains that emergency gesture outside its explicit restore page.

The root menu has only Bluetooth test, Install CFW, and Back to stock. Errors
use the separate red SYSTEM ERROR / aw shit :( screen. Bluetooth test shows
controller state, encrypted SPP state, and actual NDCP request/reply counts;
it never reports a ready controller as a completed radio test. O returns to
the root menu. A broken radio can still be inspected and restored offline.

Selecting Bluetooth test or Install CFW opens a120-second pairing entry
window. Test entry permits only identity and status requests (01/58/5A/45);
it cannot grant storage/update/restore commands. Installation requires a
separate local Install CFW entry, and returning to the menu revokes that lease.
The admitted encrypted installation session keeps its lease through a long backup;
disconnect, IGN OFF or lost encryption revokes it. After all files and staged
Gate+Product bytes are verified, Back is selected by default. Choosing Install
binds permission to that exact transaction/session. New BEGIN revokes it.

The UI has CHECK/READY/CONNECT/WORK/INSTALL_READY/INSTALL/PAUSED/RECOVERY/ERROR
states, plus the appended BT_TEST state9. Existing state numbers are unchanged.
ROM fonts render friendly English without Product assets. 0x5A publishes
the snapshot; it is not a new wire protocol. Storage/install status is authoritative,
and completion of transfer is never described as successful installation.

The new Android fork is `NoodoeInstaller/Android`; OpenNoodoe is untouched by
this change. Its common NDCP client/transport is reusable by a future Product
client, whose normal application commands are deliberately not connected yet.

The bench-only `bootstrap_recovery_install.py` uses physical SWD access and
explicit backup/UID/image checks to create only `CFWREC.DAT`. It does not
constitute a radio installation test. `bench_app_install.py` similarly is an
APP-only wired entry/restore tool, not the stock SPP updater.

## Evidence and limits

2026-09-21 bench evidence is in
`Reversing/analysis/2026-09-21-bootstrap-bench/REPORT.md`. The minimal menu was
installed and the embedded stock restore actually ran through resident0.14;
three independent full reads matched V5.16. A deliberate watchdog-feed failure
caused a real IWDG reset into early WAIT without automatically installing stock.
Bootstrap was then reinstalled. BT still reports controller startup timeout0x302
on this damaged donor; these wired tests do not establish a working radio,
physical-button gesture, or completed Gate+Product installation.

Two integration defects found on hardware are now covered by regression tests:
the pre-owner Bluetooth clock must use hal_time_ms(), and IWDG must be started
before waiting for its prescaler/reload register updates. The updated code is
shared with Product and Gate where applicable.

Detailed contracts: `Middlewares/Noodoe/Storage/BOOTSTRAP_WIRE.md`,
`App_Logic/Recovery`, and the local report directory
`Reversing/analysis/2026-09-20-bootstrap-recovery`.

For an already provisioned bench whose resource A package belongs to an older
CFW, physical SWD command8 can fill an entirely erased B slot. This is separate
from NDCP create-only file commands. `bootstrap_resource_install.py` requires
fresh complete A/B NOR backups, exact live ELF/UID and physical preimages. The
device audits FAT ownership and hashes the old A package before offering
PREPARED. The new required package's body and incomplete header are physically
read back and hashed before its completion word is programmed last. FAT and A
are never written. A nonempty B, interrupted prior update or failed readback is
not automatically replaced. Final whole-NOR verification still precedes CFW
installation; intermediate expected postimages are not new downloaded backups.

An APP-level recovery cannot run after broken vectors/early startup, failed
internal-flash installation, or damaged resident boot code. Those cases still
require a physical recovery path. First FAT file creation is not power-loss
atomic; original files are never automatically formatted/repaired/replaced.
The bench radio is faulty, so wireless and the undamaged vehicle require
separate actual verification. Android's strict target-bound bundle cannot be
built for an unidentified or mismatched bootloader by guessing its fields.
If the APP cannot service the runtime chord or reach early recovery, IGN cycling
alone is not an independent reset/rescue mechanism. Also, a previously committed
stock-loader update is processed by the resident before any APP recovery hook;
the local software reset does not override or erase that pending transaction.
