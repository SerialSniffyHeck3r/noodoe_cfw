# S24 OpenNoodoe 0.5.2 live Creation failure

## Evidence

- Vehicle: AK 550 Noodoe V5.16 profile previously identified in this project
- Phone: Samsung SM-S928N, Android 16
- App: OpenNoodoe 0.5.2
- Capture: `capture-20260902-141608-280/protocol.log`
- Rider observation: every Creation that appeared to finish produced the
  dashboard's unknown-error dialog when selected; other runtime functions
  generally worked.

Transport acceptance and visible execution are treated as separate facts.

## Reconstructed results

| Time | Operation | Transport result | Running state / visible result |
| --- | --- | --- | --- |
| 19:41:39 | APK directory-03 POI candidate, 29 files | BEGIN, all files, and DONE status 0 | `RUNNING_CREATION=0`; rider saw unknown error |
| 19:42:02 | generated clock at `0x0200`, 2 files | status 0 through DONE | `RUNNING_CREATION=0`; unknown error |
| 19:42:08 | generated speedometer at `0x0400`, 2 files | status 0 through DONE | `RUNNING_CREATION=0`; unknown error |
| 19:42:11 | preserved official speedometer-shaped bundle, 22 files | status 0 through DONE | no successful speedometer running state; unknown error |
| 19:42:24 onward | repeated same speedometer bundle | BEGIN status 21 `ALREADY_EXISTS` | no bytes retransmitted |
| 19:43:17 | gallery slot 3 | status 0 through DONE | `RUNNING_CREATION=128` (Gallery) |
| 19:43:26 | APK directory-06 Group renderer | blocked locally | packaged JPG filename and content MD5 differ |
| 19:43:31 | multi-member Group pattern | blocked locally | no Group DATA task had been started |
| 19:43:36 onward | rendered navigation image and metadata | three complete status-0 tasks | `RUNNING_CREATION=4` during Navigation, then 0 |

The first reference Creation uploads did not "do nothing." The device stored
them and committed their tasks. Repeated attempts then failed because the same
content ID was already present. This is consistent with the rider-visible error
occurring in the dashboard's post-transfer Creation loader, not in SPP or file
transport.

## Supported conclusions

1. SPP framing, task negotiation, padded CRC32, chunk accounting, and file
   commit are working. Gallery and Navigation provide positive controls using
   the same transfer commands.
2. A successful `TRANSFER_UPDATE_DONE` reply does not validate the cfg widget
   graph or prove that the Creation can be selected.
3. POI directory 03 remains only a candidate. The APK contains it, but this app
   version's default initializer selects only directory 06.
4. Group member behavior was not tested in this run. The renderer was never
   installed and no Group DATA task began.
5. Failed Creation content remains installed. A same-content retry is rejected
   with status 21 until the location is explicitly removed.

## Difference found against the official sender

OpenNoodoe 0.5.2 sorted a reference bundle so that the cfg was always file 1.
The official `CreationTransmitModel` rebuilds a Java `HashMap` keyed by asset
MD5 and the special key `creation`, then sends `entrySet()` order. For the
preserved bundles this places the cfg among the image files, not first.

This ordering difference is a plausible, unconfirmed explanation if the
dashboard validates references when the cfg file is finalized and does not
re-evaluate after later files arrive. It must be tested after removing the
currently installed failed content.

The packaged Group directory also contains an intentional content-addressing
exception: `71b8d38bb9dfd9c141bd507e61a5c9e9.jpg` has content MD5
`f7759104600962c4f29d332aacc7e4e7`. The official sender uses the filename's
32-hex identity and separately sends CRC32 for actual bytes. Rejecting that
asset solely because filename and content MD5 differ was an OpenNoodoe bug.

## OpenNoodoe 0.5.3 response

- Reproduce the official sender's HashMap iteration behavior for generated and
  reference Creation files instead of forcing cfg first.
- Preserve packaged 32-hex file identity independently from actual content
  hash, matching official transfer semantics and allowing the Group default.
- Report successful DONE as `transport committed; renderer not yet validated`.
- Include explicit status names such as `21 ALREADY_EXISTS` in the UI.
- Add an explicit per-location Creation REMOVE control. It is never automatic,
  because removal can also delete a valid user-installed theme.

## Next field test

Use one location per ignition/session. For `0x0400`, first select Speedometer in
the Content tab and run `선택 위치 Creation 제거`. Confirm status 0. Then send
the official reference bundle exactly once, wait for the transport-committed
message, and select it on the dashboard. Preserve the resulting
`RUNNING_CREATION` notification and LCD observation.

If the reordered official bundle still raises the same dialog, file order is
falsified as the cause. The next comparison must then use a fresh official-app
transfer capture with RFCOMM payload visibility, or instrument the official
sender at `CreationTransmitModel.getNextFileToSend()` and
`BTInstallApiHandler.addFile()` to record the exact file order and control
arguments.
