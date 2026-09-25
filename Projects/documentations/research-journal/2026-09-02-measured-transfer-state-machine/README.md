# OpenNoodoe 0.6.0 measured transfer state machine

## Why this exists

The phone-side file protocol was working well enough to receive successful
`BEGIN`, per-file, and `DONE` replies, but repeated button presses and delayed
replies made field results difficult to interpret. A successful `DONE` also
does not prove that the dashboard renderer accepted the installed cfg.

## Official-app evidence

The preserved protocol-1.5 implementation defines these handler states:

```text
STOPPED, STARTING, STARTED, STOPPING, RESETTING,
STARTING_TO_DELETE, ERROR_STATUS
```

It rejects a second task while one is active. Its negotiate attributes are:

```text
1 UPDATE_BEGIN
2 UPDATE_CONTINUE
3 UPDATE_DONE
4 REMOVE
5 RESET
6 CANCEL
```

`resetTask()` sends attribute 5 with the same task, location, total size, and
content ID. A successful reset clears the official sender's queued file state
and returns the handler to `STARTED`; it does not close RFCOMM and it does not
end the task. OpenNoodoe therefore follows RESET with DONE when the user asks
to abandon the active operation while preserving the Bluetooth session.

## OpenNoodoe instrumentation

Creation, gallery, resource, and remove actions are claimed before they are
queued. A second press is logged as `DUPLICATE` and rejected. Every accepted
action creates a new capture and records:

1. SPP connection state and sequence indexes.
2. Read-only DEVICE_INFO, GET_METER_PROFILE, and RIDING_STATUS preflight.
3. Task ID, location, content ID, total bytes, exact file order, file identity,
   size, and padded CRC32.
4. Each BEGIN, FILE control, FILE data, FILE finish, and DONE transition.
5. Dashboard reply status and any stale reply rejected by task/transfer ID.
6. The rider's `normal`, `no change`, or `error/stop` observation.

The visible phases are:

```text
QUEUED -> PREFLIGHT -> BEGIN -> FILE -> FILE_DATA -> FILE_FINISH
       -> COMMIT -> VALIDATION_PENDING -> OBSERVED
```

Any protocol failure moves to `FAILED`. RESET moves through
`RESET_REQUESTED -> RESETTING -> RESET_COMPLETE` or `RESET_UNCERTAIN`.

## Logging and responsiveness

The former logger wrote complete file-data sequence frames as hex. That made
the log much larger than the transferred source and caused avoidable storage
and UI work. Frames over 512 bytes now retain a 96-byte prefix, exact total
length, and CRC32. The transfer manifest retains the file-level identities
needed to reproduce and compare the run.

## What remains unknowable from this channel

The SPP protocol confirms transport commit but no renderer-validation reply has
yet been identified. `RUNNING_CREATION` (`0xC1`) is an unsolicited runtime-state
notification, not a cfg parser diagnostic. Therefore a `DONE status=0` followed
by a dashboard error is recorded as transport success plus renderer failure,
not as a successful Creation installation.

To learn the exact official sender order rather than merely OpenNoodoe's order,
instrument the working KYMCO app at `CreationTransmitModel.getNextFileToSend()`
and `BTInstallApiHandler.addFile()`, recording task/location/content ID,
filename identity, target path, size, CRC, and call order.
