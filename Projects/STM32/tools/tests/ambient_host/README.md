# ALS failure and asynchronous upper API regression

Run `python tools/tests/ambient_host/run.py` from the project directory using the
configured analysis Python runtime. The script uses installed ARM GCC and
Unicorn. It does not enumerate/open USB,ST-LINK,serial ports,or other hardware.

Both O0 and Os execute161 assertions in the actual `BSP_Ambient.c` and
`AmbientService.c`. HAL operations are mocked; production HAL/CMSIS types and
ARM critical-section instructions are retained. Each production file is also
compiled separately with `-Os -Wall -Wextra -Werror`.

- Every initialization/configuration failure stage stops later work and keeps
  ready/valid false; wrong IDs prevent sensor writes.
- ARLO0x200 with HAL_TIMEOUT and AF0x400 with HAL_AF remain distinct; status,
  elapsed time,and original diagnostic prefix are checked.
- Explicit80/100/400kHz choices,default400kHz restoration,MSB-first sensor words,failed-write configuration
  retention,disabled polling,and invalid optical samples are exercised.
- Requests/getters perform no HAL operations; a request injected from inside a
  HAL callback sees BUSY and cannot replace the active ID or its snapshot.
- Failed startup retries at bounded deadlines; three unsuccessful retries
  stop; explicit retry recovers; automatic recovery preserves the original
  explicit completion result.
- Raw published diagnostics and fresh upper snapshots both report invalid or
  aged samples as stale; the sampled publication time is distinguished from
  the later getter time.
- Mailbox response is uncommitted during HAL work; unsupported commands and a
  full queue are rejected; driver evidence remains frozen across auto-retry.

An independent `test_bitbang.c` adds162 assertions per O0/Os against actual
`BSP_AmbientBitbang.c`. Its sensor model decodes START/STOP and clock edges;
it does not use the driver's phase/bit counter to choose response data. It
checks all six address/register ACK failure positions, exact fixed outgoing
bytes, both IDs, wrong IDs, stuck/conflicting lines, delayed LOW readback,
first/second STOP failure,35ms LOW/preemption, stopped DWT, reentry, no takeover
on wrong pins/active DMA, exact owned-pin restoration, unrelated GPIO changes,
and PE-off/HAL-lock retention after a restoration mismatch. The service tests
also prove command3 identity/fresh getter and quarantine against enable requests.

CR1 writes model the hardware's PE-off ACK/POS/START/PEC/ALERT clearing rather
than plain RAM. Four saved ACK/POS combinations and PE-already-off restore
exactly; enabling PE must precede restoring receive policy. Active PEC/ALERT
is rejected without a write. Stable foreign CR1/CR2/CCR changes and failed ACK
restoration still leave PE off/HAL locked. The initial real0x401→0 transition
that exposed the old restore-check assumption is a dedicated regression.

`test_address.c` adds135 assertions per O0/Os for the fixed0x44..0x47 diagnostic.
It reuses the electrical endpoint,which accepts a chosen address mask and
independently decodes emitted addresses/register bytes. All-NACK emits only
four address bytes; each possible individual responder receives7E/7F reads at
that same address. Wrong IDs remain visible while other candidates continue.
Every ID-read ACK can fail; a line/STOP/timing failure aborts the remaining
rows. Even a previous successful ID cannot conceal a later electrical failure.
The new288-byte record preserves the original212-byte ID record byte-for-byte;
an ordinary diagnostic afterwards does not inherit the temporary PC9 pull-up.
Command4 queue/correlation/getter and restoration quarantine are also tested.
The batch has one takeover/restore and a100ms active budget; the ordinary
diagnostic retains25ms. The per-gap10ms guard is unchanged. These budgets do
not claim that interrupt/task preemption itself can be bounded.

`output/results.json` records source hashes and all six runs. Physical line
edges and clock progression are modeled; actual pin-register masks, ordering,
protocol logic and restoration execute as compiled ARM C. This does not prove
real timing, electrical signal integrity or that the actual sensor acknowledges.
