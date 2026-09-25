# UART service ARM C tests

`run.py` compiles actual `Dash_Protocol.c`, `DashService.c` and
`Vehicle_Service.c` with the installed ARM GCC at both O0 and Os, executes them
in Unicorn, and stores ELF/compiler output/results in `output`.

The 93 assertions in each build cover stock3000/400/800 strict timing and tick
wrap, every fourth admitted frame, pending reply bounds, all ten threshold
boundaries, idempotent peek, stop/request wire checksums, real-completion versus
submission accounting, operation IDs/Busy, stop TX ownership, resume, sensor
cache/stale/override/AUTO, fixed mailbox completion, failed/hung abort, permanent
submission busy and immediate submission error.

HAL and sensor observations are mocks. Locks mark boundaries but do not create
actual RTOS preemption. The separate dash_irq_host fixture exercises the actual
BSP under injected IRQ interleavings. The protocol_services suite independently
tests all256 command IDs with empty/nonempty payloads. Actual COM11/UART DMA
evidence belongs to `analysis/2026-09-12-uart-bringup`, not this host fixture.
