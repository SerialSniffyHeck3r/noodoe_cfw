# GPS geometry / EVE command regression

Run `run.py` with the project's Python runtime. The harness executes extracted production UI functions, actual world-grid projection, vendor EVE line rendering and project scissor code as ARM Cortex-M4 code. Only task allocation and SPI output are substituted. Three builds test648 heading/offset/zoom combinations and long routes, clipping and page fades.

The archived old producer comes from `Reversing/analysis/2026-09-24-gps-tearing/before`; it must continue reproducing an8KiB overflow once the real ring and circular viewport are included. New geometry must remain bounded. Counts intentionally exclude the rest of the UI and do not assert physical pixels or realFPS.
