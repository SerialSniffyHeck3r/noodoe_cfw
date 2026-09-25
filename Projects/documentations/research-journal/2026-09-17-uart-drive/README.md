# Continuous physical-UART driving simulator

The user requested continuous changing speeds; unlike the prior short test,
this process is intentionally left running after the assistant turn.

- Program: `../../STM32CubeProjects/FuckNudo_Noodoe_CFW_Project/tools/dashboard_drive.py`
- PID: see `pid.txt` and `run-001/status.json`; launched with a hidden window.
- Actual port: COM11, Uno VID:PID 2341:0043, serial [redacted USB-UART serial].
- Wire format: 115200 8N1, F5/21/09/payload/XOR, target10Hz.
- Pattern: stop, city acceleration/cruise, road acceleration/cruise,
  160..200 km/h acceleration, 200 cruise, deceleration, stop;150 seconds/cycle.
- ODO starts36475km and increases with transmitted-speed integration; no reset
  on cycle wrap. Fuel stays at the recorded one-bar value; other bytes are preserved.
- No firmware FLASH/NOR write, target reset, or injected RAM vehicle data.

Stop by creating `run-001/stop`. The owner emits one final0km/h frame and closes
COM11. Do not start another serial writer while this process is alive. USB/serial
failure ends the process with an error in status; it does not silently reconnect.

`live-01/result.json` is the actual live MCU observation after simulator startup.
The MCU is IGN ON and UI ready, but `rx_frames=0`, `rx_bytes=1` and UI speed/ODO
validity are0. The PC also received0 bytes while the target's TX counter advanced.
Thus the simulator's transmission is proven; physical link reception remains
unverified/unsuccessful. Do not report successful speed display or ride accumulation.
The stream remains active as requested while the harness can be checked.

Status/history are bounded and refreshed once per second. Each published frame
was independently validated against the repository protocol parser before launch.
The arithmetic cycle length is150seconds (the initial160-second verbal estimate
and hardcoded preflight print were incorrect; the executed plan/status use150).
