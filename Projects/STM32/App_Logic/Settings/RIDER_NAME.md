# Rider name

`Rider_Name.h` owns this app preference; Graphics receives an owned copy in
`PowerViewModel`. Future phone transport must reassemble/validate its command
before calling these APIs. No SPP opcode or successful wireless transfer is
claimed by this implementation.

```c
/* Immediate session setting from a length-delimited phone payload. */
if (!RiderName_Set(payload, payload_bytes)) { /* reject invalid name */ }
```

Payload is at most48 UTF-8 bytes, excluding NUL. Empty clears the name. Invalid
UTF-8, surrogate/out-of-range scalars, embedded NUL, C0/C1 controls and Unicode
line/paragraph separators are rejected without changing the current name.
Calls copy the packet before returning; no heap allocation or borrowed pointer.
Set is task-only and returns1 on acceptance. Get requires49 output bytes.
Session state survives IGN and retained STOP; it does not survive MCU reset.

For persistent names use `SettingsService_RequestRiderName(payload, bytes, &id)`.
It queues a bounded copy to the existing StorageTask/journal. Wait for the
matching `SettingsService_GetDiagnostics()` completed_request and inspect
completed_result. `SETTINGS_OK` from the request means queued, not saved. After
a successful commit, use RiderName_Set for the current session if it already
has an override. Without an override the getter follows committed settings.
The companion must serialize its settings requests and observe completion.

A valid existing provisioning record is required. NOT_PROVISIONED is an error
for durable settings, not a reason to format/create a partition. Caller may
explicitly choose a session-only Set instead. Storage failure preserves the
old committed name. No phone pairing is necessary to display the saved name.

Settings schema3 is312bytes: existing fields0..255 preserved; UTF-8/NUL name
256..304; zero padding305..307; CRC308..311. Schema1/244 and2/260 are read with
an empty name and no boot-time write. Existing rotating-journal commit/readback,
UID/partition checks and permissions are unchanged. Old firmware cannot read
schema3; retain the prior NVM backup for downgrades. No device migration has
been performed merely by building this version.

Welcome uses two centered32px lines at the existing positions:

```
Welcome
Rider Alex
```

With no name the second line is just `Rider`. Keep the name's spelling/case.
Names exceeding the288px line are ellipsized only in the presentation copy;
font size, geometry and the complete stored string remain unchanged. The
current Lato asset contains ASCII only: unsupported UTF-8 scalars display as
one `?` each, while their original bytes remain intact for a later language
pack. Korean/emoji glyphs are not added here. Welcome timing and eligibility
remain the existing cold/dark-panel wake policy; renaming never opens Welcome.
