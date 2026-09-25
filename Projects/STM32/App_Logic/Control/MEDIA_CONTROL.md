# Music actions and phone ownership

UP short toggles playback; UP long goes to the previous track. DOWN short
goes to the next track. ENTER long has no action in the single-phone configuration;
ENTER short retains category navigation. Long actions occur on release using
the debounced duration, and do not also emit the short action. A boot-held
key remains suppressed until released. DOWN long has no Music action.

`ButtonEvents` is the middleware's single BSP queue consumer. Its four bounded
subscribers run on the owning UI task, outside ISR. Subscribe/unsubscribe/init
also belong to that task. Handlers must not block, recursively pump events or
retain the temporary event pointer. Graphics consumes encoder state while
ProductUI independently maps key events to application effects. Product's
old Graphics/BSP bridge does not dispatch a second application event.

`DATA_DEBUG=1` uses one placeholder player, including positions,
play/pause state and track selection. It never fabricates Bluetooth links or
sends playback commands. `DATA_DEBUG=0` targets the sole actual phone and reports offline/busy. Product defaults to0
in Debug and Release; explicit test builds may still enable DATA_DEBUG=1.

## Companion wire contract

Firmware sends an NDCP frame on the selected phone's existing SPP session:

- opcode `0x10`, flags0, sequence with its high bit set;
- payload4 bytes LE32:0 toggle,1 previous,2 next;
- companion responds with the same opcode/sequence, RESPONSE flag (optionally
  ERROR), and a4-byte LE32 result (0 success, nonzero failure).

This is a new companion contract, not a stock Noodoe command. A companion must
dispatch the action to its media session and implement this response. The
existing updater range0x40..0x47 and its authorization are unchanged. Commands
share Control's frame serializer; there is no second unframed Bluetooth writer.

One request per phone is outstanding. It is bound to CID, peer address and
connection epoch. Local acceptance is not a playback success report. No ACK
within3seconds or a changed session fails the request without retransmission;
a lost ACK must not toggle playback twice. An unsent frame expires even under
TX backpressure. Bytes already queued to Bluetooth cannot be recalled. Late,
wrong-phone or wrong-sequence ACKs do not complete another request.

`g_media_control` keeps acceptance/ACK/failure diagnostics in RAM. The damaged
Bluetooth hardware and absent companion media implementation mean end-to-end
phone playback has not been verified. ARM tests cover the real reducer,
middleware fanout, serializer, peer isolation, timeout and update regressions.
