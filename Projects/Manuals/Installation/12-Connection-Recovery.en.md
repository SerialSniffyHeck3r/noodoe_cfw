# NewDoe 1.0.1.0

Install app build 173 over your existing NewDoe app, then update the included firmware. Do not uninstall the app or clear its data. This release fixes the on-device Bluetooth re-pairing flow; an ordinary firmware update does not erase pairing, preferences, maps or ride records.

- Re-pair confirmation, waiting and failure screens now own both the display and buttons. Holding O no longer falls through to the Settings Back action. These waiting screens keep the existing ignition-off sleep policy.
- Re-pairing requires a fresh local O hold. A button already held when the screen opens, or held behind another operation, cannot authorize deletion. Repeated open requests cannot interrupt a running repair. Competing Bluetooth settings commands are rejected until the repair is finished or cancelled.
- The radio closes before deleting the selected bond. Key deletion must be saved before Bluetooth starts again; the screen says to pair only after startup and the pairing window are active. Another saved phone connecting does not falsely complete a selected-phone repair. Storage failure is reported, and a new explicit repair can retry the save. A rejected Bluetooth start is also bounded by the recovery deadline; it cannot wait or retry forever.
- Short O cancels confirmation or leaves the waiting/failure screen. It does not interrupt a key write. The existing stock-recovery gesture remains available.
- Recovery text and repeated display code are compacted. Gate, Diagnostic, compiler policies, memory reservations, artwork and unrelated riding functions are retained. Existing app privacy/location-disclosure work is included.

## Re-pair a phone

1. While stopped, open Settings > Connections > Re-pair phone, select Apply and press O briefly.
2. Wait for **RE-PAIR PHONE**. UP/DOWN selects a saved phone by its address suffix, or the explicit **Forget ALL phones?** choice. Choose only the intended phone unless you deliberately want to remove every stored phone bond.
3. Release O completely, then press and hold O for two seconds on that screen. Do not hold DOWN; its three-second action is stock recovery.
4. Wait for **Pair on your phone.** On the phone's Bluetooth settings, forget the old Noodoe/NewDoe entry if it is still saved, then pair with the intended device again. Return to NewDoe and check the current device status.

The dashboard repair intentionally removes only the selected Bluetooth bond, or all bonds if you explicitly choose ALL. It is not a settings/factory reset. If an older firmware cannot open this screen, this correction is not active until its firmware has been updated; repeatedly holding O or clearing app data does not install the correction. Use the documented connection/recovery path for the installed version.

Software/emulator validation only; no new physical phone, vehicle or RF test. The previously accepted old-firmware stock-return power-interruption limitation remains, and Google Play signing migration is separate. Existing lint findings are disclosed in verification.json.



[English guide](../Guide.en.md#connection) · [한국어 설명서](../Guide.md#connection)
