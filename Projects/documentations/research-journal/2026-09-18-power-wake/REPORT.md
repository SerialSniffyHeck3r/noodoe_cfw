# IGN standby sweep and cold-wake repair

## Behavior

- IGN ON from completed Ride Summary / lit standby selects Home and performs the
  existing 800ms-per-leg cubic ring sweep. It does not replay Welcome.
- Initial boot / dark-panel wake runs Welcome, Home and the sweep. The last zero
  frame remains the fence: only a later valid UART speed packet releases the ring.
- Provisional OFF retains the whole frame for 1 second. OFF/summary reversals keep
  their page and current pose. They do not replay the sweep or prematurely reset
  the ride. The real speed and trip calculations continue independently.
- Default timing is unchanged: Summary 5s, lit standby 60min, display sleep,
  Bluetooth retention 60min, then a deep-sleep request. Both minute values are
  adjustable down to zero. This is RAM-session configuration, not new persistence.

## Root causes and changes

1. Sweep eligibility had been identical to Welcome eligibility, excluding lit
   standby. `UiState.startup_sweep` now separates those policies.
2. The wake coordinator counted CPU display-list submissions instead of checking
   actual scanout. It now waits for hardware readiness and REG_DLSWAP completion
   of a fresh frame from the current IGN epoch. Welcome time starts only then.
   Initial Product startup also keeps PWM zero through the old/test display lists.
3. The display reported awake during its oscillator/panel settling phases. It now
   remains unavailable throughout these phases. A completed sleep operation, not
   merely unavailability, is required for the graphics owner's STOP acknowledgement.
4. Actual pre-change registers were RCC_BDCR=0x00000003: LSE ON/ready, but no RTC
   source or enable. BSP_Clock previously only attached to an inherited RTC, so it
   returned NOT_READY and STOP's timekeeping prerequisite could never pass.
   A previously unselected RTC now selects the already-ready LSE and starts the
   prescaler/counter through the owned BSP. Existing RTC source/date/backup/alarm
   state is preserved; a different already-selected source is refused, not reset.
   No date is invented. Counter readiness and calendar validity remain separate.
5. RTCStamp's second SSR read for rollover checking re-locked the shadow sample.
   Reading DR after that check releases it before STOP. Otherwise the next sample
   can return the pre-sleep time and lose elapsed time compensation.

Register semantics were checked against ST RM0090 (RTC clock selection, calendar
initialization and SSR/TR/DR shadow read protocol):
[ST RM0090](https://www.st.com/resource/en/reference_manual/dm00031020-stm32f10x-microcontrollers-stmicroelectronics.pdf).

## Verification

- Actual ARM UI model: O0/Os/Oz,25 groups,110651 state +5697 speed assertions each.
- Power/UI coordinator: O0/Os,641 assertions each, including100 interrupted wakes,
  delayed hardware, independently stalled scanout and display/BT timer expiry.
- RTC BSP: O0/Os,93 assertions each: virgin selection, preserved calendar/backup,
  wrong existing source, init/sync errors and existing alarm/context checks.
- Actual STOP/display sequence with emulated MMIO/WFI: O0/Os,438 assertions each.
  Fixed code leaves no shadow lock at WFI. Removing only the new DR unlock
  reproduces102 locked entries in each build (`shadow-regression.json`).
- Settings UI: O0/Os,179 assertions each. Cube regeneration metadata fixture
  restores Product/Integrated/Graphics policy; no GUI regeneration was performed.
- Release/Debug builds pass. Existing linker RWX-segment warning remains.
  Release APP free86812B, Debug71364B, SRAM35744B, CCM16320B.

Actual first installation: RTC ready1/error0, counter advanced, calendar valid0
and sets0. PWM25, no display error, cold sweep completed once and accepted a real
UART packet after the final-zero fence. This is device evidence, not mocked state.
The final image additionally includes the RTC shadow-unlock correction.

Final-image device checks (`final-boot`, `after-capture`): RTC ready1/error0,
calendar valid0/sets0, Home card0, sweep starts/completed1/1, final-zero fence5983ms
and newly accepted UART sample6047ms. Later physical UART speed169km/h was valid.
The actual EVE480x480 frame completed in1131ms and its full RGB565 download was
CRC checked (`final-capture/display.png`). Subsequent sampled performance was
29.9FPS / CPU59.2%, with Product UI error0. No physical IGN edge occurred during
these recorded checks (epoch remained1), so they prove reset boot and continuing
operation, not a physical standby/STOP wake.

Physical standby→ON, screen-sleep→ON and complete supply removal require external
IGN/power control. ARM tests do not establish actual STOP current or guarantee that
every user-observed wake symptom is reproduced. Record final live checks separately.

## Artifacts

- `candidate.elf`: final Release, APP at0x08010000.
- `candidate-initial.elf` / `install`: earlier candidate before the extra RTC
  shadow correction. The final installation is `install-final`.
- Final APP SHA256: `e19732f28acaffafca438b57fac59bb00bf480ff2a0775e31b51eb00094ad2d2`.
- `before` / `registers` / `first-boot`: original device observations.
- Installation verifies donor UID, exact prior APP, whole new APP readback and
  original lower64KiB. NOR, options, generated Core and IOC are unchanged.
- The existing COM11 driving scenario remains under
  `../2026-09-17-startup-ring/drive/status.json`; it is not a RAM speed injection.
