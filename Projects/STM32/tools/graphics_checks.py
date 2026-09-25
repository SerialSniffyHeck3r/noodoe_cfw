"""Pure, hardware-free decoding contracts for the version-1 graphics diagnostics.

Keep this module independent of the programmer CLI. Both the install workflow and
the read-only graphics probe use these checks, and synthetic snapshots can test
failure handling without issuing any device commands.
"""
from __future__ import annotations

GRAPHICS_FIELDS = (
    'magic version sample_seq stage last_error initialized '
    'process_count last_process_ms last_process_duration_ms max_process_duration_ms '
    'render_count render_busy render_started_ms last_render_ms '
    'last_render_duration_ms max_render_duration_ms '
    'heap_total heap_free heap_largest heap_used heap_peak heap_fragmentation '
    'eve_frames eve_cmd_read eve_cmd_write eve_cmd_dl eve_cpu_reset eve_pclk '
    'hardware_sample_ms ramg_used ramg_entries cache_resets '
    'input_pressed_mask input_boot_held_mask input_events input_dropped '
    'spi_tx_bytes spi_rx_bytes spi_failures log_warnings'
).split()
GRAPHICS_TEST_FIELDS = (
    'magic version initialized current_case case_count state auto_advance '
    'case_started_ms process_count transition_count rendered_frames last_frame_sequence '
    'button_events blocked_button_commands boot_held_mask ui_events allocation_failures demo_speed'
).split()
GRAPHICS_RESULT_FIELDS = 'id state visits rendered_visits frames ui_events allocation_failures'.split()
GRAPHICS_PERFORMANCE_FIELDS = ('magic version valid fps_tenths cpu_tenths target_fps_milli '
    'continuous window_ms window_frames frame_slots_missed cpu_window_cycles idle_window_cycles').split()
GRAPHICS_LAYOUTS = {
    'graphics': ('g_graphics', GRAPHICS_FIELDS),
    'graphics_test': ('g_graphics_test', GRAPHICS_TEST_FIELDS),
}


def require(condition: bool, message: str) -> None:
    """Explicit checks must still run when Python is invoked with -O."""
    if not condition:
        raise RuntimeError(message)


def graphics_symbol_layout(symbols: dict) -> dict:
    """Resolve the two exact header layouts before interpreting RAM addresses.

    The manifest came from an unstripped, validated ELF. A runtime probe must
    independently match the device APP hash before using these addresses. This
    function does no I/O and is also used by --plan to show every byte offset.
    """
    result = {}
    for short, (symbol, fields) in GRAPHICS_LAYOUTS.items():
        require(symbol in symbols and isinstance(symbols[symbol], dict), f'Missing {symbol} symbol')
        item = symbols[symbol]
        address = int(item['address'], 0) if isinstance(item['address'], str) else item['address']
        size = item['size']
        require(size == len(fields) * 4, f'{symbol}: expected {len(fields) * 4} bytes, got {size}')
        require(isinstance(address, int) and address % 4 == 0 and
                0x20000000 <= address <= 0x20030000 - size, f'{symbol}: invalid SRAM address')
        result[short] = dict(symbol=symbol, address=address, size=size,
                             fields=[dict(name=field, offset=index * 4, address=address + index * 4)
                                     for index, field in enumerate(fields)])
    if 'g_graphics_performance' in symbols:
        item = symbols['g_graphics_performance']
        address = int(item['address'], 0) if isinstance(item['address'], str) else item['address']
        require(item['size'] == 48 and address % 4 == 0 and 0x20000000 <= address <= 0x20030000-48,
                'Invalid graphics performance ABI/address')
        result['graphics_performance'] = dict(symbol='g_graphics_performance', address=address, size=48,
            fields=[dict(name=field, offset=index*4, address=address+index*4)
                    for index, field in enumerate(GRAPHICS_PERFORMANCE_FIELDS)])
    symbol = 'g_graphics_test_results'
    if symbol in symbols:
        item = symbols[symbol]
        size = item['size']
        address = int(item['address'], 0) if isinstance(item['address'], str) else item['address']
        require(isinstance(size, int) and 28 <= size <= 128 * 28 and size % 28 == 0,
                f'{symbol}: expected 1..128 records of 28 bytes')
        require(isinstance(address, int) and address % 4 == 0 and
                0x20000000 <= address <= 0x20030000 - size, f'{symbol}: invalid SRAM address')
        result['graphics_test_results'] = dict(symbol=symbol, address=address, size=size,
            record_size=28, record_count=size // 28,
            fields=[dict(name=field, offset=index * 4, address=address + index * 4)
                    for index, field in enumerate(GRAPHICS_RESULT_FIELDS)])
    return result


def graphics_snapshot_stable(snapshot: dict) -> bool:
    """An odd publisher sequence or active render is a retry, not a GPU fault.

    The reader copies g_graphics twice while the CPU is halted. Equality detects
    unexpected writers/reset despite the halt; an even sequence additionally
    proves that the CPU was not stopped halfway through publishing the sample.
    g_graphics_test has no sequence: RUNNING during screen transition is valid.
    """
    graphics = snapshot['graphics']
    return (snapshot.get('graphics_sample_match') is True and
            graphics['sample_seq'] % 2 == 0 and graphics['render_busy'] == 0)


def check_graphics_snapshot(snapshot: dict, expected_brightness: int = 25) -> dict:
    """Validate completed graphics samples without inheriting legacy blink rules.

    FIFO read/write inequality can be normal GPU work. Only the explicit fault
    sentinel/invalid register range fails immediately; stalled pending work is
    examined across samples. Warnings and cache resets are evidence, not generic
    fatal errors: deliberately unsupported demonstrations can log warnings.
    """
    require(0 <= expected_brightness <= 99, 'Expected brightness must be 0..99')
    g, test = snapshot['graphics'], snapshot['graphics_test']
    require(graphics_snapshot_stable(snapshot), 'Graphics publisher/render is in progress; recapture')
    require(g['magic'] == 0x47524131 and g['version'] == 1, 'Unknown graphics diagnostic version')
    require(g['initialized'] == 1 and g['stage'] == 4 and g['last_error'] == 0,
            f'Graphics did not reach READY: {g}')
    require(g['process_count'] > 0 and g['render_count'] > 0, 'No graphics processing/completed render')
    require(g['spi_failures'] == 0, f'Graphics SPI errors: {g["spi_failures"]}')
    require(g['input_dropped'] == 0, f'Graphics input queue dropped events: {g["input_dropped"]}')
    if 'graphics_performance' in snapshot:
        performance=snapshot['graphics_performance']
        require(performance['magic'] == 0x47504631 and performance['version'] == 1,
                'Invalid graphics performance schema')
        require(performance['valid'] in (0,1) and performance['cpu_tenths'] <= 1000 and
                performance['fps_tenths'] <= 1000, 'Invalid CPU/FPS measurement')
        # 시작1초 이전은 아직 측정치가 없다. 목표 FPS 충족 여부는 기간별 결과로
        # 따로 판정하며 낮은 FPS를 GPU 고장/장치 읽기 실패로 감추지 않는다.
    require(0 < g['heap_total'] <= 192 * 1024 and 0 < g['heap_free'] <= g['heap_total'],
            'Invalid/exhausted LVGL heap')
    require(g['heap_largest'] <= g['heap_free'] and g['heap_used'] <= g['heap_total'] and
            g['heap_peak'] <= g['heap_total'] and g['heap_fragmentation'] <= 100,
            'Invalid LVGL allocator diagnostics')
    require(g['ramg_used'] <= 1024 * 1024, 'EVE asset usage exceeds RAM_G 1 MiB')
    require(g['eve_cpu_reset'] == 0 and g['eve_pclk'] == 3, 'EVE reset/PCLK differs from the working panel')
    for field in ('eve_cmd_read', 'eve_cmd_write'):
        value = g[field]
        require(value != 0xFFF, f'EVE coprocessor fault sentinel in {field}')
        require(value <= 0xFFC and value % 4 == 0, f'Invalid EVE FIFO pointer {field}={value:#x}')
    require(g['eve_cmd_dl'] <= 8192 and g['eve_cmd_dl'] % 4 == 0,
            'EVE display-list byte offset exceeds/alignment differs from RAM_DL 8 KiB')
    require(g['eve_frames'] > 0, 'No observed EVE scanout')
    sample_age = (g['last_process_ms'] - g['hardware_sample_ms']) & 0xFFFFFFFF
    require(sample_age <= 5000, f'GPU health sample is stale by {sample_age} active ms')

    require(test['magic'] == 0x47545354 and test['version'] == 1 and test['initialized'] == 1,
            'GraphicsTest did not initialize')
    require(1 <= test['case_count'] <= 256 and test['current_case'] < test['case_count'],
            'GraphicsTest case index is out of bounds')
    require(test['state'] in (1, 2, 3), f'GraphicsTest case failed/unvisited: {test}')
    require(test['allocation_failures'] == 0, 'GraphicsTest reported allocation failure')
    require(test['process_count'] > 0 and test['rendered_frames'] > 0,
            'GraphicsTest has no service or notified completed frame')
    if 'graphics_test_results' in snapshot:
        records = snapshot['graphics_test_results']
        require(len(records) == test['case_count'] and len(records) <= 128,
                'Persistent graphics result count differs from the case table')
        for index, record in enumerate(records):
            require(record['id'] == index and record['state'] in (0, 1, 2, 3),
                    f'Invalid/failed persistent graphics case {index}: {record}')
            require(record['allocation_failures'] == 0, f'Graphics case {index} allocation failed')

    # The LVGL renderer uses EVE directly after BSP startup. Its FIFO/DL fields
    # above replace legacy g_bsp_eve.dl_words and the old LCDTest transition count.
    panel, backlight, timer = (snapshot[name] for name in ('panel', 'backlight', 'tim5'))
    require(panel['magic'] == 0x504E4C31 and panel['ready'] == 1 and panel['stage'] == 7 and
            panel['result'] == 0 and panel['power_mode'] == 0x9C, 'Panel is not display-on/ready')
    require(backlight['magic'] == 0x424C5431 and backlight['initialized'] == 1 and
            backlight['result'] == 0 and backlight['timer_hz'] == 84000000,
            'Backlight initialization failed')
    require(timer['psc'] == 1679 and timer['arr'] == 99 and timer['ccr4'] == expected_brightness,
            f'Unexpected graphics PWM setting: {timer}')
    require((timer['ccmr2'] >> 12) & 7 == 6 and timer['ccer'] & (1 << 13) == 0 and
            timer['cr1'] & 1 and timer['ccer'] & (1 << 12), 'Graphics PWM is not enabled, active-high PWM1')
    require(snapshot['backlight_gpio'] == dict(pi0_mode=2, pi0_af=2, pi8_level=1, pc8_level=0),
            'Backlight GPIO/AF differs from the working configuration')
    if 'buttons' in snapshot:
        buttons = snapshot['buttons']
        require(buttons['magic'] == 0x42544E31 and buttons['version'] == 1 and
                buttons['initialized'] == 1 and buttons['process_count'] > 0,
                'Graphics button service did not initialize')
        require(buttons['event_overflow_count'] == 0, 'Graphics stopped consuming button events')
        for button, port in [('up', 3), ('enter', 0), ('down', 8)]:
            item = snapshot['button_gpio'][button]
            require(item['mode'] == 0 and item['pull'] == 0 and item['exti_port'] == port,
                    f'Incorrect {button} GPIO/EXTI configuration')
        mask = (1 << 12) | (1 << 15) | (1 << 6)
        require(all(snapshot['button_exti'][field] & mask == mask for field in ('imr', 'rtsr', 'ftsr')),
                'Graphics button EXTI must preserve both edges and pin routing')
    return dict(fifo_pending=g['eve_cmd_read'] != g['eve_cmd_write'],
                gpu_sample_age_ms=sample_age, current_case=test['current_case'],
                current_case_state=test['state'], visual_confirmation=False)


def delta32(after: int, before: int) -> int:
    """Modulo-32 delta supports long-running counters across one wrap."""
    return (after - before) & 0xFFFFFFFF


def check_graphics_progress(first: dict, second: dict, require_case_advance: bool = False) -> dict:
    """Require service/scanout progress; a static page need not redraw every poll.

    Automatic testcase runs can additionally require a screen transition and a
    notified completed frame. The interval must exceed the UI's case period for
    that stronger requirement. This is test coverage, not visual correctness.
    """
    before, after = first['graphics'], second['graphics']
    tb, ta = first['graphics_test'], second['graphics_test']
    fields = ('process_count', 'render_count', 'eve_frames', 'last_process_ms',
              'hardware_sample_ms', 'spi_tx_bytes', 'spi_rx_bytes', 'cache_resets')
    delta = {field: delta32(after[field], before[field]) for field in fields}
    for field in ('process_count', 'eve_frames', 'last_process_ms', 'hardware_sample_ms'):
        require(0 < delta[field] < 0x80000000, f'Graphics {field} did not progress or restarted')
    test_delta = {field: delta32(ta[field], tb[field])
                  for field in ('process_count', 'transition_count', 'rendered_frames')}
    require(0 < test_delta['process_count'] < 0x80000000, 'GraphicsTest service stopped/restarted')
    require(ta['case_count'] == tb['case_count'], 'GraphicsTest case table changed during observation')
    if require_case_advance:
        require(tb['auto_advance'] == 1 and ta['auto_advance'] == 1, 'Automatic case advance is disabled')
        require(0 < test_delta['transition_count'] < 0x80000000, 'Automatic GraphicsTest did not change case')
        require(0 < test_delta['rendered_frames'] < 0x80000000 and 0 < delta['render_count'] < 0x80000000,
                'Changed graphics cases produced no completed render')
    pending_before = before['eve_cmd_read'] != before['eve_cmd_write']
    pending_after = after['eve_cmd_read'] != after['eve_cmd_write']
    same_fifo = all(before[field] == after[field] for field in ('eve_cmd_read', 'eve_cmd_write'))
    require(not (pending_before and pending_after and same_fifo and delta['render_count'] == 0 and
                 delta['last_process_ms'] >= 1000), 'EVE command FIFO remained stalled for at least 1 active second')
    return dict(graphics=delta, test=test_delta,
                observed_render_fps=delta['render_count'] * 1000.0 / delta['last_process_ms'])
