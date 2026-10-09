# Companion 0.9.67 · Actual stock mode takes priority

Update the APK without clearing data. This is an **Android-only fix**: Product remains 0.9.66, and the 0.9.67 installer ZIP is byte-for-byte identical to 0.9.66. An already working 0.9.66 device does not need reflashing.

Device inspection, connection recovery, Bootstrap connection and pairing verification now prioritize a valid stock identity from the selected Bluetooth address. The app returns to stock setup and clears old Bootstrap waits, pairing recovery, migration prompts and its update lock. Recognition requires no ZIP or historical phase check. The selected ZIP and historical evidence are kept. Actual installation still requires stock 5.14/5.16, without a hardware, bootloader-version, model or PCBA whitelist, plus fresh device and stationary IGN ON checks.

Separate saved VERIFY/STOCK pairing steps are merged into one connection check, including old preferences. Inside the pairing guide, **Check current firmware** works without completing the old reset steps. Actual Bootstrap still requires manual phone unpairing and device pairing reset, followed by secure connection and saved-key verification.

Stock detection archives earlier attempts but does not certify the success or cancellation of old flash/NOR writes. It sends no firmware, erase or reset commands. A stale install action only observes stock; start a new installation explicitly afterwards. Silence, rejection or a mismatched Bluetooth address never counts as stock.

For the reported failure, install APK 0.9.67 on the same phone and use **Check current firmware / Check device**. The diagnostic shows repeated secure connections with no NDCP identity response, but lacks the initial Bootstrap transfer failure. This fixes the stale phone-side recovery loop; it does not establish the original radio/handoff cause.

If Stage 6/4 requests a manual reset, hold **UP + O together for 3 seconds**, release and let the phone check the result. The device reset limitation remains. F4 stock 5.14/5.16 admission does not physically qualify every vehicle.

Validation: real stock framing/NDCP simulations, journal and UI workflow tests, APK signature, actual ZIP importer and Product hash. No physical phone, Bluetooth, LCD or vehicle test.
