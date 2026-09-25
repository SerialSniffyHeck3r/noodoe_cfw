"""Continuous bench driving simulation through the actual Uno USB UART.

115200 8N1, one recorded CMD21 frame every 100 ms. Only speed and ODO change;
the measured fuel-one-bar fixture and unknown fields stay intact. This tool
does not touch SWD, IGN, RTC, firmware, or the real vehicle dashboard.
"""
from collections import deque
from pathlib import Path
import argparse
import json
import math
import os
import time
import serial
import dashboard_uart

# Seconds, starting/ending km/h. Each boundary is continuous, including wrap.
SEGMENTS = (
    ('stopped', 10, 0, 0),
    ('city_acceleration', 12, 0, 50),
    ('city_cruise', 15, 50, 50),
    ('road_acceleration', 10, 50, 90),
    ('road_cruise', 15, 90, 90),
    ('high_speed_acceleration', 14, 90, 160),
    ('overspeed_acceleration', 10, 160, 200),
    ('overspeed_cruise', 8, 200, 200),
    ('high_speed_deceleration', 20, 200, 60),
    ('approach', 10, 60, 60),
    ('braking_to_stop', 12, 60, 0),
    ('traffic_light_stop', 14, 0, 0),
)
CYCLE_SECONDS = sum(row[1] for row in SEGMENTS)
PERIOD = .1


def sample(elapsed):
    """Pure repeated speed profile; integer speeds match the stock wire field."""
    if not math.isfinite(elapsed) or elapsed < 0:
        raise ValueError('Elapsed seconds must be finite and nonnegative')
    cycle = int(elapsed // CYCLE_SECONDS)
    position = elapsed % CYCLE_SECONDS
    for name, duration, start, end in SEGMENTS:
        if position < duration:
            return name, int(start + (end - start) * position / duration + .5), cycle
        position -= duration
    raise RuntimeError('Invalid profile boundary')


def main():
    """Own one serial port until stop-file/timeout/error; always finish at zero."""
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--port', default='COM11')
    ap.add_argument('--odo', type=int, default=36475)
    ap.add_argument('--seconds', type=float, default=0, help='0 = repeat until stop file')
    ap.add_argument('--output-directory', type=Path, required=True)
    ap.add_argument('--execute', action='store_true')
    args = ap.parse_args()
    if not math.isfinite(args.seconds) or args.seconds < 0:
        ap.error('seconds must be finite and nonnegative')
    dashboard_uart.frame(0, args.odo)
    plan = {'port': args.port, 'baud': 115200, 'format': '8N1',
            'cycle_seconds': CYCLE_SECONDS, 'interval_seconds': PERIOD,
            'initial_odo_km': args.odo, 'fuel_observed_bars': 1,
            'segments': SEGMENTS, 'seconds': args.seconds,
            'stop_file': str(args.output_directory / 'stop')}
    print(json.dumps(plan), flush=True)
    if not args.execute:
        return
    out = args.output_directory
    out.mkdir(parents=True, exist_ok=False)
    (out / 'plan.json').write_text(json.dumps(plan, indent=2) + '\n')
    events = deque(maxlen=6000)
    state = dict(plan, pid=os.getpid(), running=False, error=None, tx_count=0,
                 rx_bytes=0, phase='opening', speed_kph=0, odo_km=args.odo,
                 distance_km=0.0, completed_cycles=0, timing_gaps=0,
                 source='synthetic PC driving scenario; real physical UART',
                 receive_verification='not inferred from PC transmission')
    uart = serial.Serial(port=None, baudrate=115200, bytesize=8, parity='N',
                         stopbits=1, timeout=.005, write_timeout=2)
    uart.dtr = False
    uart.rts = False
    uart.port = args.port

    def publish():
        """Bounded atomic diagnostics; file sharing errors never stop serial TX."""
        state['log_write_failures'] = dashboard_uart.log_write_failures
        dashboard_uart.write_json(out / 'status.json', state)
        dashboard_uart.write_json(out / 'uart.json', dict(state, events=list(events)))

    def transmit(speed, odo, elapsed, phase):
        """Encode the existing measured frame and log only full serial writes."""
        packet = dashboard_uart.frame(speed, odo)
        if uart.write(packet) != len(packet):
            raise OSError('Short UART frame write')
        state['tx_count'] += 1
        events.append({'s': elapsed, 'direction': 'TX', 'phase': phase,
                       'hex': packet.hex(' ')})

    started = time.monotonic()
    try:
        uart.open()
        state['running'] = True
        started = next_tx = time.monotonic()
        next_report = started
        previous_time = None
        previous_speed = 0
        distance = 0.0
        while not (out / 'stop').exists():
            now = time.monotonic()
            elapsed = now - started
            if args.seconds and elapsed >= args.seconds:
                break
            if now >= next_tx:
                phase, speed, cycle = sample(elapsed)
                # Integrate actually transmitted speeds. A suspended PC must
                # not manufacture distance for a long interval without frames.
                if previous_time is not None:
                    dt = now - previous_time
                    if dt <= .5:
                        distance += (previous_speed + speed) * .5 * dt / 3600
                    else:
                        state['timing_gaps'] += 1
                odo = args.odo + int(distance)
                transmit(speed, odo, elapsed, phase)
                if state['phase'] != phase:
                    print(f'cycle={cycle} {phase} speed={speed} odo={odo}', flush=True)
                state.update(phase=phase, speed_kph=speed, odo_km=odo,
                             distance_km=distance, completed_cycles=cycle)
                previous_time, previous_speed = now, speed
                # No catch-up burst: skip missed slots while retaining 10 Hz
                # absolute scheduling during normal operation.
                next_tx += PERIOD
                if next_tx <= now:
                    next_tx = now + PERIOD
            received = uart.read(4096)
            if received:
                state['rx_bytes'] += len(received)
                events.append({'s': time.monotonic() - started, 'direction': 'RX',
                               'hex': received.hex(' ')})
            if now >= next_report:
                state.update(elapsed_seconds=elapsed, updated_unix=time.time())
                publish()
                next_report = now + 1
    except Exception as error:
        state['error'] = str(error)
        raise
    finally:
        try:
            if uart.is_open:
                transmit(0, state['odo_km'], time.monotonic() - started, 'final_stop')
                uart.flush()
                state['speed_kph'] = 0
        except Exception as error:
            state['final_stop_error'] = str(error)
        finally:
            uart.close()
            state.update(running=False, phase='closed', updated_unix=time.time(),
                         elapsed_seconds=time.monotonic() - started)
            publish()


if __name__ == '__main__':
    main()
