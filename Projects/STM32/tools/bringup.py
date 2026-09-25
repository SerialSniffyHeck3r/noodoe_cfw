"""APP-only install and repeatable board test; no GUI, mass erase or option writes.

Every subprocess uses an argument array (never a shell command). Flash writes have
one fixed address and a validated ELF-derived binary. A failed step leaves its
logs and stops the sequence; a failure is never converted into a successful test.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
import time
from verified_flash import reconcile, read_expected
from graphics_checks import (GRAPHICS_FIELDS, GRAPHICS_TEST_FIELDS, GRAPHICS_RESULT_FIELDS, GRAPHICS_PERFORMANCE_FIELDS, graphics_symbol_layout,
                             graphics_snapshot_stable, check_graphics_snapshot,
                             check_graphics_progress)

PROJECT = Path(__file__).resolve().parents[1]
RESEARCH = PROJECT.parents[1]
CLI = Path(r'C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe')
APP = 0x08010000
FLASH = 0x08000000
FLASH_SIZE = 0x80000
PRESERVED = 0x10000
STOCK_SHA256 = '38207bed741a8fef9e6d2ed24b86c8cb8b376ba5e4c957163f5edd83fbeb8037'
# SystemClock_Config 이후의 정상 운용 목표다. bsp_boot가 C 초기화 전에 만드는
# HSI 16MHz 기준과 별개이며, 새 IOC 생성본을 검증하기 전에는 이 시험을 통과할 수 없다.
EXPECTED_RUNTIME_CLOCK_HZ = 168_000_000


def require(condition: bool, message: str) -> None:
    """Use an explicit exception: Python -O must not remove safety checks."""
    if not condition:
        raise RuntimeError(message)


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def run(command: list[str], log: Path, timeout: int = 240) -> str:
    """Write commands/results locally; a timeout kills this CLI, not another process."""
    print(f'Running {log.name}', flush=True)
    with log.open('w', encoding='utf-8') as stream:
        stream.write(json.dumps(command, ensure_ascii=False) + '\n')
        stream.flush()
        result = subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT,
                                timeout=timeout, check=False)
    text = log.read_text(encoding='utf-8', errors='replace')
    require(result.returncode == 0, f'Command failed ({result.returncode}): {log}')
    # Some programmer failures have been reported with a successful process exit.
    require(not re.search(r'(?im)^\s*(?:Error:|Error\s*:|No STM32 target)', text),
            f'Programmer reported an error: {log}')
    return text


class Board:
    """One owner/one sequential connection; explicit conservative SWD clock."""
    def __init__(self, serial: str, folder: Path, frequency_khz: int = 100):
        require(frequency_khz in (50, 100), 'Supported SWD clocks: 50/100 kHz')
        self.frequency_khz = frequency_khz
        self.read_frequency_khz = frequency_khz
        self.live_flash_reads = False
        self.serial, self.folder = serial, folder

    def command(self, name: str, *operations: str, timeout: int = 240) -> str:
        # Faster clocks apply only to read uploads. Flash writes and register
        # control keep their conservative independently configured frequency.
        frequency=self.read_frequency_khz if '-u' in operations else self.frequency_khz
        return run([str(CLI), '-c', 'port=SWD', f'sn={self.serial}',
                    f'freq={frequency}', 'mode=HOTPLUG', *operations],
                   self.folder / f'{name}.log', timeout)

    def resume(self, name: str = 'resume') -> None:
        self.command(name, '-run')

    def program(self, image: Path) -> None:
        """Use a normal software-reset connection for the programmer's RAM loader.

        HOTPLUG is correct for inspection but the stock RTOS CPU/peripheral state
        caused this probe's sector erase to fail. NORMAL/SWrst succeeded on the
        same target/image at 100 kHz. Inspection continues to use HOTPLUG.
        """
        payload=image.read_bytes()
        require(len(payload)>=8,'Missing APP vectors')
        sp,rv=struct.unpack_from('<II',payload)
        require(not(sp==0x2002ff00 and rv>=0x08020000),
                'Layout2 Product binary must never be written at legacy APP address; use gate-aware installer')
        run([str(CLI), '-c', 'port=SWD', f'sn={self.serial}', f'freq={self.frequency_khz}',
             'mode=NORMAL', 'reset=SWrst', '-halt', '-w', str(image), hex(APP), '-v'],
            self.folder / 'program-app.log')

    def dump(self, name: str, address: int, size: int, resume: bool = True) -> bytes:
        """Freeze watchdogs/TIM6 when halted, read, then resume if requested.

        DBGMCU_APB1_FZ is a debug register, not a persistent option byte. We set
        only watchdog/TIM6-freeze bits using its current value in prepare().
        All dumps use a halted core to eliminate inconsistent RAM snapshots.
        """
        path = self.folder / f'{name}.bin'
        require(not path.exists(), f'Refusing to reuse an old dump: {path}')
        live=self.live_flash_reads and FLASH<=address<address+size<=FLASH+FLASH_SIZE
        ops = ([] if live else ['-halt']) + ['-u', hex(address), hex(size), str(path)]
        if resume and not live:
            ops += ['-run']
        self.command(name, *ops)
        require(path.exists() and path.stat().st_size == size, f'Incomplete dump: {path}')
        return path.read_bytes()

    def prepare(self) -> None:
        """Freeze watchdogs and HAL TIM6 during debug halt; prove identity.

        SysTick stops automatically on halt. Leaving TIM6 running accumulates
        a pending update across every snapshot/retry and skews tick deltas.
        Bit4 changes debug-halt behavior only, not the running timer frequency.
        Preserve the original word for the existing successful-run restore.
        """
        info = self.command('target', '-r32', '0xE0042008', '4', '-ob', 'displ')
        require(re.search(r'Device ID\s*:\s*0x419\b', info), 'Unexpected MCU device ID')
        require(re.search(r'(?:Flash|NVM) size\s*:\s*512\s*KBytes', info), 'Expected 512 KiB target')
        require(re.search(r'RDP\s*:\s*0xAA\b', info, re.I), 'Expected RDP0; never unlock automatically')
        values = re.findall(r'0xE0042008\s*:\s*([0-9A-Fa-f]{8})', info, re.I)
        require(len(values) == 1, 'Cannot parse DBGMCU_APB1_FZ')
        self.freeze_before = int(values[0], 16)
        self.command('watchdog-freeze', '-w32', '0xE0042008', hex(self.freeze_before | 0x1810))

    def snapshot(self, name: str, symbols: dict, lcd_test: bool = False,
                 graphics_test: bool = False) -> dict:
        """One halt gives mutually consistent diagnostic structs and registers."""
        require(not (lcd_test and graphics_test), 'Select one display validation profile')
        display_test = lcd_test or graphics_test
        specs = [('boot', 'g_bsp_boot', 220), ('runtime', 'g_bsp_bringup', 56),
                 ('fault', 'g_bsp_fault', 40)]
        if lcd_test:
            require('g_bsp_lcd_test' in symbols, 'LCDTest is not linked in this ELF')
            require(symbols['g_bsp_lcd_test']['size'] == 52, 'Unknown LCD diagnostic layout')
            specs += [('lcd', 'g_bsp_lcd_test', 52)]
        if graphics_test:
            # ELF의 크기/주소를 먼저 검사한다. 그래픽 모드에는 GC로 제거될 수
            # 있는 legacy LCDTest나 facade 진단을 필수로 요구하지 않는다.
            layout = graphics_symbol_layout(symbols)
            specs += [(short, item['symbol'], item['size']) for short, item in layout.items()]
        if display_test:
            for short, symbol in [('eve', 'g_bsp_eve'), ('panel', 'g_bsp_lcd_panel'),
                                  ('backlight', 'g_bsp_backlight')]:
                size = symbols[symbol]['size']
                require(0 < size <= 256 and size % 4 == 0, f'Unknown {symbol} size')
                specs.append((short, symbol, size))
            # These are optional in older LCD-only images. Keep their complete
            # versioned words even if a future diagnostic layout is unfamiliar.
            for short, symbol in [('buttons', 'g_bsp_buttons'),
                                  ('display', 'g_bsp_display'),
                                  ('lcd_buttons', 'g_bsp_lcd_buttons')]:
                if symbol in symbols:
                    size = symbols[symbol]['size']
                    require(0 < size <= 1024 and size % 4 == 0, f'Unknown {symbol} size')
                    specs.append((short, symbol, size))
        operations = ['-halt']
        for short, symbol, size in specs:
            addr = symbols[symbol]
            if isinstance(addr, dict):
                addr = addr['address']
            if isinstance(addr, str):
                addr = int(addr, 0)
            require(0x20000000 <= addr <= 0x20030000 - size, f'Invalid diagnostic address {symbol}')
            path = self.folder / f'{name}-{short}.bin'
            require(not path.exists(), f'Old snapshot exists: {path}')
            operations += ['-u', hex(addr), str(size), str(path)]
        if display_test:
            timer_path = self.folder / f'{name}-tim5.bin'
            require(not timer_path.exists(), 'Refusing old TIM5 register snapshot')
            operations += ['-u', '0x40000C00', '0x44', str(timer_path)]
            operations += ['-u', '0x40022000', '0x24', str(self.folder / f'{name}-gpioi.bin'),
                           '-u', '0x40020800', '0x18', str(self.folder / f'{name}-gpioc.bin')]
            if 'g_bsp_buttons' in symbols:
                # GPIO input configuration and EXTI ownership prove the real
                # stock button routing independently of C diagnostic variables.
                for short, address, size in [('gpioa', 0x40020000, 0x18),
                                             ('gpiod', 0x40020C00, 0x18),
                                             ('exti', 0x40013C00, 0x18),
                                             ('syscfg', 0x40013808, 0x10)]:
                    path = self.folder / f'{name}-{short}.bin'
                    require(not path.exists(), f'Refusing old {short} register snapshot')
                    operations += ['-u', hex(address), hex(size), str(path)]
        if graphics_test:
            # CPU는 첫 복사부터 끝까지 halt 상태다. 두 복사가 같아도 홀수
            # sample_seq라면 C가 갱신 중 멈춘 것이므로 resume 후 재시도한다.
            tail = self.folder / f'{name}-graphics-tail.bin'
            require(not tail.exists(), f'Old graphics tail exists: {tail}')
            operations += ['-u', hex(layout['graphics']['address']),
                           str(layout['graphics']['size']), str(tail)]
        operations += ['-coreReg', '-r32', '0xE000ED08', '4', '-r32', '0x40023800', '16', '-run']
        self.command(name, *operations)
        fields = {
            'graphics': GRAPHICS_FIELDS,
            'graphics_test': GRAPHICS_TEST_FIELDS,
            'graphics_performance': GRAPHICS_PERFORMANCE_FIELDS,
            'runtime': 'magic stage heartbeat hal_tick kernel_tick tim6_interrupts core_clock vtor control basepri primask free_heap min_free_heap stack_free_words'.split(),
            'fault': 'magic code ipsr cfsr hfsr dfsr mmfar bfar msp psp'.split(),
            'lcd': 'magic version stage error hal_tick transitions backlight_on requested_percent init_attempts last_result core_clock wait_errors eve_frames'.split(),
            'eve': 'magic version phase result spi_status spi_error last_address last_value reg_id cpu_reset chip_id frequency_hz frames dl_swap dl_words pclk transfer_count ready'.split(),
            'panel': 'magic stage failed_stage result spi_error board_revision protocol_selector last_command last_tx_word last_rx_word power_mode write_frames read_frames init_attempts ready'.split(),
            'backlight': 'magic stage result initialized percent timer_hz prescaler period compare updates'.split(),
            'display': 'magic version stage last_result backend_result initialized width height frame_open frame_error word_count frames_presented requested_brightness actual_brightness last_frames failures failed_stage'.split(),
            'buttons': ('magic version initialized process_count last_process_ms pressed_mask raw_pressed_mask '
                        'irq_count irq_pending_mask irq_raw_pressed_mask last_processed_irq_mask '
                        'events_enqueued events_pending event_overflow_count event_sequence').split()
                       + [f'{field}_{button}' for field in ('irq', 'press', 'release', 'current_ms', 'last_ms', 'class')
                          for button in ('up', 'down', 'enter')],
            'lcd_buttons': ('magic version service_count redraws pressed_mask long_mask very_long_mask '
                            'last_button last_event last_duration_ms consumed_events').split()
                           + [f'{field}_{button}' for field in ('last_ms', 'short', 'long', 'very_long')
                              for button in ('up', 'down', 'enter')],
        }
        decoded = {}
        for short, symbol, size in specs:
            data = (self.folder / f'{name}-{short}.bin').read_bytes()
            require(len(data) == size, f'Incomplete {short} snapshot')
            words = list(struct.unpack(f'<{size // 4}I', data))
            if short == 'boot':
                # Layout is versioned in bsp_boot.h. Preserve all words for later
                # forensic work; extract only this phase's acceptance fields.
                decoded[short] = dict(magic=words[0], version=words[1], status=words[2],
                                     error_detail=words[3], entry_control=words[4],
                                     entry_basepri=words[5], entry_ipsr=words[6],
                                     entry_vtor=words[7], entry_rcc_cr=words[8],
                                     entry_rcc_pllcfgr=words[9], entry_rcc_cfgr=words[10],
                                     runtime_vtor=words[52], runtime_primask=words[53],
                                     runtime_system_core_clock=words[54], words=words)
            elif short == 'graphics_test_results':
                require(len(words) % 7 == 0 and 1 <= len(words) // 7 <= 128,
                        'Unknown persistent graphics result layout')
                decoded[short] = [dict(zip(GRAPHICS_RESULT_FIELDS, words[offset:offset + 7]))
                                  for offset in range(0, len(words), 7)]
            elif short in fields:
                require(len(fields[short]) == len(words), f'Unknown {short} diagnostic layout')
                decoded[short] = dict(zip(fields[short], words))
            else:
                decoded[short] = {'words': words, 'symbol': symbol, 'size': size}
        if graphics_test:
            first_bytes = (self.folder / f'{name}-graphics.bin').read_bytes()
            tail_bytes = tail.read_bytes()
            require(len(tail_bytes) == layout['graphics']['size'], 'Incomplete graphics tail')
            decoded['graphics_sample_match'] = first_bytes == tail_bytes
        if display_test:
            data = timer_path.read_bytes()
            require(len(data) == 0x44, 'Incomplete TIM5 snapshot')
            words = struct.unpack('<17I', data)
            decoded['tim5'] = dict(cr1=words[0], ccmr2=words[7], ccer=words[8],
                                   psc=words[10], arr=words[11], ccr4=words[16])
            gpioi = struct.unpack('<9I', (self.folder / f'{name}-gpioi.bin').read_bytes())
            gpioc = struct.unpack('<6I', (self.folder / f'{name}-gpioc.bin').read_bytes())
            decoded['backlight_gpio'] = dict(pi0_mode=gpioi[0] & 3, pi0_af=gpioi[8] & 15,
                                             pi8_level=(gpioi[5] >> 8) & 1,
                                             pc8_level=(gpioc[5] >> 8) & 1)
            if 'g_bsp_buttons' in symbols:
                gpioa = struct.unpack('<6I', (self.folder / f'{name}-gpioa.bin').read_bytes())
                gpiod = struct.unpack('<6I', (self.folder / f'{name}-gpiod.bin').read_bytes())
                exti = struct.unpack('<6I', (self.folder / f'{name}-exti.bin').read_bytes())
                routing = struct.unpack('<4I', (self.folder / f'{name}-syscfg.bin').read_bytes())
                decoded['button_gpio'] = {
                    button: {'mode': (words[0] >> (pin * 2)) & 3,
                             'pull': (words[3] >> (pin * 2)) & 3,
                             'level': (words[4] >> pin) & 1,
                             'exti_port': (routing[pin // 4] >> ((pin % 4) * 4)) & 15}
                    for button, words, pin in [('up', gpiod, 12), ('enter', gpioa, 15),
                                               ('down', gpioi, 6)]}
                decoded['button_exti'] = dict(zip(('imr', 'emr', 'rtsr', 'ftsr', 'swier', 'pr'), exti))
        (self.folder / f'{name}.json').write_text(json.dumps(decoded, indent=2), encoding='utf-8')
        return decoded

    def graphics_snapshot(self, name: str, symbols: dict, attempts: int = 8) -> dict:
        """중간 렌더 표본은 제한적으로 재시도하고 실제 fault는 바로 중단한다.

        각 시도는 CPU를 다시 실행시킨다. freeze 이후 홀수 sequence가 계속
        보이면 파일을 덮지 않고 모든 증거를 보존한 채 실패한다. raw 구조체를
        여러 번 읽었다는 사실은 화면의 육안 정확성을 증명하지 않는다.
        """
        require(1 <= attempts <= 20, 'Invalid graphics snapshot attempt limit')
        capture_trace = []
        for index in range(attempts):
            snapshot = self.snapshot(f'{name}-try{index + 1}', symbols, graphics_test=True)
            # 실패한 렌더 표본도 실제 halt/run 한 번이다. 최종 두 표본 사이의
            # tick 차이에 이 정지들이 누적되므로 원래 runtime 값을 모두 보존한다.
            # graphics sequence는 runtime publisher의 원자성을 보장하지 않는다.
            capture_trace.append(dict(evidence=f'{name}-try{index + 1}',
                                      runtime=dict(snapshot['runtime'])))
            snapshot['capture_attempts'] = index + 1
            snapshot['evidence_name'] = f'{name}-try{index + 1}'
            snapshot['capture_trace'] = list(capture_trace)
            (self.folder / f'{name}-try{index + 1}.json').write_text(
                json.dumps(snapshot, indent=2), encoding='utf-8')
            check_snapshot(snapshot)
            g, test = snapshot['graphics'], snapshot['graphics_test']
            require(g['last_error'] == 0 and g['stage'] != 0x80000000,
                    f'Graphics reported failure: {g}')
            require(test['allocation_failures'] == 0 and test['state'] != 4,
                    f'GraphicsTest reported failure: {test}')
            if graphics_snapshot_stable(snapshot):
                return snapshot
            print(f'{name}: graphics publisher/render busy; resuming before recapture', flush=True)
            # 주기 렌더의 같은 위상에서 계속 halt하지 않도록 5ms service
            # 주기와 서로 다른 간격을 쓴다. 8회 실패는 샘플 확보 실패이며
            # 그 사실만으로 GPU 고장이나 펌웨어 손상을 뜻하지 않는다.
            time.sleep((0.029, 0.047, 0.071, 0.113, 0.037, 0.089, 0.061)[index % 7])
        raise RuntimeError(f'{name}: no complete graphics sample in {attempts} attempts')


def check_snapshot(snapshot: dict) -> None:
    boot, runtime, fault = (snapshot[name] for name in ('boot', 'runtime', 'fault'))
    require(boot['magic'] == 0x424F4F54 and boot['version'] == 1 and boot['status'] == 0x20,
            f'Boot normalization failed: {boot}')
    require(boot['entry_vtor'] == 0x20000000, 'Reset did not demonstrate original BL SRAM vector handoff')
    require(boot['runtime_vtor'] == APP and boot['runtime_primask'] == 1,
            'Incorrect VTOR/IRQ state at pre-HAL boundary')
    require(runtime['magic'] == 0x42525550 and runtime['stage'] == 6, f'RTOS task did not start: {runtime}')
    require(runtime['vtor'] == APP and runtime['core_clock'] == EXPECTED_RUNTIME_CLOCK_HZ,
            f'Wrong runtime clock/vector; expected APP VTOR and {EXPECTED_RUNTIME_CLOCK_HZ} Hz after SystemClock_Config')
    # Cortex-M4 CONTROL[2](FPCA)는 부동소수점 문맥이 활성화되면 정상적으로
    # 켜진다. nPRIV=0, SPSEL=1 계약은 그대로이며 FPCA만 0/1 모두 허용한다.
    # 예약 비트를 포함한 다른 값은 거부하여 단순 low-bit 비교로 숨기지 않는다.
    control = runtime['control']
    require((control & 0x3) == 0x2 and (control & ~0x7) == 0 and
            runtime['basepri'] == 0 and runtime['primask'] == 0,
            'Expected privileged PSP thread (CONTROL 2 or 6) with interrupts enabled')
    require(fault['magic'] == 0 and fault['code'] == 0, f'Fault recorded: {fault}')
    require(runtime['min_free_heap'] > 0 and runtime['stack_free_words'] > 32, 'Insufficient observed heap/stack margin')


def check_lcd_snapshot(snapshot: dict) -> None:
    """Separate device scanout and PWM evidence from generic MCU liveness.

    The raw DDIC/EVE structs are retained for diagnosis. This contract uses the
    versioned LCDTest status and actual timer registers; visible panel output
    still requires the user's observation and is reported separately.
    """
    lcd, timer = snapshot['lcd'], snapshot['tim5']
    require(lcd['magic'] == 0x4C434454 and lcd['version'] == 1, 'Unknown LCDTest status')
    require(lcd['stage'] == 6 and lcd['error'] == 0 and lcd['wait_errors'] == 0,
            f'LCD initialization or scanout failed: {lcd}')
    require(lcd['core_clock'] == EXPECTED_RUNTIME_CLOCK_HZ and lcd['init_attempts'] == 1,
            'Unexpected LCD clock or repeated initialization')
    require(lcd['transitions'] > 0 and lcd['eve_frames'] > 0, 'No PWM/scanout activity')
    require(timer['psc'] == 1679 and timer['arr'] == 99, f'Wrong PWM frequency: {timer}')
    require(timer['ccr4'] in (0, 25), f'Expected off or exactly 25% PWM: {timer}')
    require((timer['ccmr2'] >> 12) & 7 == 6, 'TIM5 channel4 is not PWM1')
    require(timer['ccer'] & (1 << 13) == 0, 'TIM5 channel4 is not active-high')
    require(timer['cr1'] & 1 and timer['ccer'] & (1 << 12), 'PWM timer/output is not enabled')
    require(snapshot['backlight_gpio'] == {'pi0_mode': 2, 'pi0_af': 2, 'pi8_level': 1, 'pc8_level': 0},
            f'Backlight GPIO/AF differs from the working configuration: {snapshot["backlight_gpio"]}')
    eve, panel, backlight = (snapshot[name] for name in ('eve', 'panel', 'backlight'))
    require(eve['magic'] == 0x45564531 and eve['ready'] == 1 and eve['result'] == 0
            and eve['reg_id'] == 0x7C and eve['cpu_reset'] == 0 and eve['pclk'] == 3
            and eve['dl_swap'] == 0 and eve['dl_words'] > 3,
            f'EVE did not accept the test display list: {eve}')
    require(panel['magic'] == 0x504E4C31 and panel['ready'] == 1 and panel['stage'] == 7
            and panel['result'] == 0 and panel['power_mode'] == 0x9C,
            f'LCD panel did not report ready/display-on: {panel}')
    require(backlight['magic'] == 0x424C5431 and backlight['initialized'] == 1
            and backlight['result'] == 0 and backlight['timer_hz'] == 84000000,
            f'Backlight initialization failed: {backlight}')
    if 'buttons' in snapshot:
        buttons, display, visual = (snapshot[name] for name in ('buttons', 'display', 'lcd_buttons'))
        require(buttons['magic'] == 0x42544E31 and buttons['version'] == 1
                and buttons['initialized'] == 1 and buttons['process_count'] > 0,
                f'Buttons service did not initialize: {buttons}')
        require(buttons['event_overflow_count'] == 0, 'LCDTest stopped consuming button events')
        require(display['magic'] == 0x44535031 and display['version'] == 1
                and display['initialized'] == 1 and display['failures'] == 0,
                f'Display facade reported failure: {display}')
        require(visual['magic'] == 0x4C425431 and visual['version'] == 1 and visual['redraws'] > 0,
                'Button test frame has not been submitted')
        for button, port in [('up', 3), ('enter', 0), ('down', 8)]:
            actual = snapshot['button_gpio'][button]
            require(actual['mode'] == 0 and actual['pull'] == 0 and actual['exti_port'] == port,
                    f'Wrong stock {button} GPIO/EXTI routing: {actual}')
        mask = (1 << 12) | (1 << 15) | (1 << 6)
        exti = snapshot['button_exti']
        require(all(exti[field] & mask == mask for field in ('imr', 'rtsr', 'ftsr')),
                f'Button EXTI must enable both edges on PD12/PA15/PI6: {exti}')


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--serial', required=True, help='Exact ST-LINK serial; never choose an arbitrary probe')
    parser.add_argument('--configuration', choices=['Debug', 'Release'], default='Debug')
    parser.add_argument('--frequency-khz', type=int, choices=[50,100], default=100)
    parser.add_argument('--read-frequency-khz',type=int,choices=[50,100,400,950],help='Read-only upload clock; qualify against a known APP before raising')
    parser.add_argument('--output', type=Path, required=True, help='New evidence directory; existing directory rejected')
    parser.add_argument('--skip-build', action='store_true', help='Validate an already built ELF (does not skip validation)')
    parser.add_argument('--test-only', action='store_true', help='Do not program; require device image to match ELF')
    parser.add_argument('--cycles', type=int, default=3)
    profiles = parser.add_mutually_exclusive_group()
    profiles.add_argument('--lcd-test', action='store_true', help='Also prove LCDTest scanout and 25%%/off PWM')
    profiles.add_argument('--graphics-test', action='store_true', help='Prove LVGL/EVE health and automatic case progress at constant 25%% PWM')
    parser.add_argument('--graphics-pinned-case',action='store_true',help='Stay on one case; still require completed frame progress')
    parser.add_argument('--live-flash-reads',action='store_true',help='Read internal flash without halting known CFW that does not write flash')
    args = parser.parse_args()
    require(1 <= args.cycles <= 20, 'cycles must be between 1 and 20')
    require(not args.graphics_pinned_case or args.graphics_test,'Pinned case requires graphics profile')
    require(re.fullmatch(r'[A-Za-z0-9]+', args.serial), 'Invalid probe serial')
    folder = args.output.resolve()
    folder.mkdir(parents=True, exist_ok=False)
    summary = {'result': 'incomplete', 'configuration': args.configuration, 'serial': args.serial,
               'frequency_khz': args.frequency_khz, 'cycles': [], 'flash_write_attempted': False, 'flash_written': False,
               'lcd_test': args.lcd_test, 'graphics_test': args.graphics_test,
               'visual_confirmation': 'not_observed_by_tool'}
    board = Board(args.serial, folder, args.frequency_khz)
    board.read_frequency_khz=args.read_frequency_khz or args.frequency_khz
    summary['read_frequency_khz']=board.read_frequency_khz
    board.live_flash_reads=args.live_flash_reads
    summary['live_flash_reads']=args.live_flash_reads
    summary['graphics_pinned_case']=args.graphics_pinned_case
    try:
        run(['powershell.exe', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
             str(PROJECT / 'tools/check_project.ps1')], folder / 'project-check.log')
        if not args.skip_build:
            run(['powershell.exe', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
                 str(PROJECT / 'tools/build.ps1'), '-Configuration', args.configuration],
                folder / 'build.log', timeout=300)
        elf = PROJECT / args.configuration / f'{PROJECT.name}.elf'
        # Validator is a separate function-level tool. Freshly export every run;
        # never trust a stale BIN or a manifest from an earlier build.
        run([sys.executable, str(PROJECT / 'tools/validate_image.py'), str(elf)], folder / 'image-check.log')
        manifest_path = elf.parent / 'app.manifest.json'
        manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
        require(int(manifest['flash_address'],16)==APP and manifest.get('layout_version',1)==1,
                'This legacy bring-up tool cannot install/test the RecoveryGate Product layout')
        image_path = elf.parent / 'app.bin'
        image = image_path.read_bytes()
        require(8 <= len(image) <= FLASH_SIZE - PRESERVED, 'Invalid APP size')
        require(manifest['binary_sha256'] == sha(image) and manifest['binary_size'] == len(image),
                'Validator manifest does not match exported image')
        if args.graphics_test:
            summary['graphics_layout'] = graphics_symbol_layout(manifest['symbols'])
        summary['image_sha256'], summary['image_size'] = sha(image), len(image)
        (folder / 'app.bin').write_bytes(image)
        (folder / 'image-manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
        # This immutable reference ties preserved BL/config to this actual module.
        original = RESEARCH / 'VERY_IMPORTANT_ORIGINAL_NOODOE_BOOTLOARDER/noodoe_full_flash_dump_A.bin'
        original_data = original.read_bytes()
        require(len(original_data) == FLASH_SIZE and sha(original_data) == STOCK_SHA256,
                'Original backup is missing, wrong-sized, or changed')
        board.prepare()
        # Lower 64 KiB includes BL + pending-update metadata + device data.
        # No pending update may be left for the BL to install after our reset.
        before = read_expected(board, 'preserved-before', FLASH, original_data[:PRESERVED])
        require(before == original_data[:PRESERVED], 'Preserved BL/config differs from known original')
        require(struct.unpack_from('<4I', before, 0x8004) == (0, 0, 0, 0), 'Pending installer metadata present')
        summary['preserved_sha256'] = sha(before)
        if not args.test_only:
            # A stable full snapshot is the recovery image for THIS installation,
            # even if the board currently holds an earlier custom application.
            first = board.dump('before-full-a', FLASH, FLASH_SIZE, resume=False)
            second = board.dump('before-full-b', FLASH, FLASH_SIZE, resume=False)
            first,second = reconcile(board, 'before-full', FLASH, first, second)
            require(first == second and first[:PRESERVED] == before, 'Pre-flash reads do not match')
            summary['previous_full_sha256'] = sha(first)
            summary['previous_full_files'] = [str(folder / name) for name in (
                ('before-full-verified-a.bin','before-full-verified-b.bin')
                if (folder/'before-full-verification.json').exists() else ('before-full-a.bin','before-full-b.bin'))]
            # The ONLY flash programming call. Cube erases only sectors touched
            # by this APP binary, starting at sector 4; there is no erase-all.
            summary['flash_write_attempted'] = True
            board.program(folder / 'app.bin')
            summary['flash_written'] = True
        readback = read_expected(board, 'app-readback', APP, image)
        require(readback == image, 'APP readback differs from validated image')
        after = read_expected(board, 'preserved-after', FLASH, before)
        require(after == before, 'Preserved lower 64 KiB changed')
        for index in range(args.cycles):
            label = f'cycle-{index + 1}'
            # System reset goes through address 0/stock BL. Never set PC directly
            # to APP when claiming verification of the real bootloader chain.
            board.command(f'{label}-reset', '-rst', '-run')
            time.sleep(5 if args.lcd_test or args.graphics_test else 3)
            first = (board.graphics_snapshot(f'{label}-a', manifest['symbols']) if args.graphics_test
                     else board.snapshot(f'{label}-a', manifest['symbols'], args.lcd_test))
            check_snapshot(first)
            if args.lcd_test:
                check_lcd_snapshot(first)
            if args.graphics_test:
                check_graphics_snapshot(first)
            # GraphicsTest의 기본 화면 주기 8초를 넘긴다. CPU halt 시간은
            # HAL/LVGL 시간이 진행하지 않으므로 대기 간격에 포함하지 않는다.
            time.sleep(9.137 if args.graphics_test else (2.317 if args.lcd_test else 2))
            second = (board.graphics_snapshot(f'{label}-b', manifest['symbols']) if args.graphics_test
                      else board.snapshot(f'{label}-b', manifest['symbols'], args.lcd_test))
            check_snapshot(second)
            if args.lcd_test:
                check_lcd_snapshot(second)
                require(second['lcd']['transitions'] > first['lcd']['transitions'], 'Backlight blinking stopped')
                require(second['lcd']['eve_frames'] > first['lcd']['eve_frames'], 'EVE scanout stopped')
            graphics_progress = None
            if args.graphics_test:
                check_graphics_snapshot(second)
                graphics_progress = check_graphics_progress(first, second, require_case_advance=not args.graphics_pinned_case)
                if args.graphics_pinned_case:
                    require(second['graphics']['render_count']>first['graphics']['render_count'], 'Pinned graphic produced no frames')
                    require(first['graphics_test']['current_case']==second['graphics_test']['current_case']==0, 'Pinned arc case changed')
            delta = {field: second['runtime'][field] - first['runtime'][field]
                     for field in ('heartbeat', 'hal_tick', 'kernel_tick', 'tim6_interrupts')}
            require(all(value > 0 for value in delta.values()), f'No independent tick/task progress: {delta}')
            require(abs(delta['hal_tick'] - delta['kernel_tick']) <= 2,
                    f'HAL/kernel tick progress diverged: {delta}')
            summary['cycles'].append({'cycle': index + 1, 'delta': delta, 'runtime': second['runtime'],
                                     'lcd': second.get('lcd'), 'tim5': second.get('tim5'),
                                     'graphics': second.get('graphics'), 'graphics_test': second.get('graphics_test'),
                                     'graphics_progress': graphics_progress})
            print(f'{label}: PASS {delta}', flush=True)
        # Restore the prior debug freeze configuration while the core is running.
        # This avoids leaving watchdog behavior dependent on this test session.
        board.command('restore-debug-freeze', '-w32', '0xE0042008', hex(board.freeze_before), '-run')
        summary['result'] = 'pass'
        print('PASS: APP readback, preserved BL/config, reset chain, HAL tick and RTOS heartbeat.', flush=True)
        return 0
    except Exception as error:
        summary['result'], summary['error'] = 'fail', str(error)
        print(f'FAIL: {error}', file=sys.stderr, flush=True)
        # No blind automatic restore/reset: preserve a failed startup's registers
        # and RAM for diagnosis. SWD remains available; recover through APP-only
        # programming after reading the evidence, never by erasing the BL.
        return 1
    finally:
        (folder / 'summary.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')


if __name__ == '__main__':
    raise SystemExit(main())
