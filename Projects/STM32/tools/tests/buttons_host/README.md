# Buttons actual-C tests

Run `python tools/tests/buttons_host/run_buttons.py` with the installed analysis Python. The runner uses STM32CubeIDE's ARM GCC and Unicorn 2.1.4 from `Reversing/.tools/analysis-python`; it does not generate Cube code or access hardware. `--toolchain` and `--output` override the tool/output locations.

The production `BSP_Buttons.c` is compiled together with the C test fixture at both `-O0` and `-O2`, using `-Wall -Wextra -Werror`. GPIO, HAL tick and NVIC calls are mocked; button logic is not rewritten in Python. The emulator must return to the sentinel, so a timeout cannot be mistaken for success. `results.json` records production C/header hashes, test case, failure line and assertion count.

Ten scenarios cover initialization ownership and shared pending preservation, press bounce and IRQ spam, short decision boundaries at 59/60 ms, delayed release processing without noise promotion, 2000/2001 ms long boundary and release-duration freeze, 3000 ms very-long and threshold catch-up, no automatic repeat, simultaneous buttons, release-bounce recovery, boot-held input, tick wrap and duration saturation, event overflow, and invalid API arguments. These verify software state transitions; electrical behavior, real IRQ wiring and RTOS scheduling still require board testing.
