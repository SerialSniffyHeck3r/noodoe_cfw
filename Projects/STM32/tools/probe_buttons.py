"""Capture button diagnostics; optionally test the real EXTI dispatch by SWIER.

This performs no flash/option writes. A matching APP hash is required before RAM
symbols are interpreted. Software EXTI validates ISR routing, not physical presses.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import re
import time
from bringup import Board, APP, check_snapshot, require, sha


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--serial', default='STLINK_SERIAL_REQUIRED')
    parser.add_argument('--trigger-irq', action='store_true')
    args = parser.parse_args()
    folder = args.output.resolve()
    folder.mkdir(parents=True, exist_ok=False)
    manifest = json.loads(args.manifest.read_text(encoding='utf-8'))
    require(manifest['status'] == 'valid' and int(manifest['flash_address'], 0) == APP,
            'Expected validated APP manifest')
    size = manifest['binary_size']
    require(8 <= size <= 0x70000, 'APP size outside permitted range')
    board = Board(args.serial, folder)
    result = {'result': 'incomplete', 'physical_input_confirmation': False,
              'software_exti_requested': args.trigger_irq}
    try:
        board.prepare()
        current = board.dump('app-identity', APP, size)
        require(sha(current) == manifest['binary_sha256'], 'Device APP does not match symbol manifest')
        first = board.snapshot('before', manifest['symbols'], lcd_test=True)
        check_snapshot(first)
        if args.trigger_irq:
            # Require an idle software trigger register: do not alter pending
            # software requests belonging to another peripheral/EXTI owner.
            text = board.command('swier-before', '-r32', '0x40013C10', '4')
            words = re.findall(r'0x40013C10\s*:\s*([0-9A-Fa-f]{8})', text, re.I)
            require(len(words) == 1 and int(words[0], 16) == 0, 'EXTI SWIER is not idle')
            # The HAL ISR clears PR/SWIER immediately. Halt during the CLI's
            # write/readback so it cannot misreport that self-clearing value as
            # a download failure, then resume to service the pending interrupts.
            board.command('software-exti', '-halt', '-w32', '0x40013C10', '0x9040', '-run')
        time.sleep(0.1)
        second = board.snapshot('after', manifest['symbols'], lcd_test=True)
        check_snapshot(second)
        delta = {name: second['buttons'][f'irq_{name}'] - first['buttons'][f'irq_{name}']
                 for name in ('up', 'down', 'enter')}
        if args.trigger_irq:
            require(all(value >= 1 for value in delta.values()), f'Missing EXTI callback: {delta}')
        require(second['buttons']['process_count'] > first['buttons']['process_count'],
                'Button task service stopped')
        result.update(result='pass', irq_delta=delta, before=first, after=second)
        print(json.dumps({'result': 'pass', 'irq_delta': delta,
                          'physical_input_confirmation': False}), flush=True)
        return 0
    except Exception as error:
        result.update(result='fail', error=str(error))
        print(f'FAIL: {error}', flush=True)
        return 1
    finally:
        if hasattr(board, 'freeze_before'):
            board.command('restore-debug-freeze', '-w32', '0xE0042008', hex(board.freeze_before), '-run')
        (folder / 'result.json').write_text(json.dumps(result, indent=2), encoding='utf-8')


if __name__ == '__main__':
    raise SystemExit(main())
