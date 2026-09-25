# 2026-09-02 gallery, creation, and media field test

## Scope and evidence

This note correlates the AK 550 dashboard observation with the OpenNoodoe 0.4.1
application log, the preserved official Noodoe 2.1.14 APK, and the V5.16
firmware command-handler analysis. The live log is preserved at
`captures/20260902-field-gallery-theme-media/opennoodoe.log`.

Evidence labels used below:

- **Confirmed on vehicle**: both a protocol result and the rider's dashboard
  observation are available.
- **Confirmed in log**: the SPP request/reply sequence is complete, but this
  does not by itself prove successful rendering.
- **Static evidence**: present in the official APK or firmware, not yet replayed
  with a known-good bundle on this vehicle.
- **Inference**: best explanation supported by the preceding evidence.

## Results

### Gallery slot 6: confirmed success

Task 6 negotiated location `0x0600`, sent one 24,515-byte file in chunks of
11,816 + 11,816 + 883 bytes, received status 0 for every data packet and
terminate, and received status 0 for task DONE. The log ends the operation with
`GALLERY slot 6: complete` at line 10415. This validates the current padded CRC,
`0x0D` chunk/cumulative parsing, and the final slot byte for inserting into an
unused slot.

This result does **not** validate replacement of an occupied slot or deletion.
No `CONTENT_REMOVE` request exists in this capture. `removeContent()` exists in
the service, but the current UI does not invoke it. Therefore the reported
delete failure is currently an unimplemented/unexercised path, not a dashboard
rejection captured on the wire.

### Clock creation `0x0200`: stored, then retry was corrupted by a late reply

Task 3 successfully negotiated, reused the already-present 179-byte cfg, sent
the 12,182-byte JPEG, and received successful file replies. Its DONE sequence
was acknowledged at 03:14:43.827, but OpenNoodoe timed out after six seconds at
03:14:49.830. The successful task-3 DONE reply arrived at 03:14:50.336.

By then task 4 was waiting for a new `0x0A` BEGIN reply. OpenNoodoe 0.4.1 matched
replies by command ID only, so it mistook the late task-3 reply for task 4's
reply. The real task-4 `ALREADY_EXISTS` reply then arrived with no matching
waiter, followed by file status 8 and cleanup status 23. This is an application
reply-correlation bug, not evidence that the first upload failed.

### Speedometer creation `0x0400`: complete device acceptance after 6.8 seconds

Task 5 successfully sent the 179-byte cfg and 12,182-byte JPEG. DONE was
sequence-acknowledged at 03:15:12.070. OpenNoodoe timed out at 03:15:18.074, but
the dashboard returned status 0 for task 5 at 03:15:18.897. The dashboard took
about 6.8 seconds after the sequence ACK, just beyond the application's six
second limit.

The user's visible error after selecting the uploaded clock/speedometer is a
separate renderer-level failure. Status 0 from DONE proves that the transfer
task was committed; it does not prove that the creation schema can be rendered.

### Why the uploaded clock and speedometer show an error

OpenNoodoe 0.4.1 sends the same generic configuration to every creation
location: one `BackgroundWidget` and one JPEG. The preserved official app shows
that creation cfg is type-specific:

- The captured official clock bundle contains 21 resources and
  `BackgroundWidget`, `ClockDigitWidget`, and two `DateWidget` entries. Its
  clock widget supplies `format`, ten digit images, punctuation, AM/PM, and
  weekday images.
- The captured official speedometer bundle contains 21 resources and
  `BackgroundWidget`, `SpeedBarWidget`, `SpeedDigitWidget`, and
  `OdometerWidget`. It supplies digit sets, six odometer positions, bar type,
  and color.
- `CreationBundler` constructs each widget through a type-specific bundler,
  merges all referenced files, writes the cfg, and names every file by MD5.

**Inference:** V5.16 accepts and stores the generic bundle because its hashes,
lengths, and transfer framing are valid. The clock/speed renderer then rejects
or fails to instantiate the semantically incomplete widget graph. A known-good
official-shaped reference bundle is the next test; increasing the timeout alone
cannot repair this renderer error.

### Group/radar `0x0700`: endpoint and activation confirmed, function incomplete

Task 13 transferred and completed with status 0. Immediately afterward the
dashboard reported a running creation state, and the rider saw a radar-like
screen with `00:00` and no members.

The official default group cfg contains `BackgroundWidget` plus
`MembersWidget` with a `groupId`; other discovery creations use `RadarWidget`,
`ProximityRingWidget`, ETA, direction, location, and member data. The generic
OpenNoodoe cfg contains none of these.

Therefore this test confirms the location and activation path, not a working
radar implementation. The empty radar/default time is consistent with a
built-in group shell that has neither a valid group binding nor live member
telemetry.

### Music `0x0A00` and audio

All three recorded music attempts were rejected immediately at negotiation with
status 8 (`INVALID_DATA`) for tasks 7, 8, and 9. No music file bytes were sent.
The current V5.16 firmware's statically recovered accepted-location branch also
does not include `0x0A00` or `0x0B00`, although the old official Android library
defines MUSIC and AUDIO enum values and ships default music/audio cfg assets.

This is strong evidence that these enum values describe another firmware or
product capability and are unsupported by this AK 550 V5.16 handler. There is
no `0x0B00` attempt in this capture, so audio rejection remains inferred rather
than live-confirmed. OpenNoodoe also has no runtime track/artist/playback bridge;
its media listener is currently used only for the protected diagnostic gate.

## OpenNoodoe 0.4.3 correction

The following transport corrections were implemented after this capture:

- A pending `0x0A` reply must match the request task ID.
- Pending `0x0B` and `0x0D` replies must match both task and transfer IDs.
- Stale transfer replies are logged and ignored instead of satisfying a newer
  request.
- Creation/navigation task BEGIN, DONE, failure cleanup, and content removal
  wait up to 20 seconds. Per-file packet waits remain six seconds.
- Unit tests reproduce the observed task-3/task-4 late-reply case.
- Preserved official-app clock and speedometer bundles are packaged as explicit
  reference probes. They are never sent automatically and are available only
  through the clock/speedometer reference button.

Firmware and other protected write paths were not enabled or exercised.

## Next controlled vehicle test

1. Install OpenNoodoe 0.4.3 and reconnect once.
2. Insert one photo into a known-empty slot and confirm a single complete task.
3. Do not retry a creation while DONE is pending; confirm that a 6-8 second
   commit now ends as complete without disconnecting.
4. Add and send a preserved, known-good official clock bundle at `0x0200`.
5. Add and send a preserved, known-good official speedometer bundle at `0x0400`.
6. Record the dashboard selection result and the following `RUNNING_CREATION`
   notification for each.
7. Leave gallery removal disabled until the official app's exact gallery
   content-ID/slot delete semantics are recovered and backed up.
8. Treat `0x0A00`/`0x0B00` as unsupported on this profile unless a different
   firmware branch or capability report proves otherwise.
