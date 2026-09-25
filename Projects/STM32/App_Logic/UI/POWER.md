# Retained riding mode and summary placement — 2026-09-18

Later user instruction supersedes the Home-on-every-wake behavior below.
Only the first boot chooses Home. Retained IGN returns, including Screen hold,
Bluetooth hold and Deep sleep, preserve the riding category, its local item and
footer. The startup ring sweep and Welcome/scanout eligibility stay independent
of that choice. Full Settings continues to close into its saved riding context;
remote control arming and held button actions are not silently rearmed.
This is retained RAM across IGN/STOP, not newly added persistent navigation
storage for a full loss of power.

Summary title and all Dist/Time/OIL rows shift up24px as one group. Title ink
bottom185→161, row bottoms246/302/358→222/278/334. Prefix/value/unit X positions,
font sizes, fixed widths and24px rise/fade remain unchanged. The common view
clip expands upward fromy145 to121 so the title is not cropped; its lower edge
is unchanged. Welcome coordinates are unchanged.

# Configurable OFF sequence — 2026-09-18

This supersedes earlier AWAKE panel-sleep cycling and indefinite-BT policies.
System → Power sequence controls three explicit stages:

| Label / state | Panel | Backlight | BT | Residence |
|---|---|---|---|---|
| Screen hold / OFF_DISPLAY_HOLD | ON (retained scanout) | OFF | maintained | 0..1440min, default60 |
| Bluetooth hold / OFF_BT_HOLD | OFF (retained EVE RAM) | OFF | maintained | 0..1440min, default60 |
| Deep sleep / OFF_DEEP_SLEEP | OFF | OFF | stopped | Until IGN ON |

All stages default enabled. Each has Enabled/Skipped; at least one must remain
on. Disabled stages and zero-minute intermediate stages are skipped. The last
enabled stage is held until IGN ON, even when its duration is zero: a timeout
never silently enables a disabled deeper state. A malformed empty mask falls
back to Screen hold. Times belong to state entry, are wrap-safe, and are never
extended by buttons, phone links, clock refresh or repeated IGN samples.

Deep sleep is MCU STOP, not verified physical removal of all board power.
No unknown PD13/PG14/PI9 rail switch is asserted. It waits for all four owners
and DMA idle; inherited watchdog and RTC continue to bound sleep intervals.
BT retention applies only to the first two stages. This damaged donor has not
proven actual RF link retention or electrical consumption.

Screen hold never issues panel/EVE sleep. It submits a clock/ODO frame at entry
and once per valid minute, waits for actual swap, then leaves scanout running.
BL remains0 throughout. ON from this state starts the Home ring sweep without
Welcome and waits for fresh ON scanout before PWM. ON from an unavailable
panel also waits for hardware wake and replays Welcome. Interrupted wake cannot
accept a prior epoch's frame. Existing1s provisional OFF hold and5s Summary
are unchanged; stage selection never creates another ride-end event.

Settings are session RAM, like existing AppSettings. No NOR migration/format
or new persistent writes. Source ownership: ui_off_stages.c pure selection;
ui_power.c state transitions; power_ui.c hardware policy; settings_power.c
catalog. Existing diagnostic state numbers4/5/6 are retained, DEEP appended7;
AppSettings and SettingsUI version2 track their expanded RAM layouts.

## 최신 홈/전원 배경 (2026-09-16)

- IGN OFF 뒤 첫1000ms는 밝기까지 포함한 직전 프레임을 그대로 유지한다. 배경 shade/사진 GPU 쓰기, 화면 재구성, 절전 정책 변경, 세션 종료를 하지 않는다. 대기 종료 시 SESSION_END를 한 번 확정하고 주행 snapshot을 만든다. 그때부터 선택 사진 전환·20% 밝기로240ms fade·현재 위치에서 링400ms exit·요약24px rise/240ms fade를 함께 시작한다. 링 scanout을 추가로 기다리지 않는다. 1초 이내 ON 복귀는 같은 주행 세션/배경을 유지한다. 요약은 확정 후5초이며 전환 완료 후 AWAKE 프레임을 유지한다.
- 이미지 휘도 기반 자동 조절은 제거했다. ON 정보 화면20%, 음악32%, 홈 사진100%에 사용자 중앙 밝기 설정을 적용한다. OFF 전환/대기와 Ride Summary는20%다. 첫1초 동안은 기존 밝기를 전혀 변경하지 않으며20% 요청은 종료 확정 이후에만 시작한다. 음악앨범은IGN ON 음악에만 사용한다. 강제OFF사진 선택은 저장된 wallpaper enabled 설정을 변경하지 않는다.
- 홈 UP/DOWN은 사진만/날짜/날짜+UART속도3단계 순환한다. 날짜는RTC Gregorian 결과의영문 월약어·자연 일·요일, 예 Sep. 16 Wed다. 사용자의Thu는포맷 예시이며2026-09-16을Thu로하드코딩하지않는다. 숫자는D-DIN64 고정폭,km/h·mph는Lato24 고정위치,미수신은---다. DATA_DEBUG 시작카드는홈0이다. 실제UART/날짜는placeholder로대체하지않는다.
- 홈 아이콘 strip은 주카테고리 변경5초 후240msfade로숨긴다. 하위모드·속도값 갱신은타이머를리셋하지않으며 다른카테고리는다시표시한다. shell가시성과idlealpha를분리한다.
- Ride Summary의Dist./Time/OIL은동일X114·168·338 고정prefix/value/unit칸이다. 값D-DIN48,설명Lato20,잉크하단246/302/358. 거리자연소수1자리,시간H:MM,OIL자연정수%,미확인은--.24px상승/240msfade는그룹전체공유한다.
- 사진은순정album을보존한별도root WALL0.JPG..WALL2.JPG가우선이다. PhotoImport ABI2의slot|0x100은해당override를create-only로생성한다. 완전JPEGdecode검증은출력tile을버려현재pixels를절대로수정하지않고,읽기대조후명시적재부팅에서만활성화한다. 기존파일덮어쓰기/삭제/포맷은하지않는다. tools/wallpaper_install.py는전체백업+검증journal의현재기준·ARM정확쓰기계획·실기기모든변경영역preimage/readback을필수로한다. 추가사진교체는새로운버전저장정책이필요하며현재한번생성경로를overwrite로완화하지않는다.

# IGN / display / low-power contract — 2026-09-15

This supersedes the old UI-only OFF/SAVE/POWER_CUT model.

| State | Display / BT behavior | Power policy |
|---|---|---|
| IGN_STARTING | Warm return: current ring pose. Actual panel sleep: Welcome. Fresh ON scanout before lamp. | RUN |
| IGN_ON | Driving / quick / full settings | RUN |
| IGN_STOPPING | Exact prior frame and brightness for1s; commit ride end once, fade/exit and Summary5s. | RUN → ECONOMY |
| OFF_DISPLAY_HOLD (4) | Panel/EVE continuously ON, backlight0, BT retained. One clock/ODO frame per valid minute; no sleep/wake cycling. | DISPLAY_SLEEP: MCU tickless Sleep, UART/DMA clocks retained |
| OFF_BT_HOLD (5) | Panel/EVE and backlight OFF, BT retained, no UI frames. | DISPLAY_SLEEP: MCU tickless Sleep |
| OFF_DEEP_SLEEP (7) | Panel/EVE OFF; controller shutdown requested; IO/storage/graphics/BT must all acknowledge, DMA idle. | DEEP: coordinated SDRAM self-refresh + MCU STOP, IGN wake |

BOOT/FAULT remain initialization/failure sentinels, not additional operating modes.
PG13 HIGH means IGN OFF. EXTI wakes owners; the I/O task confirms the11ms debounce.
UART traffic, phone connections and buttons never extend OFF timers or wake the glass.
Invalid speed intervals do not create distance. Session time/distance are separate from Trip A/B,
ODO and maintenance; a partial session is marked in its model, not invented from missing data.

## Physical OFF versus committed session end

IGN_STOPPING contains
DELAY → SUMMARY. DELAY holds the exact last frame for1000ms: no text/ring/image
updates, no shade writes, and no change to the RUN power policy. Photo blend,
opacity and brightness clocks are paused. ON before confirmation cancels the end
without clearing the active ride.

The first tick at or after1000ms of continuous OFF commits SESSION_END once.
IgnitionSession copies the final distance/time into the immutable finished
snapshot and clears only this ride's active counters. At that same transition,
the selected wallpaper is requested, its brightness eases toward20% over240ms,
the ring exits from its current pose over400ms, and Ride Summary rises24px and
fades in over240ms. These motions overlap; ring scanout does not gate commitment.
SUMMARY remains for5000ms from this commit point, then selects the first enabled OFF stage.

UI_OFF_HIDING/UI_OFF_SETTLING and UI_EVT_RING_HIDDEN keep their numeric values
for compatibility but no longer trigger shutdown. Repeated ticks or stale
notifications cannot duplicate SESSION_END. ON after commitment starts a new
ride and keeps the prior summary snapshot. All deadlines use wrap-safe unsigned
millisecond subtraction; invalid speed samples never manufacture distance.

The fixed central summary has the Ride Summary title and three data rows:
Dist. value km/mi, Time H:MM, and OIL value %. Labels/units are20px Lato,
title32px Lato, numbers48px D-DIN. All three rows share the252px composite
centered at X240: X114/prefix44 +gap10 +X168/numbers160 +gap10 +X338/unit28.
Numbers right-align at X328 exclusive. Data-row ink bottoms are246/302/358.
Distance, hours and oil percent have no leading zeroes; minutes keep two digits.
Unknown oil displays--. Numeric vocabulary fixes the ink baseline across digits.
Clock arms extend along their original slope to the circle as the ring fades;
their visible-ring endpoints and all ODO geometry remain unchanged.

## Ownership and public boundaries

- UI state/policy: ui_power.c, power_ui.c, ignition_session.c.
- Reusable view/animation: Graphics/UI/power_view.c, power_scene.c, scene_transition.c.
- Cooperative policy: Middlewares/Noodoe/Power/PowerService. Only the I/O, storage,
  graphics and Bluetooth owners can acknowledge their own quiescent state.
- Hardware: BSP_DisplayPower, EVE/panel retained sleep commands, BSP_Dash_SetSleeping,
  BSP_LowPower strong tickless hook. Core keeps its Cube weak hook and USER CODE bridge.
- Standby and Bluetooth hold settings are independent0..1440minute values, step1,
  default60 each. Each residence timer starts at its state entry.
  Settings v1 stays RAM-only; no implicit NOR format or persistence claim.

## Interrupted transitions and overlays

PowerScene keeps ring and clock/footer tracks independently. Retargeting samples the
current cubic pose. It never restarts at a fully visible/hidden endpoint, including
an interrupted settings animation. The footer hides downward and emerges upward;
the clock hides upward. A warm restart during summary/awake has no Welcome delay.
Welcome requires a cold boot or an unavailable panel (asleep or still settling).

Sleep wake holds PWM at0 until the panel has settled and a newly composed display
list has actually swapped into scanout. CPU submission counts alone do not qualify.
The acknowledgement belongs to the current IGN epoch; Welcome's timer starts at
this acknowledgement, not while the panel is still waking. It preserves EVE RAM_G,
LVGL objects and assets. No second LVGL initialization or SDRAM destructive init occurs.
All power overlays explicitly suppress the separately parented global button hints
and Settings view. Empty labels are initialized with empty strings, never LVGL's TEXT
default. Summary no longer uses the old oil/time-with-seconds layout.
An OFF-delay/summary reversal restores the old card/footer. Returning from completed
standby or a sleeping display selects Home, preserves the footer and runs the sweep.

## Sleep and wake hardware sequence

ECONOMY and BT-retained DISPLAY_SLEEP use ordinary CPU Sleep/WFI with HSE/PLL and
UART clocks intact. H4 UART has no enabled eHCILL handshake, so STOP while radio
transport is live would risk losing its first bytes.

DEEP requires all four owners, actual IGN OFF, no enabled DMA stream, valid LSE RTC
prescalers127/255, and a FreeRTOS idle window. Storage refuses while update/backup
or SWD storage work, queued/active photo decoding, or a photo import is active. UART RX is stopped; USB PCD is stopped; BT must be
OFF with transport closed and no quarantined DMA.

The port arms RTC WUT (LSE/16), reloads an already-running IWDG, enters SDRAM
self-refresh, then STOP with low-power regulator/flash power-down. On any IRQ:
restore HSE/PLL/SYSCLK before SDRAM NORMAL and unmasking interrupts, compensate
HAL/RTOS elapsed time from RTC, then let the proper owner resume. Wakes are bounded
by min(500ms, half inherited watchdog period using conservative60kHz LSI).
It never starts/configures watchdog, resets the RTC calendar, writes option bytes,
or toggles the uncertain external power-cut outputs PD13/PG14/PI9.
An oscillator/SDRAM restore failure leaves a diagnostic fail-stop; it is not a healthy wake.

## Validation and limits

tools/tests/product_ui tests actual ARM state/ride code at O0/Os.
tools/tests/power_ui_host executes the actual coordinator/state/ride/animation
together, with independently delayed submission and scanout completion. It checks
1000ms complete-frame hold,999ms cancellation, one snapshot/reset,5s summary,
standby timeout,3s hardware wake delay, independently delayed scanout and100
interrupted sleep/wake cycles. Session commit does not wait for scanout.
tools/tests/settings_ui includes30 rapid retargets with no position discontinuity,
independent ring/shell direction and ODO below-screen geometry.
tools/tests/power_host executes actual power coordinator, STOP port and display
sequencer against emulated MMIO/RTC/WFI; this does not prove silicon power/current.
Device evidence and build manifests: Reversing/analysis/2026-09-14-ign-power.
Latest commit/summary evidence: Reversing/analysis/2026-09-15-ign-commit.
BT radio remains faulty/unverified on this donor. Current consumption requires an
external measurement; ST-LINK diagnostics alone do not establish a power budget.
Capture servicing of an awake static frame is an explicit diagnostic operation,
with no scene repaint or implicit wake of sleeping EVE.

## Wallpaper while OFF

WallpaperRuntime_Process(page, off) owns the image selection and shade policy.
During OFF the selected photograph remains available and central brightness becomes
20 without overwriting the user's ON brightness setting. Clock/ODO stencil cutouts
remain black. Full Settings hides the photo only while IGN_ON; switching OFF from
Settings restores it. OFF_AWAKE keeps the user backlight until the display timeout.
During the initial1s hold no replacement frame is submitted; wallpaper changes
become visible with the subsequent exit frame. Sleep/wake keeps the existing
scanout/PWM readiness guards.

Current software regression evidence is in analysis/2026-09-15-ign-wallpaper.
Device installation/verification must be read from that folder's installation
summary; a compiled image or ARM test pass is not proof it was flashed.
# Cold-wake speed-ring sweep (2026-09-17)

Cold boot with IGN ON, IGN ON from the completed-summary standby screen, or IGN ON
after display sleep returns the central category to Home. OFF-delay/summary
reversals retain the selected category and do not replay the sweep. Existing Welcome and
shell entrance run first. `SpeedHome_Startup` then waits for an actual Home
frame, sweeps the ring0→10000→0 with the shared slow–fast–slow cubic (800ms per
leg), and requires actual displayed peak/zero frames before advancing.

The final displayed zero establishes a monotonic timestamp fence. The ring
holds at zero until a valid speed telemetry packet has `telemetry_ms` strictly
later than that fence. A changed generic frame sequence, an old valid speed,
or an invalid/stale packet cannot release it. The ordinary150ms retargeting
then starts from the displayed zero. This does not flush UART, halt an I/O
task, change raw speed validity, or inject demonstration speed into trips,
distance integration, settings movement locks or other domain calculations.

`SpeedHome_OverrideArc` is presentation-only. Each phase is nonblocking;
IGN OFF cancels it before composition, including the1s provisional hold which
still preserves the last visible frame. A subsequent warm ON does not restart
an interrupted sweep. `g_speed_startup` (magic SWP1/version1) exposes phase,
epoch, angle, endpoint timing, final frame fence and first accepted telemetry
timestamp for SWD verification. No generated Cube/vendor source is changed.
