"""Inspect LVGL/EVE diagnostics with an explicit, opt-in hardware execution mode.

기본 동작은 manifest 주소/오프셋/실행 계획 출력뿐이다. --execute에서만 SWD를
열며 flash/option/APP RAM 쓰기나 reset은 없다. 일관된 RAM 관측을 위해 CPU
halt/run과 DBGMCU watchdog freeze 임시 변경은 수행하므로 완전한 무간섭 측정은
아니다. 화면 품질/실제 버튼 조작은 이 도구가 증명하지 않는다.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import re
import struct
import sys
import time
from bringup import Board, APP, FLASH, FLASH_SIZE, PRESERVED, RESEARCH, STOCK_SHA256
from bringup import check_snapshot, require, sha
from verified_flash import read_expected
from graphics_checks import graphics_symbol_layout, check_graphics_snapshot, check_graphics_progress


def load_plan(manifest_path: Path, samples: int, interval: float,
              require_case_advance: bool, require_all_cases: bool) -> tuple[dict, dict]:
    """장치 접속 전에 이미지 범위와 두 C 구조체의 실제 ELF 계약을 확인한다."""
    manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
    require(manifest.get('schema_version') == 1 and manifest.get('status') == 'valid',
            'Expected a valid version-1 ELF manifest')
    require(int(manifest['flash_address'], 0) == APP and
            int(manifest['flash_limit_exclusive'], 0) == FLASH + FLASH_SIZE,
            'Manifest is not an APP-only image')
    require(isinstance(manifest['binary_size'], int) and 8 <= manifest['binary_size'] <= 0x70000,
            'APP size outside allowed flash range')
    require(re.fullmatch(r'[0-9a-f]{64}', manifest['binary_sha256']), 'Invalid APP SHA-256')
    require(2 <= samples <= 100 and 0.1 <= interval <= 60, 'Use 2..100 samples, interval 0.1..60 seconds')
    if require_case_advance:
        require(interval > 8, '--require-case-advance needs interval > 8 seconds for the default UI period')
    layout = graphics_symbol_layout(manifest['symbols'])
    plan = dict(mode='plan', hardware_access=False, flash_writes=False, option_writes=False,
                app_ram_writes=False, reset=False, frequency_khz=100,
                debug_effects=['temporary DBGMCU_APB1_FZ watchdog freeze bits 0x1800',
                               'CPU halt/read/run for each sample; original freeze value restored'],
                manifest=str(manifest_path.resolve()), app_address=f'0x{APP:08X}',
                app_size=manifest['binary_size'], app_sha256=manifest['binary_sha256'],
                graphics_layout=layout, samples=samples, interval_seconds=interval,
                require_case_advance=require_case_advance, require_all_cases=require_all_cases,
                device_preconditions=['ID 0x419, 512 KiB, RDP0',
                                      'APP hash matches manifest before interpreting RAM',
                                      'lower 64 KiB matches immutable original; installer metadata idle'],
                atomic_sample='halt; read all diagnostics; reread g_graphics; resume; accept equal even sequence and render_busy=0',
                visual_confirmation=False)
    return manifest, plan


def case_coverage(samples: list[dict]) -> dict:
    """렌더 완료와 명시적인 미지원 상태를 분리하여 관측 범위를 계산한다.

    SKIPPED는 구현 성공이 아니다. 전체 순회 검사는 RENDERED 또는 명시적
    SKIPPED로 관측한 페이지의 합집합만 요구한다. RUNNING을 방문 성공으로
    승격하지 않으며 각 페이지에서 실제 읽은 state 숫자도 별도로 남긴다.
    """
    require(bool(samples), 'No graphics case samples to summarize')
    count = samples[0]['graphics_test']['case_count']
    rendered, skipped, states = set(), set(), {}
    persistent_used = False
    for sample in samples:
        test = sample['graphics_test']
        case, state = test['current_case'], test['state']
        require(test['case_count'] == count and 0 <= case < count, 'Changing/invalid graphics case table')
        if 'graphics_test_results' in sample:
            # 현재 case를 잠깐 못 봐도 누적 rendered_visits가 실제 제출 사실을
            # 보존한다. 재방문으로 state가 RUNNING이 되어도 이전 완료는 남는다.
            # state/카운터 갱신 사이에 halt한 순간에는 완료를 추측하지 않는다.
            persistent_used = True
            records = sample['graphics_test_results']
            require(len(records) == count and count <= 128, 'Invalid persistent case count')
            for index, record in enumerate(records):
                require(record['id'] == index and record['state'] in (0, 1, 2, 3) and
                        record['allocation_failures'] == 0, f'Invalid/failed persistent case {index}')
                states.setdefault(index, set()).add(record['state'])
                if record['rendered_visits'] > 0 and record['frames'] > 0:
                    rendered.add(index)
                if record['state'] == 3 and record['visits'] > 0:
                    skipped.add(index)
            continue
        states.setdefault(case, set()).add(state)
        if state == 2: rendered.add(case)
        elif state == 3: skipped.add(case)
    return dict(case_count=count, observed_rendered_cases=sorted(rendered),
                observed_skipped_cases=sorted(skipped),
                observed_case_states={str(case): sorted(values) for case, values in sorted(states.items())},
                all_cases_observed=(rendered | skipped) == set(range(count)),
                persistent_result_table_used=persistent_used,
                skipped_is_implementation_pass=False)


def check_runtime_progress(previous: dict, current: dict) -> dict:
    """실제 halt 사이의 각 구간에 기존 ±2 tick 허용치를 적용한다.

    HAL/TIM6와 RTOS/SysTick은 debugger 정지 동안 같은 방식으로 진행하지
    않는다. 또한 runtime 구조체는 graphics sequence로 보호되지 않는다.
    따라서 이것은 태스크/시계의 대략적인 진척 검사이며 무간섭 시간 정확도
    검사가 아니다. 재시도 횟수만으로 큰 오차를 허용하지 않고, 중간에 읽은
    모든 runtime을 검사한다. 한 구간의 정지/역행/큰 발산은 다른 구간에서
    상쇄되어도 실패한다. 일반 bringup.py의 두 표본 기준은 바꾸지 않는다.
    """
    fields = ('heartbeat', 'hal_tick', 'kernel_tick', 'tim6_interrupts')
    attempts = current.get('capture_attempts', 1)
    require(type(attempts) is int and 1 <= attempts <= 20, 'Invalid runtime capture count')
    trace = current.get('capture_trace')
    if trace is None:
        # 이전 단일 표본 형식은 기존 기준 그대로 검사할 수 있다. 여러 halt를
        # 주장하면서 중간 표본이 없으면 allowance를 추측하지 않고 중단한다.
        require(attempts == 1, 'Missing runtime trace for repeated captures')
        trace = [dict(evidence=current.get('evidence_name', 'single-capture'),
                      runtime=current['runtime'])]
    require(isinstance(trace, list) and len(trace) == attempts, 'Runtime trace/count mismatch')
    require(trace[-1]['runtime'] == current['runtime'], 'Runtime trace does not end at accepted sample')

    def delta_between(before: dict, after: dict) -> dict:
        for state in (before, after):
            require(all(type(state[field]) is int and 0 <= state[field] <= 0xFFFFFFFF
                        for field in fields), 'Invalid runtime counter')
        delta = {field: (after[field] - before[field]) & 0xFFFFFFFF for field in fields}
        require(all(0 < value < 0x80000000 for value in delta.values()), f'Task/tick stopped: {delta}')
        return delta

    intervals = []
    before = previous['runtime']
    for capture in trace:
        delta = delta_between(before, capture['runtime'])
        skew = delta['hal_tick'] - delta['kernel_tick']
        require(abs(skew) <= 2,
                f'HAL/kernel ticks diverged in {capture["evidence"]}: {delta}')
        intervals.append(dict(evidence=capture['evidence'], delta=delta, hal_kernel_skew=skew))
        before = capture['runtime']
    total = delta_between(previous['runtime'], current['runtime'])
    return dict(delta=total, checked_halt_intervals=len(intervals), per_interval_skew_limit=2,
                hal_kernel_skew=total['hal_tick'] - total['kernel_tick'], intervals=intervals,
                timing_claim='approximate progression under intrusive halt/read/run; not clock accuracy')


def execute(args: argparse.Namespace, manifest: dict, plan: dict) -> int:
    """읽기만으로 프로그램 정체성/생존/렌더/시험 진척을 검증하고 원본을 보존한다.

    실행자는 정확한 ST-LINK serial과 새 evidence 디렉터리를 지정해야 한다.
    finally는 실패해도 자신이 변경한 debug freeze를 복원한다. 복원 실패 역시
    실패 결과로 남기며, 성공한 렌더 검사를 근거로 cleanup 오류를 숨기지 않는다.
    """
    require(args.serial and re.fullmatch(r'[A-Za-z0-9]+', args.serial), '--execute needs an exact --serial')
    require(args.output is not None, '--execute needs a new --output directory')
    folder = args.output.resolve()
    folder.mkdir(parents=True, exist_ok=False)
    board = Board(args.serial, folder, args.frequency_khz)
    result = dict(plan, mode='execute', hardware_access=True, result='incomplete', samples=[], progress=[])
    code = 1
    try:
        original = RESEARCH / 'VERY_IMPORTANT_ORIGINAL_NOODOE_BOOTLOARDER/noodoe_full_flash_dump_A.bin'
        stock = original.read_bytes()
        require(len(stock) == FLASH_SIZE and sha(stock) == STOCK_SHA256, 'Immutable original backup changed')
        board.prepare()
        preserved = read_expected(board, 'preserved-before', FLASH, stock[:PRESERVED],resume=True)
        require(preserved == stock[:PRESERVED], 'Lower 64 KiB differs from original')
        require(struct.unpack_from('<4I', preserved, 0x8004) == (0, 0, 0, 0), 'Pending installer metadata')
        # 현재 manifest 옆에 해시로 확인한 export가 있으면 실패한 대용량 USB
        # read의 page를 두 번 재관측할 수 있다. 기준 byte를 임의로 채우지 않는다.
        expected = None
        for candidate in args.manifest.parent.glob('*.bin'):
            if candidate.stat().st_size == manifest['binary_size']:
                data = candidate.read_bytes()
                if sha(data) == manifest['binary_sha256']:
                    expected = data
                    break
        image = (read_expected(board, 'app-identity', APP, expected,resume=True)
                 if expected is not None else board.dump('app-identity', APP, manifest['binary_size']))
        require(sha(image) == manifest['binary_sha256'], 'Device APP does not match symbol manifest')
        result['preserved_sha256'] = sha(preserved)
        previous = None
        for index in range(args.samples):
            if previous is not None:
                # halt 중 정지된 tick은 interval에 포함하지 않는다. SWD 덤프의
                # 벽시계 지연을 FPS 분모로 쓰지 않고 펌웨어의 active ms를 쓴다.
                time.sleep(args.interval)
            sample = board.graphics_snapshot(f'sample-{index + 1:03d}', manifest['symbols'])
            check_snapshot(sample)
            health = check_graphics_snapshot(sample)
            result['samples'].append(dict(evidence=sample['evidence_name'], health=health,
                                          graphics=sample['graphics'], graphics_test=sample['graphics_test'],
                                          runtime=sample['runtime'], capture_attempts=sample['capture_attempts'],
                                          capture_trace=sample['capture_trace']))
            if 'graphics_test_results' in sample:
                result['samples'][-1]['graphics_test_results'] = sample['graphics_test_results']
            if 'graphics_performance' in sample:
                result['samples'][-1]['graphics_performance'] = sample['graphics_performance']
            if previous is not None:
                result['progress'].append(check_graphics_progress(previous, sample, args.require_case_advance))
                result['progress'][-1]['runtime'] = check_runtime_progress(previous, sample)
            previous = sample
        result.update(case_coverage(result['samples']))
        if args.require_all_cases:
            require(result['all_cases_observed'],
                    f'Not all cases observed: rendered={result["observed_rendered_cases"]}, '
                    f'explicitly skipped={result["observed_skipped_cases"]}, total={result["case_count"]}')
        after = read_expected(board, 'preserved-after', FLASH, preserved,resume=True)
        require(after == preserved, 'Lower 64 KiB changed during observation')
        result['result'] = 'pass'
        code = 0
    except Exception as error:
        result.update(result='fail', error=str(error))
    finally:
        if hasattr(board, 'freeze_before'):
            try:
                board.command('restore-debug-freeze', '-w32', '0xE0042008', hex(board.freeze_before), '-run')
                result['debug_freeze_restored'] = True
            except Exception as error:
                result.update(result='fail', cleanup_error=str(error), debug_freeze_restored=False)
                code = 1
        (folder / 'result.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps(dict(result=result['result'], evidence=str(folder),
                          error=result.get('error'), cleanup_error=result.get('cleanup_error'),
                          observed_rendered_cases=result.get('observed_rendered_cases', []),
                          observed_skipped_cases=result.get('observed_skipped_cases', []),
                          visual_confirmation=False)), flush=True)
    return code


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--plan', action='store_true', help='Default: print offsets/plan without device access')
    mode.add_argument('--execute', action='store_true', help='Actually observe the target; never program/reset')
    parser.add_argument('--serial', help='Exact ST-LINK serial, required only with --execute')
    parser.add_argument('--frequency-khz', type=int, choices=[50,100], default=100)
    parser.add_argument('--output', type=Path, help='New evidence directory, required only with --execute')
    parser.add_argument('--samples', type=int, default=3)
    parser.add_argument('--interval', type=float, default=4.137)
    parser.add_argument('--require-case-advance', action='store_true')
    parser.add_argument('--require-all-cases', action='store_true',
                        help='Require every case observed RENDERED or explicitly SKIPPED; report them separately')
    args = parser.parse_args()
    try:
        manifest, plan = load_plan(args.manifest, args.samples, args.interval,
                                   args.require_case_advance, args.require_all_cases)
        plan['frequency_khz'] = args.frequency_khz
        if not args.execute:
            print(json.dumps(plan, indent=2))
            return 0
        return execute(args, manifest, plan)
    except Exception as error:
        print(f'FAIL: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
