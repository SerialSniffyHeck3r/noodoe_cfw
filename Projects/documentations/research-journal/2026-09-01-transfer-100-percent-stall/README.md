# OpenNoodoe file transfer 100% stall investigation

## Scope

- Device: AK 550 Noodoe, firmware 1.62, PCBA SR0701
- Android client: OpenNoodoe 0.4.0 on Galaxy Tab Active3
- Preserved capture: `captures/20260901-slot1-theme-transfer-investigation/`
- This investigation only pulled logs over ADB. It did not issue an additional vehicle command.

## Confirmed observations

The `20260901-182249-513/protocol.log` capture contains three distinct failures.

1. Gallery slot 1 sent 15,158 bytes in two chunks. The dashboard replied with
   `chunk=11816, cumulative=11816`, then `chunk=3342, cumulative=15158`.
   OpenNoodoe treated the second reply's offset 8 field as a cumulative counter,
   compared 3,342 with 15,158, and aborted after the UI had reached 100%.
2. A one-chunk gallery transfer and a creation configuration both delivered all
   bytes, but `FILE_CONTROL UPDATE_TERMINATE` returned status 1.
3. A failed file left its negotiated transfer task open. The SPP connection later
   timed out or disconnected instead of receiving the normal task `DONE` command.

The same capture also shows both `com.noodoe.sunray` and `io.opennoodoe.app`
attempting Bluetooth connections around 18:24. This is a separate source of SPP
contention and must be excluded during the retest.

Android's process exit history contains no ANR entry and Dropbox has no
`data_app_anr` report for OpenNoodoe. It does show a `LOW_MEMORY` process exit at
18:22:34, immediately before the 18:22:49 service restart used for this test.
The observed lag is therefore not a confirmed main-thread ANR. The supported
explanation is a combination of process restart, serialized 6-second command
timeouts after protocol failures, and competing SPP clients.

## Root causes

### File data reply layout

The live 16-byte `0x0D` reply demonstrates this layout:

| Offset | Width | Meaning |
| --- | ---: | --- |
| 0 | 2 | status |
| 2 | 2 | task ID |
| 4 | 2 | transfer ID |
| 8 | 4 | bytes accepted in this chunk |
| 12 | 4 | cumulative bytes accepted |

OpenNoodoe 0.4.0 called offset 8 `received` and compared it with the cumulative
expected position. That comparison necessarily fails on the second chunk.

### CRC32 padding

The official app's `SunrayTransferFileTaskHandler.addFile` reads 1,024-byte blocks.
When the final read is not divisible by four, it appends zero bytes to the next
four-byte boundary before updating CRC32. The unpadded byte count remains the
advertised file size.

OpenNoodoe 0.4.0 calculated ordinary CRC32 without this padding. Because the
dashboard accepts all data and rejects `UPDATE_TERMINATE`, the CRC mismatch is the
strongest explanation for status 1 at finalization. A live 0.4.1 retest is still
required before this is considered confirmed on firmware 1.62.

### Failed task cleanup

The official transfer handler's `stopTask()` sends `TRANSFER_UPDATE_DONE` even
after transfer failure. OpenNoodoe returned immediately and left the task open.
Version 0.4.1 now performs a best-effort `DONE` for failed ordinary file and
navigation transfers. Firmware transfer behavior was deliberately left unchanged.

## Gallery replacement finding

The official `GalleryPresenter` does not overwrite a populated slot directly. It
calls `uninstallGalleryImage(index)`, waits one second, and then installs the new
image. `DashboardManager.uninstallGalleryImage` deletes the old locally tracked
gallery file and triggers `ACTION_SYNC_GALLERY`.

This does not yet prove which dashboard-side remove/query sequence is required
when a third-party app does not know the old MD5 filename. OpenNoodoe must not send
a blind location-wide `REMOVE`: that could delete more than the selected slot.
Replacement of an already populated slot therefore remains unsupported pending
an official-app replacement capture or a proven per-slot query/delete sequence.

## OpenNoodoe 0.4.1 changes

- Decode data reply offset 8 as `chunkSize` and offset 12 as `cumulativeSize`.
- Validate task ID, transfer ID, exact chunk acceptance, and cumulative progress.
- Calculate official-compatible CRC32 with zero padding to a four-byte boundary.
- Reuse the same CRC in START and UPDATE_TERMINATE controls.
- Close failed ordinary transfers with a best-effort task `DONE`.
- Log `chunk=` and `cumulative=` explicitly.

## Vehicle retest

1. Force-stop both official Noodoe packages so OpenNoodoe is the only SPP client.
2. Start a fresh capture session, connect OpenNoodoe, and read device information.
3. Send an image to a known empty gallery slot. Prefer a generated file larger
   than 11,816 bytes so the multi-chunk path is exercised.
4. Confirm every `0x0D` reply shows the expected `chunk` and increasing
   `cumulative` values.
5. Confirm `FILE FINISH` returns status 0 and task `DONE` returns status 0.
6. Verify the image appears on the dashboard, then mark the observation in-app.
7. Do not test replacement of a populated slot until its delete sequence is
   implemented from stronger evidence.
