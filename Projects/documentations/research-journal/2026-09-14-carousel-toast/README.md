# Carousel, toast, fixed numeric baseline — 2026-09-14

## Final changes

- Average-speed numeric baseline no longer depends on the current string's ink bounds. With D-DIN,6/8 overshoot7 by1px; the old code moved the whole label348→349→348 at97.6/97.7/97.8. Fixed numeric vocabulary keeps the full label atY348. Font, fixed boxes, horizontal alignment and natural glyph overshoot are retained.
- Carousel center moved123→117; inactive16→22px, active32→36px. Same48px pitch,8 modes, white/gray states and shared240ms Slow–Fast–Slow motion. The full-screen shell owns the bar; the existing body viewport is unchanged.
- App_Logic/UI/Popup_Notifications owns nonblocking requests and timing. Graphics/UI/Toast_View owns the rounded272×48 panel,1px border and Lato24 line. Public task APIs: ShowToastMessages(text,seconds), HideToastMessages(). Printable ASCII1..63bytes;1..60seconds; accepted requests own copied text. Long text uses measured ellipsis without per-frame allocation.
- Entering resettable TRIP A/B shows Hold O to reset for3seconds, including fade. A↔B does not restart it; TODAY/refuel do not falsely advertise manual reset. Existing long-ENTER confirmation remains.
- Product Debug optimization exceptions are synchronized through services_build.py; no Core/IOC/vendor edits or heap/stack reductions.
- Capture waits for REG_DLSWAP as well as command FIFO idle. Diagnostic command4 returns a snapshot and the first4096bytes of that same RAM_DL via the existing CRC mailbox buffer. Commands1/2/3 and mailbox size remain compatible. This is diagnostic hardening, not evidence of a prior hardware display defect.

## Validation

- Release: 407356bytes at0x08010000; SHA256 `aeb41b0cc96685b9956380db1be388c1beccbbb63fb0a3156e73ec766b277a14`.
- Debug: 458604bytes; reserve 148bytes. Future feature work must consider this small link margin.
- install-complete: APP readback matched; lower64KiB SHA256 `f38ed839c4fbce74d4d8227fcf13764194f405c4860cda75c628fce66064b574` preserved;2 reset/HAL/RTOS cycles passed. GUI not used. BT/ALS/NOR/RTC and option bytes unchanged.
- Actual ARM C, bothO0/Os: popup6036assertions, mode strip28097, typography10801, capture8272. Typography uses shipped font metrics and the actual TripLabel source. Capture tests cover pending display swap, timeout/read failure, snapshot/DL payload, pixel generation and CRC. Layer regeneration fixture passed; no actual GUI regeneration claimed.
- complete-positions: live MCU values97.6,97.7,97.8,99.7,100.0 all label bounds(272,348,354,382). Development facts only, no speed/ODO/IGN substitution.
- swap-toast has the exact final Release SHA. Automatic toast observed at718ms, hidden at3005ms; full original PNG/RGB565 preserved. swap-repeat:12 successive real GPU snapshots, all four sampled regions present. TRIP workload29.9FPS, CPU68.3% in the sampled window; these are samples, not a worst-case guarantee.
- cleanup-complete: development source returned LIVE, page preview canceled, toast hidden. COM11 sender PID40136 retained,1→200km/h indefinitely. Actual UART recovery counters are recorded; zero UART errors is not claimed.

## Correction to intermediate diagnosis

Some model-visible image previews appeared to omit upper central content. This was initially and incorrectly treated as an EVE/capture defect. Direct original PNG/RGB565 inspection found the expected pixels in every corresponding TRIP capture: icons53, title27, capsule304 and distance41 foreground pixels on the fixed sampled rows. See original-pixel-audit.json. dl-long is legitimately the blank home after preview TTL expired.

The speculative per-frame state-reset change and removal of the toast border were reverted. The final renderer retains the original1px toast border. No physical panel photograph was taken; EVE rasters and MCU memory are the evidence. Do not infer missing pixels solely from a successive image preview when the saved file has not been checked.

install/install-frame/install-dl/install-fill are intermediate variants, not the final image. install-swap and install-complete have identical final Release bytes. dump_dl.py is guarded as a historical live-DL helper; use hardware_ui.py --gpu-diagnostics for the final atomic command4. The fill-repeat-toast run passed12 region checks and exported its image, then its final metrics report hit a host variable-shadowing exception; recovered-result.json records that limitation and the script is corrected. It is not the source of final firmware validation.

## Source references

The pinned LVGL sources are local to the project. FT81x CMD_SWAP/REG_DLSWAP and CMD_SNAPSHOT2 contracts were checked against the [Bridgetek programming guide](https://brtchip.com/wp-content/uploads/Support/Documentation/Programming_Guides/ICs/EVE/FT81X_Series_Programmer_Guide.pdf),§5.12 and§5.64. Runtime conclusions above come from local captures/tests, not assumed behavior from that document.
