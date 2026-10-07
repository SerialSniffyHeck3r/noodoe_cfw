# First installation from stock · 0.9.60

[Installation index](README.en.md) · [한국어 illustrated steps](08-First-Install.md)

## Prepare

Use the matching 0.9.60 APK and installation ZIP. Install the APK over the existing app to retain logs and backups. Keep the ZIP intact; do not choose a BIN from a stock dump. Select the correct Noodoe and stop riding sync. Keep the phone and stationary vehicle adequately powered, leave the installer running and follow the requested ignition actions. Do not operate the starter or force-stop the installer during writing.

First-use admission accepts **F4/HW0, stock5.16 and bootloader0.10–0.19** without a model-name or PCBA whitelist. The app still binds the actual device UID/version/hash and stores a double-read bootloader capture. Allowed versions are not a claim that every hardware revision has been physically tested.

IGN and permanent12V are different: installation can remain active with IGN OFF. Switching IGN is not equivalent to disconnecting module power.

## Follow the roles in order

1. **Stock → Bootstrap:** the app identifies stock firmware and sends the installation tool over the stock protocol. Follow the device prompt for the handoff. A temporary Bluetooth disconnect during restart is expected.
2. **Bootstrap:** look for FuckNudo Bootstrap / Welcome. Approve an Android pairing prompt if shown. The app verifies the actual role, image and device identity; connection alone is not installation success.
3. **Storage audit and backups:** the app verifies FAT allocations/free space, preserves original metadata and captures the device-specific lower flash. It prepares the stock recovery APP, CFW resources/settings/photos/logs and update A/B slots. These slots are firmware candidates and a known-good fallback, not two photo slots. An ordinary first installation does not require downloading the entire128MiB NOR image.
4. **Transfer and verify:** the app writes and reads back the necessary files and compares the final layout. The current byte count belongs to that operation; overall stage progress is not a time estimate.
5. **Local approval:** follow Install CFW? on Noodoe. Release any held key first, choose the requested action and confirm as shown. Once committed, installation may continue without the phone. A phone error popup is not permission to resend the whole installation.
6. **Install and restart:** NOODOE INSTALLER is the stock writer, not the final CFW screen. Let the role transition complete and the app reconnect.
7. **Candidate confirmation:** when the new CFW UPDATE screen actually appears, use the app screen-confirmation button. The app checks the same device/candidate and healthy execution for at least5seconds. Current firmware has a device-owned confirmation window of up to5minutes; older running firmware reports its own deadline. Approval does not extend it indefinitely.
8. **Complete:** wait for the app to confirm the candidate permanently. Failed confirmation uses a known-good rollback if available; first installation without a previous CFW may enter independent recovery instead.

## Cancellation and reinstall

During a cancellable Bootstrap session, release O and hold it freshly for3seconds to request safe cancellation. A write may need to reach its safe boundary. Committed work is not forcibly undone. IGN OFF alone does not cancel installation.

Ordinary CFW updates preserve settings/photos/trips. First install and an explicit fresh start initialize their new CFW data. When reinstalling after a keep-data stock return, choose retained-data restoration or a fresh start deliberately. Keep logs and the same ZIP if a result is unknown; use [troubleshooting](11-Troubleshooting.en.md).
