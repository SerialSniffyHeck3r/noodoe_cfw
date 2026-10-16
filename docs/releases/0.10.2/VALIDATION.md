# 0.10.2 validation

Software checks only; no SWD, vehicle, physical Bluetooth, power-loss or battery-current test was performed.

- Product Debug and Release built with existing optimization policies and validated ELF/vector/layout contracts. Both meet the user-confirmed 1 KiB minimum: Debug 1036 bytes, Release 1036 bytes free. The 32 KiB development target remains advisory and is not met. No heap/stack, RAM reservation, font, icon or UI asset was cut.
- Production UART BSP compiled and executed as ARM at O0 and Os: 100 assertions each. Tests cover PE3 before UART init, 20 ms settling, init/RX failure, sleep/wake/retry, unrelated GPIO preservation, active and uncertain TX-DMA ownership, RX recovery and IRQ handling.
- Production vehicle service/protocol compiled and executed as ARM at O0 and Os: 216 assertions each. Invalid calibration variants still request and accept known speed/ODO frames; unavailable light fallback, stop/reconnect, asynchronous failures and existing alert cases pass. This does not establish the affected Xciting calibration contents or wire packet format.
- Android selected tests: 63 passed, zero skipped/failed. Actual installer imports include the previous 0.9.73 and 0.10.1 ZIPs, the new Product/Bootstrap, unchanged Gate/resources and matching language contract. Additional installer, handoff and language-bundle checks pass.
- APK version 0.10.2/code105; Product hash in APK matches the padded ZIP Product. APK signing certificate matches 0.10.1. Gate, stock/uninstall/diagnostic images and language archive are unchanged. Fresh Bootstrap includes the shared vehicle-link fix.
- Matching compiled relink objects reproduce DEX, preserve application classes, pass ZIP integrity/alignment, and contain no source or signing keys. The reconstructed APK was not installed on a phone.

The affected 2021 Xciting 400 still needs a real-device check of speed, odometer,
trip accumulation and sleep/wake. Stock analysis establishes a missing PE3 init
sequence; it does not prove that this pin alone caused that particular report.
No guessed model-specific packets were added.

The accepted old-code stock-return power-interruption limitation remains.
See INSTALL.md for the ordinary 0.10.1 update versus older-Gate transition.
Latest/beta publication does not mean physical qualification of all revisions.

한국어: 코드·모의 실행·빌드·설치 ZIP 검사를 통과한 수정입니다. 해당 Xciting의
실물 수신 회복이나 소비전류를 측정했다는 뜻은 아닙니다. 이전 Gate 전환 중
구버전 코드의 전원 차단 제한은 그대로이며, 새 검사 기준을 낮추지 않았습니다.
