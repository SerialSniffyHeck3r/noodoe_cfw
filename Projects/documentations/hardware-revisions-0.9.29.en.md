# Supporting the older display wiring from stock assembly

PCBA text is not a GPIO strap. DeviceInfo HW, PCBA, bootloader version, the four-bit strap and the panel selector are separate identities. This change follows the branches actually taken by stock V5.16 rather than treating the digits in `SR0701` as a wiring revision.

The input was the 458,748-byte SR1.5 V5.16 OTA APP, SHA-256 `3b64673054ca84b9cf504f4cc7ebcb2e6615b9fbf59df79a37a92dcf14f037ca`. The captured BL 0.14 hash is `f8b379c3fac078a8e01d8b6c36fccc5bb5ea5852a0db008822ef6871e0f38df5`. Addresses below assume the original APP execution address. Neither BL code nor factory data is changed.

## Finding the switch

Stock function `0x0804259E` reads PA3, PH3, PH2 and PB10 into a four-bit strap. Its pin table is at `0x08076DD0`; the cached result is at `0x2002330D`. Panel startup, shutdown and the system reset path consult this value. The shared BSP reader keeps the input/no-pull configuration.

The donor evidence reports strap 6, panel 3, PCBA `sr0601`, BL 0.14. Separate vehicle identity evidence reports `SR0701`, BL 0.15 and panel 4. It does not establish that vehicle's strap; no value was invented from its name.

## Two ways to reset the panel

Panel startup at `0x0806AB0A` compares the strap with 3. For 0–2, helpers `0x0801FCCC` and `0x0801FCF6` modify bit 7 of EVE `REG_GPIOX`, address `0x302094`. For 3 and above, stock toggles MCU PC13. The EVE path uses read-modify-write, preserving the other GPIO bits. Toggling PC13 on the older board cannot substitute for that action.

The common startup sequence is PI11 LOW, 10ms, reset HIGH, 10ms, reset LOW, 20ms, reset HIGH, 50ms, SPI4 setup with argument `0xA5`, command `0x1100`, 120ms, command `0x2900`, and status read `0x0A00`. Status is compared with `0x009C`. The SPI unit here is a 16-bit word, not a byte.

Shutdown at `0x0806AC54` sends `0x1000`, waits 120ms, sends `0x2800`, drives panel reset LOW, waits 10ms, drives PI11 HIGH, and deinitializes SPI. On the older wiring, EVE must still be alive to lower GPIO7. The new CFW branch therefore completes LCD shutdown before taking EVE PB1 low. The already-working PC13 branch retains its order, polarity and delays.

## Follow the wiring all the way through recovery

The branch belongs in startup, full display shutdown, STOP wake, emergency display recovery and Gate. A Product that works on GPIO7 while Gate cannot display recovery instructions would be incomplete support. Gate feature bit 2 now identifies this wiring support. The matching installer ZIP includes it. Ordinary Product updates on the existing PC13 profile do not require a Gate replacement.

The implemented common-panel profiles accept panel selectors 3/4 and straps 0–15, selecting GPIO7 below 3. BL identity, original factory data, NOR/RAM/display checks and the stock-restoration contract remain separate prerequisites. The APK still checks actual stock DeviceInfo against its package target. **Expanding display wiring support does not turn every unknown PCBA and BL into an authenticated installation target.**

Stock also contains a different protocol for panel values at least 10, excluding 255. Startup `0x0806ACF8` sends `001D 1FAA 0C5A 0000 1FAA 0102` and waits 50ms; shutdown `0x0806AD28` includes a 140ms delay. That protocol is not mixed into the panel 3/4 profile. Supporting it requires its own identity and recovery contract, rather than forcing the common sequence onto an unknown panel.

Stock system reset at `0x08043C50` has a separate PD13 and strap-dependent PI9/PG14 handshake. It was not confused with panel reset or added to display recovery. The established MCU reset route is preserved.

## What was checked

The original ARM instructions were emulated for all 32 strap/panel combinations and one failed panel-status response. The GPIO/MMIO and SPI replies were modeled. Current BSP tests passed 252 checks per O0/Os build; Gate passed eight paths per build. The older display shutdown/reinitialization path completed 1,000 cycles each under O0/Os/Oz, with 3,030 transition checks and 23 emergency-input checks per run.

This establishes software branch and control-order behavior. It does not measure electrical signals on an older board, LCD illumination or Galaxy S24 Ultra pairing reliability.
