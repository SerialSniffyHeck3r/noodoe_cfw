# Graphics input actual-C tests

`python -B tools/tests/graphics_input_host/run_graphics_input.py` compiles the actual
`Graphics/Port/src/graphics_input.c` with the production Graphics/BSP headers and
mock LVGL/BSP boundaries, then runs it in Unicorn at both O0 and O2. The runner also
extracts the **unchanged function body** of `Graphics_Shutdown` from `graphics.c`
to exercise its lifecycle calls with mocked globals and dependencies. It does not
compile the firmware project, generate Cube code or access a device.

The installed ARM GCC and `Reversing/.tools/analysis-python` Unicorn are reused;
there are no downloads. `--toolchain` and `--output` override their defaults.
Generated fixtures, compile logs and JSON results default to this directory's
`output` subdirectory. Source/body hashes identify exactly what was executed.

Ten cases cover encoder setup, boot-held RELEASE→SHORT suppression, fresh presses,
duplicate/unarmed SHORT suppression, real ENTER state, disabled encoder with an
active observer, bounded overflow reporting, deinit/reinit and allocation failure,
actual repeated Shutdown calls, and invalid shutdown contexts. Deleted mock input
objects are poisoned so reset/delete after free is counted as failure. Debounce,
electrical signals, full LVGL scheduling and real display output are outside this
test; the separate buttons_host suite tests the actual BSP debounce state machine.

Product 입력 통합 시험은 BSP PRESS/RELEASE/SHORT 큐→실제 graphics_input.c→App_Logic/UI의 product_input.c→UiState 경로를 실행한다. LVGL encoder를 끈 상태, 고장난 UP의 부팅 잠금, ENTER 한 번 한 카드, OBD 가용성, 장시간 입력 분리까지 검사한다. 실제 PA15 전기적 누름을 수행한 시험은 아니다.
