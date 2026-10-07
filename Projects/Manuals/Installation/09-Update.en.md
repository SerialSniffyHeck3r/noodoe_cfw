# Update an existing CFW · 0.9.61

[Installation index](README.en.md) · [한국어](09-Update.md)

1. Install the current APK over the old app, retaining its data, logs and backups.
2. Select the correct device and stop riding sync.
3. Choose the matching0.9.61 ZIP and start the ordinary Product update.
4. Let transfer, full verification, approval and restart complete. The ordinary payload is384KiB when Gate/resources already match.
5. Confirm only after seeing the actual new CFW screen. Wait for exact-candidate reconnection, at least5seconds of healthy operation and permanent confirmation. The current candidate deadline is up to5minutes; older firmware may report another deadline.

Both APK and Product are needed for the new charging indication. Gate, Bootstrap, stock, resources, Uninstall and Diagnostic are unchanged from0.9.59; a working compatible Gate does not need reinstalling for this update.

Settings, rider name, photo slots, trips and maintenance baselines survive ordinary updates and rollback. Explicit reset/fresh start/full removal are separate actions. A transfer reaching100% is not proof of boot confirmation.

## If interrupted

Reconnect to the same device with the same ZIP and query the last operation. The app may reuse a complete staged image after integrity checks. A lost COMMIT/RESET reply requires reconciliation, not repeated writes. Do not delete app data or select another package to hide an unresolved operation.

Version0.9.59 fixed local approval incorrectly requiring a phone RESET acknowledgment and reduced repeated key writes. When upgrading from0.9.58 or earlier, the old firmware still performs the first handoff. Real storage/key-persistence failures continue to stop the operation. The user's successful stock-to-CFW result does not establish every CFW-to-CFW migration path.

A failed candidate attempts its known-good fallback. If no usable fallback exists, Gate recovery remains. See [recovery](10-Recovery.en.md) and [troubleshooting](11-Troubleshooting.en.md).
